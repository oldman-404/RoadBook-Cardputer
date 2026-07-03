#pragma once

#include <cstdint>

class BatteryManager {
public:
    void begin(uint32_t nowMs);
    void update(uint32_t nowMs);

    int32_t levelPercent() const;
    int16_t voltageMillivolts() const;

    bool valid() const;
    bool low() const;
    bool critical() const;

private:
    static bool isReadingValid(int32_t level, int16_t voltage);

    static constexpr uint32_t kUpdateIntervalMs = 5000;

    uint32_t lastUpdateMs_{0};
    int32_t levelPercent_{-1};
    int16_t voltageMillivolts_{0};
    bool valid_{false};
};
