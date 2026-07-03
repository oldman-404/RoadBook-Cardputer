#include "GridManager.h"

#include <cmath>

namespace {

constexpr double kPi = 3.14159265358979323846;

}  // namespace

GridTile GridManager::calculateTile(double latitude, double longitude, uint16_t gridSizeMeters) const
{
    if (latitude > kMaxLatitude) {
        latitude = kMaxLatitude;
    } else if (latitude < -kMaxLatitude) {
        latitude = -kMaxLatitude;
    }

    const double latitudeRadians = latitude * kPi / 180.0;
    const double longitudeRadians = longitude * kPi / 180.0;

    const double mercatorX = kEarthRadius * longitudeRadians;
    const double mercatorY = kEarthRadius * std::log(std::tan(kPi / 4.0 + latitudeRadians / 2.0));

    GridTile tile{};
    tile.x = static_cast<int32_t>(std::floor(mercatorX / static_cast<double>(gridSizeMeters)));
    tile.y = static_cast<int32_t>(std::floor(mercatorY / static_cast<double>(gridSizeMeters)));
    tile.sizeMeters = gridSizeMeters;
    return tile;
}

bool GridManager::sameTile(const GridTile& first, const GridTile& second) const
{
    return first.x == second.x && first.y == second.y && first.sizeMeters == second.sizeMeters;
}
