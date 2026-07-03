#include "DriveSessionManager.h"

#include <cmath>

namespace {

constexpr double kEarthRadiusMeters = 6371000.0;
constexpr double kPi = 3.14159265358979323846;
constexpr double kMinMovementMeters = 5.0;
constexpr double kMaxImpliedSpeedKmph = 250.0;
constexpr double kMinMovingSpeedKmph = 3.0;
constexpr uint32_t kMaxLocationAgeMs = 3000;
constexpr uint32_t kMaxSegmentGapMs = 30000;

double toRadians(double degrees)
{
    return degrees * kPi / 180.0;
}

}  // namespace

bool DriveSessionManager::isValidNumber(double value)
{
    return !std::isnan(value) && !std::isinf(value);
}

double DriveSessionManager::haversineMeters(double lat1, double lon1, double lat2, double lon2)
{
    const double lat1Rad = toRadians(lat1);
    const double lat2Rad = toRadians(lat2);
    const double deltaLat = toRadians(lat2 - lat1);
    const double deltaLon = toRadians(lon2 - lon1);

    const double sinHalfLat = std::sin(deltaLat / 2.0);
    const double sinHalfLon = std::sin(deltaLon / 2.0);
    const double haversine = sinHalfLat * sinHalfLat +
                             std::cos(lat1Rad) * std::cos(lat2Rad) * sinHalfLon * sinHalfLon;
    const double centralAngle = 2.0 * std::atan2(std::sqrt(haversine), std::sqrt(1.0 - haversine));
    return kEarthRadiusMeters * centralAngle;
}

void DriveSessionManager::begin(uint32_t nowMs)
{
    reset(nowMs);
}

void DriveSessionManager::acceptReferencePoint(double latitude, double longitude, uint32_t nowMs)
{
    referenceLat_ = latitude;
    referenceLon_ = longitude;
    referenceTimeMs_ = nowMs;
    hasReference_ = true;
    ++acceptedPointCount_;
}

void DriveSessionManager::update(bool locationValid, double latitude, double longitude, double currentSpeedKmph,
                                 uint32_t locationAgeMs, uint32_t nowMs, bool accumulateDistance)
{
    if (isValidNumber(currentSpeedKmph)) {
        currentSpeedKmph_ = currentSpeedKmph;
        if (accumulateDistance && !paused_ && currentSpeedKmph_ > maxSpeedKmph_) {
            maxSpeedKmph_ = currentSpeedKmph_;
        }
    }

    if (!accumulateDistance) {
        lastMovingUpdateMs_ = 0;
        return;
    }

    const bool locationFresh = locationValid && locationAgeMs <= kMaxLocationAgeMs;
    const bool isMoving = !paused_ && locationFresh && isValidNumber(currentSpeedKmph_) &&
                          currentSpeedKmph_ >= kMinMovingSpeedKmph;

    if (isMoving) {
        if (lastMovingUpdateMs_ != 0 && nowMs > lastMovingUpdateMs_) {
            movingSeconds_ += (nowMs - lastMovingUpdateMs_) / 1000U;
        }
        lastMovingUpdateMs_ = nowMs;
    } else {
        lastMovingUpdateMs_ = 0;
    }

    if (!locationFresh) {
        return;
    }

    if (!isValidNumber(latitude) || !isValidNumber(longitude)) {
        return;
    }

    started_ = true;

    if (!hasReference_) {
        acceptReferencePoint(latitude, longitude, nowMs);
        return;
    }

    if (paused_) {
        return;
    }

    const uint32_t gapMs = nowMs - referenceTimeMs_;
    if (gapMs > kMaxSegmentGapMs) {
        ++rejectedPointCount_;
        acceptReferencePoint(latitude, longitude, nowMs);
        return;
    }

    if (gapMs == 0) {
        return;
    }

    const double segmentMeters = haversineMeters(referenceLat_, referenceLon_, latitude, longitude);

    if (segmentMeters < kMinMovementMeters) {
        referenceLat_ = latitude;
        referenceLon_ = longitude;
        referenceTimeMs_ = nowMs;
        return;
    }

    const double gapHours = static_cast<double>(gapMs) / 3600000.0;
    const double impliedSpeedKmph = (segmentMeters / 1000.0) / gapHours;

    if (!isValidNumber(impliedSpeedKmph) || impliedSpeedKmph > kMaxImpliedSpeedKmph) {
        ++rejectedPointCount_;
        return;
    }

    distanceKm_ += segmentMeters / 1000.0;
    acceptReferencePoint(latitude, longitude, nowMs);
}

void DriveSessionManager::pause()
{
    paused_ = true;
    lastMovingUpdateMs_ = 0;
}

void DriveSessionManager::resume(uint32_t nowMs)
{
    paused_ = false;
    lastMovingUpdateMs_ = 0;
    (void)nowMs;
}

void DriveSessionManager::reset(uint32_t nowMs)
{
    paused_ = false;
    started_ = false;
    hasReference_ = false;

    referenceLat_ = 0.0;
    referenceLon_ = 0.0;
    referenceTimeMs_ = 0;

    sessionStartMs_ = nowMs;
    lastMovingUpdateMs_ = 0;

    distanceKm_ = 0.0;
    currentSpeedKmph_ = 0.0;
    maxSpeedKmph_ = 0.0;

    movingSeconds_ = 0;
    acceptedPointCount_ = 0;
    rejectedPointCount_ = 0;
}

void DriveSessionManager::resetReferencePoint(double latitude, double longitude, uint32_t nowMs)
{
    referenceLat_ = latitude;
    referenceLon_ = longitude;
    referenceTimeMs_ = nowMs;
    hasReference_ = true;
    lastMovingUpdateMs_ = 0;
}

bool DriveSessionManager::paused() const
{
    return paused_;
}

bool DriveSessionManager::started() const
{
    return started_;
}

double DriveSessionManager::distanceKm() const
{
    return distanceKm_;
}

double DriveSessionManager::currentSpeedKmph() const
{
    return currentSpeedKmph_;
}

double DriveSessionManager::maxSpeedKmph() const
{
    return maxSpeedKmph_;
}

double DriveSessionManager::averageMovingSpeedKmph() const
{
    if (movingSeconds_ == 0 || distanceKm_ <= 0.0) {
        return 0.0;
    }

    const double movingHours = static_cast<double>(movingSeconds_) / 3600.0;
    if (movingHours <= 0.0) {
        return 0.0;
    }

    const double average = distanceKm_ / movingHours;
    return isValidNumber(average) ? average : 0.0;
}

uint32_t DriveSessionManager::elapsedSeconds(uint32_t nowMs) const
{
    if (nowMs <= sessionStartMs_) {
        return 0;
    }
    return (nowMs - sessionStartMs_) / 1000U;
}

uint32_t DriveSessionManager::movingSeconds() const
{
    return movingSeconds_;
}

uint32_t DriveSessionManager::acceptedPointCount() const
{
    return acceptedPointCount_;
}

uint32_t DriveSessionManager::rejectedPointCount() const
{
    return rejectedPointCount_;
}
