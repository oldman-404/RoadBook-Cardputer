#pragma once

#include <cstddef>
#include <cstdint>

#include "GridManager.h"

enum class TileMarkResult {
    Error,
    AlreadyVisited,
    NewlyVisited,
};

class StorageManager {
public:
    bool begin();
    bool retry();
    void printDiagnostics();

    bool ready() const;
    uint64_t cardSizeMB() const;
    uint32_t totalTiles() const;
    uint32_t lastRetryFrequencyHz() const;

    TileMarkResult markVisited(const GridTile& tile);

private:
    void configureSpiSlavesInactive();
    void beginSharedSpiBus();
    void deselectSpiSlaves();
    bool tryMount(uint32_t frequencyHz, uint8_t& cardTypeOut);
    bool mountAndInitialize(uint32_t& frequencyOut, bool logFailedAttempts = false);
    bool ensureDirectories();
    bool runSelfTest();
    bool runDiagnosticFileTest(bool& writeOk, bool& readOk, bool& deleteOk);
    bool loadTotalTiles();
    bool saveTotalTiles();
    void resetSpiBus();

    static int32_t floorDiv(int32_t numerator, int32_t denominator);
    static int32_t positiveMod(int32_t value, int32_t modulus);
    static bool buildBucketPath(char* buffer, size_t bufferSize, int32_t bucketX, int32_t bucketY);
    static const char* cardTypeName(uint8_t cardType);
    static bool isValidCardType(uint8_t cardType);

    bool ready_{false};
    uint64_t cardSizeMB_{0};
    uint32_t totalTiles_{0};
    uint32_t lastRetryFrequencyHz_{0};
};
