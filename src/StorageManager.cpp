#include "StorageManager.h"

#include <SD.h>
#include <SPI.h>

#include <cstdio>
#include <cstring>

namespace {

constexpr int kSdCsPin = 12;
constexpr int kSdMisoPin = 39;
constexpr int kSdMosiPin = 14;
constexpr int kSdSckPin = 40;
constexpr uint32_t kSdFrequencyHz = 25000000;

constexpr int32_t kBucketTiles = 64;
constexpr size_t kBitmapBytes = 512;

constexpr const char* kRoadbookDir = "/roadbook";
constexpr const char* kTilesDir = "/roadbook/tiles";
constexpr const char* kGridDir = "/roadbook/tiles/100";
constexpr const char* kStatsPath = "/roadbook/stats_100.txt";
constexpr const char* kStatsTempPath = "/roadbook/stats_100.txt.tmp";
constexpr const char* kSelfTestPath = "/roadbook/.selftest";

}  // namespace

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

    SPI.begin(kSdSckPin, kSdMisoPin, kSdMosiPin, kSdCsPin);

    if (!SD.begin(kSdCsPin, SPI, kSdFrequencyHz)) {
        return false;
    }

    cardSizeMB_ = SD.cardSize() / (1024ULL * 1024ULL);

    if (!ensureDirectories() || !runSelfTest() || !loadTotalTiles()) {
        SD.end();
        return false;
    }

    ready_ = true;
    return true;
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
