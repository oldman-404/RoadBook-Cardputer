#pragma once

#include <cstdint>

class StorageManager;

enum class JourneyStatus {
    Disabled,
    WaitingForTime,
    Recording,
    Paused,
    StorageError,
};

class JourneyLogger {
public:
    void begin(StorageManager& storage);
    void prepareForRecording();
    void update(bool locationValid, bool dateValid, bool timeValid, double latitude, double longitude,
                double altitudeMeters, double speedKmph, uint32_t satellites, uint32_t locationAgeMs,
                uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second,
                uint32_t nowMs, bool sessionRecording, bool accurateFix);

    void pause();
    void resume();
    bool stop();
    bool flush();
    void resetPositionReference();
    bool verifyOpenFile(uint64_t& sizeBytesOut) const;

    JourneyStatus status() const;
    bool recording() const;
    bool paused() const;

    uint32_t writtenPointCount() const;
    uint32_t skippedPointCount() const;
    uint32_t writeErrorCount() const;

    const char* currentFilePath() const;

private:
    bool ensureJourneysDirectory();
    bool openNewFile(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second);
    void closeFile(bool flushFirst);
    bool writePoint(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second,
                    double latitude, double longitude, double altitudeMeters, double speedKmph, uint32_t satellites,
                    uint32_t locationAgeMs);
    void flushIfNeeded(uint32_t nowMs, bool force);
    void handleWriteError();
    bool isPointEligible(bool locationValid, double latitude, double longitude, double altitudeMeters,
                         double speedKmph, uint32_t locationAgeMs) const;
    bool shouldWritePoint(double latitude, double longitude, uint32_t nowMs) const;
    bool sameUtcSecond(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute,
                       uint8_t second) const;
    void syncStorageAvailability();
    void printPeriodicDiagnostics(uint32_t nowMs);
    const char* currentFileName() const;

    static double haversineMeters(double lat1, double lon1, double lat2, double lon2);
    static bool isFiniteNumber(double value);

    StorageManager* storage_{nullptr};
    JourneyStatus status_{JourneyStatus::Disabled};

    bool fileOpen_{false};
    char currentFilePath_[64];
    char currentFileName_[24];

    uint32_t writtenPointCount_{0};
    uint32_t skippedPointCount_{0};
    uint32_t writeErrorCount_{0};

    uint32_t lastProcessMs_{0};
    uint32_t lastWrittenMs_{0};
    uint32_t lastFlushMs_{0};
    uint32_t lastSerialDiagMs_{0};
    uint32_t pointsSinceFlush_{0};

    bool hasLastWrittenPoint_{false};
    double lastWrittenLat_{0.0};
    double lastWrittenLon_{0.0};

    uint16_t lastWrittenYear_{0};
    uint8_t lastWrittenMonth_{0};
    uint8_t lastWrittenDay_{0};
    uint8_t lastWrittenHour_{0};
    uint8_t lastWrittenMinute_{0};
    uint8_t lastWrittenSecond_{0};
};
