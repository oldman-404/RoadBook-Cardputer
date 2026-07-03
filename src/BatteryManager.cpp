#include "BatteryManager.h"

#include <M5Unified.h>

bool BatteryManager::isReadingValid(int32_t level, int16_t voltage)
{
    if (level < 0 || level > 100) {
        return false;
    }
    if (voltage <= 0) {
        return false;
    }
    return true;
}

void BatteryManager::begin(uint32_t nowMs)
{
    lastUpdateMs_ = 0;
    levelPercent_ = -1;
    voltageMillivolts_ = 0;
    valid_ = false;
    update(nowMs);
}

void BatteryManager::update(uint32_t nowMs)
{
    if (lastUpdateMs_ != 0 && (nowMs - lastUpdateMs_) < kUpdateIntervalMs) {
        return;
    }

    lastUpdateMs_ = nowMs;

    const int32_t rawLevel = M5.Power.getBatteryLevel();
    const int16_t rawVoltage = M5.Power.getBatteryVoltage();

    if (!isReadingValid(rawLevel, rawVoltage)) {
        return;
    }

    int32_t clampedLevel = rawLevel;
    if (clampedLevel > 100) {
        clampedLevel = 100;
    }
    if (clampedLevel < 0) {
        clampedLevel = 0;
    }

    levelPercent_ = clampedLevel;
    voltageMillivolts_ = rawVoltage;
    valid_ = true;
}

int32_t BatteryManager::levelPercent() const
{
    return levelPercent_;
}

int16_t BatteryManager::voltageMillivolts() const
{
    return voltageMillivolts_;
}

bool BatteryManager::valid() const
{
    return valid_;
}

bool BatteryManager::low() const
{
    return valid_ && levelPercent_ <= 20;
}

bool BatteryManager::critical() const
{
    return valid_ && levelPercent_ <= 10;
}
