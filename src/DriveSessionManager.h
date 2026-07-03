#pragma once

#include <cstdint>

class DriveSessionManager {
public:
    void begin(uint32_t nowMs);

    void update(bool locationValid, double latitude, double longitude, double currentSpeedKmph, uint32_t locationAgeMs,
                uint32_t nowMs, bool accumulateDistance);

    void pause();
    void resume(uint32_t nowMs);
    void reset(uint32_t nowMs);
    void resetReferencePoint(double latitude, double longitude, uint32_t nowMs);

    bool paused() const;
    bool started() const;

    double distanceKm() const;
    double currentSpeedKmph() const;
    double maxSpeedKmph() const;
    double averageMovingSpeedKmph() const;

    uint32_t elapsedSeconds(uint32_t nowMs) const;
    uint32_t movingSeconds() const;
    uint32_t acceptedPointCount() const;
    uint32_t rejectedPointCount() const;

private:
    static double haversineMeters(double lat1, double lon1, double lat2, double lon2);
    static bool isValidNumber(double value);
    void acceptReferencePoint(double latitude, double longitude, uint32_t nowMs);

    bool paused_{false};
    bool started_{false};
    bool hasReference_{false};

    double referenceLat_{0.0};
    double referenceLon_{0.0};
    uint32_t referenceTimeMs_{0};

    uint32_t sessionStartMs_{0};
    uint32_t lastMovingUpdateMs_{0};

    double distanceKm_{0.0};
    double currentSpeedKmph_{0.0};
    double maxSpeedKmph_{0.0};

    uint32_t movingSeconds_{0};
    uint32_t acceptedPointCount_{0};
    uint32_t rejectedPointCount_{0};
};
