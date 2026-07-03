#include "StorageManager.h"

#include <SD.h>
#include <SPI.h>

#include <Arduino.h>

#include <cstdio>
#include <cstring>

namespace {

constexpr int kSdCsPin = 12;
constexpr int kSdMisoPin = 39;
constexpr int kSdMosiPin = 14;
constexpr int kSdSckPin = 40;

constexpr int kLoraNssPin = 5;
constexpr int kLoraResetPin = 3;
constexpr int kLoraBusyPin = 6;
constexpr int kLoraIrqPin = 4;

constexpr uint32_t kMountFrequenciesHz[] = {25000000, 10000000, 4000000};
constexpr size_t kMountFrequencyCount = sizeof(kMountFrequenciesHz) / sizeof(kMountFrequenciesHz[0]);

constexpr uint32_t kSpiSlaveSettleMs = 10;

constexpr int32_t kBucketTiles = 64;
constexpr size_t kBitmapBytes = 512;

constexpr const char* kRoadbookDir = "/roadbook";
constexpr const char* kTilesDir = "/roadbook/tiles";
constexpr const char* kGridDir = "/roadbook/tiles/100";
constexpr const char* kStatsPath = "/roadbook/stats_100.txt";
constexpr const char* kStatsTempPath = "/roadbook/stats_100.txt.tmp";
constexpr const char* kSelfTestPath = "/roadbook/.selftest";
constexpr const char* kDiagnosticTestPath = "/roadbook/.sd_test.tmp";

}  // namespace

const char* StorageManager::cardTypeName(uint8_t cardType)
{
    switch (cardType) {
        case CARD_NONE:
            return "NONE";
        case CARD_MMC:
            return "MMC";
        case CARD_SD:
            return "SDSC";
        case CARD_SDHC:
            return "SDHC";
        default:
            return "UNKNOWN";
    }
}

bool StorageManager::isValidCardType(uint8_t cardType)
{
    return cardType != CARD_NONE;
}

void StorageManager::configureSpiSlavesInactive()
{
    pinMode(kSdCsPin, OUTPUT);
    digitalWrite(kSdCsPin, HIGH);

    pinMode(kLoraNssPin, OUTPUT);
    digitalWrite(kLoraNssPin, HIGH);

    pinMode(kLoraResetPin, OUTPUT);
    digitalWrite(kLoraResetPin, HIGH);

    pinMode(kLoraBusyPin, INPUT);
    pinMode(kLoraIrqPin, INPUT);

    delay(kSpiSlaveSettleMs);
}

void StorageManager::beginSharedSpiBus()
{
    SPI.end();
    SPI.begin(kSdSckPin, kSdMisoPin, kSdMosiPin);
}

void StorageManager::deselectSpiSlaves()
{
    digitalWrite(kLoraNssPin, HIGH);
    digitalWrite(kSdCsPin, HIGH);
}

void StorageManager::resetSpiBus()
{
    SD.end();
    configureSpiSlavesInactive();
    beginSharedSpiBus();
}

bool StorageManager::tryMount(uint32_t frequencyHz, uint8_t& cardTypeOut)
{
    cardTypeOut = CARD_NONE;

    deselectSpiSlaves();

    if (!SD.begin(kSdCsPin, SPI, frequencyHz)) {
        return false;
    }

    cardTypeOut = SD.cardType();
    if (!isValidCardType(cardTypeOut)) {
        SD.end();
        deselectSpiSlaves();
        cardTypeOut = CARD_NONE;
        return false;
    }

    digitalWrite(kLoraNssPin, HIGH);
    return true;
}

bool StorageManager::mountAndInitialize(uint32_t& frequencyOut, bool logFailedAttempts)
{
    frequencyOut = 0;

    for (size_t index = 0; index < kMountFrequencyCount; ++index) {
        uint8_t cardType = CARD_NONE;
        const uint32_t frequency = kMountFrequenciesHz[index];

        if (!tryMount(frequency, cardType)) {
            if (logFailedAttempts) {
                Serial.printf("SD frequency=%u mount=FAIL\n", frequency);
            }
            continue;
        }

        cardSizeMB_ = SD.cardSize() / (1024ULL * 1024ULL);

        if (ensureDirectories() && runSelfTest() && loadTotalTiles()) {
            frequencyOut = frequency;
            digitalWrite(kLoraNssPin, HIGH);
            return true;
        }

        SD.end();
        deselectSpiSlaves();

        if (logFailedAttempts) {
            Serial.printf("SD frequency=%u mount=OK init=FAIL\n", frequency);
        }
    }

    cardSizeMB_ = 0;
    return false;
}

int32_t StorageManager::floorDiv(int32_t numerator, int32_t denominator)
{
    const int32_t quotient = numerator / denominator;
    const int32_t remainder = numerator % denominator;
    if (remainder != 0 && ((remainder > 0) != (denominator > 0))) {
        return quotient - 1;
    }
    return quotient;
}

int32_t StorageManager::positiveMod(int32_t value, int32_t modulus)
{
    const int32_t remainder = value % modulus;
    return remainder < 0 ? remainder + modulus : remainder;
}

bool StorageManager::buildBucketPath(char* buffer, size_t bufferSize, int32_t bucketX, int32_t bucketY)
{
    return std::snprintf(buffer, bufferSize, "%s/%ld_%ld.bin", kGridDir, static_cast<long>(bucketX),
                         static_cast<long>(bucketY)) > 0;
}

bool StorageManager::begin()
{
    ready_ = false;
    cardSizeMB_ = 0;
    totalTiles_ = 0;
    lastRetryFrequencyHz_ = 0;

    configureSpiSlavesInactive();

    Serial.println("SPI INIT");
    Serial.println("SD CS=HIGH");
    Serial.println("LORA NSS=HIGH");
    Serial.println("LORA RESET=HIGH");

    beginSharedSpiBus();

    uint32_t frequencyHz = 0;
    const bool mounted = mountAndInitialize(frequencyHz, true);

    Serial.printf("SD frequency=%u\n", frequencyHz);
    Serial.printf("SD mount=%s\n", mounted ? "OK" : "FAIL");

    if (!mounted) {
        SD.end();
        deselectSpiSlaves();
        return false;
    }

    Serial.println("SPI coexistence: SD mounted, LoRa inactive");
    lastRetryFrequencyHz_ = frequencyHz;
    ready_ = true;
    return true;
}

bool StorageManager::retry()
{
    ready_ = false;
    cardSizeMB_ = 0;
    totalTiles_ = 0;
    lastRetryFrequencyHz_ = 0;

    resetSpiBus();

    uint32_t frequencyHz = 0;
    if (!mountAndInitialize(frequencyHz, true)) {
        SD.end();
        deselectSpiSlaves();
        return false;
    }

    digitalWrite(kLoraNssPin, HIGH);
    lastRetryFrequencyHz_ = frequencyHz;
    ready_ = true;
    return true;
}

void StorageManager::printDiagnostics()
{
    const bool wasReady = ready_;

    Serial.println("SD DIAG begin");
    Serial.println("pins SCK=40 MISO=39 MOSI=14 CS=12");

    resetSpiBus();

    bool mountOk = false;
    uint32_t frequencyHz = 0;
    uint8_t cardType = CARD_NONE;

    for (size_t index = 0; index < kMountFrequencyCount; ++index) {
        const uint32_t candidateFrequency = kMountFrequenciesHz[index];
        if (tryMount(candidateFrequency, cardType)) {
            mountOk = true;
            frequencyHz = candidateFrequency;
            break;
        }
    }

    Serial.printf("retry frequency=%u\n", frequencyHz);
    Serial.printf("mount=%s\n", mountOk ? "OK" : "FAIL");
    Serial.printf("cardType=%s\n", cardTypeName(cardType));

    if (mountOk) {
        const uint64_t cardSizeBytes = SD.cardSize();
        const uint64_t totalBytes = SD.totalBytes();
        const uint64_t usedBytes = SD.usedBytes();

        Serial.printf("cardSizeMB=%llu\n", cardSizeBytes / (1024ULL * 1024ULL));
        Serial.printf("totalMB=%llu\n", totalBytes / (1024ULL * 1024ULL));
        Serial.printf("usedMB=%llu\n", usedBytes / (1024ULL * 1024ULL));

        const bool mkdirOk = ensureDirectories();
        Serial.printf("mkdir /roadbook=%s\n", mkdirOk ? "OK" : "FAIL");

        bool writeOk = false;
        bool readOk = false;
        bool deleteOk = false;
        if (mkdirOk) {
            runDiagnosticFileTest(writeOk, readOk, deleteOk);
        }

        Serial.printf("write test=%s\n", writeOk ? "OK" : "FAIL");
        Serial.printf("read test=%s\n", readOk ? "OK" : "FAIL");
        Serial.printf("delete test=%s\n", deleteOk ? "OK" : "FAIL");
    } else {
        Serial.println("cardSizeMB=--");
        Serial.println("totalMB=--");
        Serial.println("usedMB=--");
        Serial.println("mkdir /roadbook=FAIL");
        Serial.println("write test=FAIL");
        Serial.println("read test=FAIL");
        Serial.println("delete test=FAIL");
    }

    Serial.println("SD DIAG end");

    SD.end();
    deselectSpiSlaves();

    if (wasReady) {
        retry();
    } else {
        ready_ = false;
        cardSizeMB_ = 0;
        totalTiles_ = 0;
        lastRetryFrequencyHz_ = 0;
    }
}

bool StorageManager::ready() const
{
    return ready_;
}

uint64_t StorageManager::cardSizeMB() const
{
    return cardSizeMB_;
}

uint32_t StorageManager::totalTiles() const
{
    return totalTiles_;
}

uint32_t StorageManager::lastRetryFrequencyHz() const
{
    return lastRetryFrequencyHz_;
}

bool StorageManager::ensureDirectories()
{
    if (!SD.exists(kRoadbookDir) && !SD.mkdir(kRoadbookDir)) {
        return false;
    }
    if (!SD.exists(kTilesDir) && !SD.mkdir(kTilesDir)) {
        return false;
    }
    if (!SD.exists(kGridDir) && !SD.mkdir(kGridDir)) {
        return false;
    }
    return true;
}

bool StorageManager::runSelfTest()
{
    static constexpr const char kPayload[] = "roadbook";

    File writeFile = SD.open(kSelfTestPath, FILE_WRITE);
    if (!writeFile) {
        return false;
    }
    if (writeFile.write(reinterpret_cast<const uint8_t*>(kPayload), sizeof(kPayload) - 1) != sizeof(kPayload) - 1) {
        writeFile.close();
        SD.remove(kSelfTestPath);
        return false;
    }
    writeFile.close();

    File readFile = SD.open(kSelfTestPath, FILE_READ);
    if (!readFile) {
        SD.remove(kSelfTestPath);
        return false;
    }

    char buffer[16] = {};
    const size_t bytesRead = readFile.read(reinterpret_cast<uint8_t*>(buffer), sizeof(kPayload) - 1);
    readFile.close();
    SD.remove(kSelfTestPath);

    return bytesRead == (sizeof(kPayload) - 1) && std::strncmp(buffer, kPayload, sizeof(kPayload) - 1) == 0;
}

bool StorageManager::runDiagnosticFileTest(bool& writeOk, bool& readOk, bool& deleteOk)
{
    static constexpr const char kPayload[] = "roadbook-sd-test";

    writeOk = false;
    readOk = false;
    deleteOk = false;

    if (SD.exists(kDiagnosticTestPath)) {
        SD.remove(kDiagnosticTestPath);
    }

    File writeFile = SD.open(kDiagnosticTestPath, FILE_WRITE);
    if (!writeFile) {
        return false;
    }

    if (writeFile.write(reinterpret_cast<const uint8_t*>(kPayload), sizeof(kPayload) - 1) == sizeof(kPayload) - 1) {
        writeOk = true;
    }
    writeFile.close();

    if (!writeOk) {
        SD.remove(kDiagnosticTestPath);
        return false;
    }

    File readFile = SD.open(kDiagnosticTestPath, FILE_READ);
    if (!readFile) {
        SD.remove(kDiagnosticTestPath);
        return false;
    }

    char buffer[24] = {};
    const size_t bytesRead = readFile.read(reinterpret_cast<uint8_t*>(buffer), sizeof(kPayload) - 1);
    readFile.close();

    if (bytesRead == (sizeof(kPayload) - 1) && std::strncmp(buffer, kPayload, sizeof(kPayload) - 1) == 0) {
        readOk = true;
    }

    deleteOk = SD.remove(kDiagnosticTestPath);
    return writeOk && readOk && deleteOk;
}

bool StorageManager::loadTotalTiles()
{
    totalTiles_ = 0;

    if (!SD.exists(kStatsPath)) {
        return true;
    }

    File statsFile = SD.open(kStatsPath, FILE_READ);
    if (!statsFile) {
        return false;
    }

    char line[24] = {};
    const size_t bytesRead = statsFile.read(reinterpret_cast<uint8_t*>(line), sizeof(line) - 1);
    statsFile.close();

    if (bytesRead == 0) {
        return true;
    }

    unsigned long parsedTotal = 0;
    if (std::sscanf(line, "%lu", &parsedTotal) != 1) {
        return false;
    }

    totalTiles_ = static_cast<uint32_t>(parsedTotal);
    return true;
}

bool StorageManager::saveTotalTiles()
{
    File tempFile = SD.open(kStatsTempPath, FILE_WRITE);
    if (!tempFile) {
        return false;
    }

    char line[24];
    std::snprintf(line, sizeof(line), "%u\n", totalTiles_);
    if (tempFile.print(line) != static_cast<int>(std::strlen(line))) {
        tempFile.close();
        SD.remove(kStatsTempPath);
        return false;
    }
    tempFile.close();

    if (SD.exists(kStatsPath) && !SD.remove(kStatsPath)) {
        SD.remove(kStatsTempPath);
        return false;
    }

    if (!SD.rename(kStatsTempPath, kStatsPath)) {
        SD.remove(kStatsTempPath);
        return false;
    }

    return true;
}

TileMarkResult StorageManager::markVisited(const GridTile& tile)
{
    if (!ready_ || tile.sizeMeters != 100) {
        return TileMarkResult::Error;
    }

    const int32_t bucketX = floorDiv(tile.x, kBucketTiles);
    const int32_t bucketY = floorDiv(tile.y, kBucketTiles);
    const int32_t localX = positiveMod(tile.x, kBucketTiles);
    const int32_t localY = positiveMod(tile.y, kBucketTiles);
    const uint16_t bitIndex = static_cast<uint16_t>(localY * kBucketTiles + localX);

    char bucketPath[64];
    if (!buildBucketPath(bucketPath, sizeof(bucketPath), bucketX, bucketY)) {
        return TileMarkResult::Error;
    }

    uint8_t bitmap[kBitmapBytes] = {};

    if (SD.exists(bucketPath)) {
        File readFile = SD.open(bucketPath, FILE_READ);
        if (!readFile) {
            return TileMarkResult::Error;
        }
        if (readFile.read(bitmap, kBitmapBytes) != kBitmapBytes) {
            readFile.close();
            return TileMarkResult::Error;
        }
        readFile.close();
    }

    const uint16_t byteIndex = bitIndex / 8;
    const uint8_t bitMask = static_cast<uint8_t>(1U << (bitIndex % 8));
    if ((bitmap[byteIndex] & bitMask) != 0) {
        return TileMarkResult::AlreadyVisited;
    }

    bitmap[byteIndex] |= bitMask;

    File writeFile = SD.open(bucketPath, FILE_WRITE);
    if (!writeFile) {
        return TileMarkResult::Error;
    }
    if (writeFile.write(bitmap, kBitmapBytes) != kBitmapBytes) {
        writeFile.close();
        return TileMarkResult::Error;
    }
    writeFile.close();

    ++totalTiles_;
    if (!saveTotalTiles()) {
        --totalTiles_;
        return TileMarkResult::Error;
    }

    return TileMarkResult::NewlyVisited;
}
