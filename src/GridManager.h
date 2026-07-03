#pragma once

#include <cstdint>

struct GridTile {
    int32_t x;
    int32_t y;
    uint16_t sizeMeters;
};

class GridManager {
public:
    GridTile calculateTile(double latitude, double longitude, uint16_t gridSizeMeters = 100) const;
    bool sameTile(const GridTile& first, const GridTile& second) const;

private:
    static constexpr double kEarthRadius = 6378137.0;
    static constexpr double kMaxLatitude = 85.05112878;
};
