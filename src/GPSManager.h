#pragma once

#include <cstdint>

class GPSManager {
public:
    GPSManager();
    ~GPSManager();

    GPSManager(const GPSManager&) = delete;
    GPSManager& operator=(const GPSManager&) = delete;

    void begin();
    void update();

    bool hasReceivedData() const;
    bool hasFix() const;

    uint32_t charactersProcessed() const;
    uint32_t satellites() const;

    double latitude() const;
    double longitude() const;
    double speedKmph() const;
    double altitudeMeters() const;

    bool dateValid() const;
    bool timeValid() const;

    uint16_t year() const;
    uint8_t month() const;
    uint8_t day() const;

    uint8_t hour() const;
    uint8_t minute() const;
    uint8_t second() const;

private:
    struct Impl;
    Impl* _impl;
};
