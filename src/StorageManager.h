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
    bool ready() const;
    uint64_t cardSizeMB() const;
    uint32_t totalTiles() const;

    TileMarkResult markVisited(const GridTile& tile);

private:
    bool ensureDirectories();
    bool runSelfTest();
    bool loadTotalTiles();
    bool saveTotalTiles();

    static int32_t floorDiv(int32_t numerator, int32_t denominator);
    static int32_t positiveMod(int32_t value, int32_t modulus);
    static bool buildBucketPath(char* buffer, size_t bufferSize, int32_t bucketX, int32_t bucketY);

    bool ready_{false};
    uint64_t cardSizeMB_{0};
    uint32_t totalTiles_{0};
};
