#include "GPSManager.h"

#include <MultipleSatellite.h>

#include <cmath>

namespace {

constexpr uint32_t kMaxAccurateLocationAgeMs = 3000;
constexpr uint32_t kMinAccurateSatellites = 4;
constexpr double kMaxAccurateHdop = 3.0;

bool isFiniteNumber(double value)
{
    return !std::isnan(value) && !std::isinf(value);
}

}  // namespace

struct GPSManager::Impl {
    MultipleSatellite gps{Serial1, 115200, SERIAL_8N1, 15, 13};
    bool receivedData{false};
};

GPSManager::GPSManager() : _impl(new Impl()) {}

GPSManager::~GPSManager()
{
    delete _impl;
}

void GPSManager::begin()
{
    _impl->gps.begin();
}

void GPSManager::update()
{
    _impl->gps.updateGPS();

    if (_impl->gps.charsProcessed() > 0) {
        _impl->receivedData = true;
    }
}

bool GPSManager::hasReceivedData() const
{
    return _impl->receivedData;
}

bool GPSManager::hasFix() const
{
    return _impl->gps.location.isValid();
}

uint32_t GPSManager::charactersProcessed() const
{
    return _impl->gps.charsProcessed();
}

uint32_t GPSManager::satellites() const
{
    return _impl->gps.satellites.value();
}

double GPSManager::latitude() const
{
    return _impl->gps.location.lat();
}

double GPSManager::longitude() const
{
    return _impl->gps.location.lng();
}

double GPSManager::speedKmph() const
{
    return _impl->gps.speed.kmph();
}

double GPSManager::altitudeMeters() const
{
    return _impl->gps.altitude.meters();
}

bool GPSManager::dateValid() const
{
    return _impl->gps.date.isValid();
}

bool GPSManager::timeValid() const
{
    return _impl->gps.time.isValid();
}

uint16_t GPSManager::year() const
{
    return _impl->gps.date.year();
}

uint8_t GPSManager::month() const
{
    return _impl->gps.date.month();
}

uint8_t GPSManager::day() const
{
    return _impl->gps.date.day();
}

uint8_t GPSManager::hour() const
{
    return _impl->gps.time.hour();
}

uint8_t GPSManager::minute() const
{
    return _impl->gps.time.minute();
}

uint8_t GPSManager::second() const
{
    return _impl->gps.time.second();
}

uint32_t GPSManager::locationAgeMs() const
{
    if (!_impl->gps.location.isValid()) {
        return UINT32_MAX;
    }
    return _impl->gps.location.age();
}

bool GPSManager::speedValid() const
{
    return _impl->gps.speed.isValid();
}

bool GPSManager::hdopValid() const
{
    return _impl->gps.hdop.isValid();
}

double GPSManager::hdop() const
{
    if (!_impl->gps.hdop.isValid()) {
        return 0.0;
    }
    return _impl->gps.hdop.hdop();
}

bool GPSManager::hasAccurateFix() const
{
    if (!_impl->gps.location.isValid()) {
        return false;
    }

    const uint32_t ageMs = locationAgeMs();
    if (ageMs > kMaxAccurateLocationAgeMs) {
        return false;
    }

    if (satellites() < kMinAccurateSatellites) {
        return false;
    }

    if (!_impl->gps.hdop.isValid()) {
        return false;
    }

    const double hdopValue = _impl->gps.hdop.hdop();
    if (!isFiniteNumber(hdopValue) || hdopValue > kMaxAccurateHdop) {
        return false;
    }

    const double lat = latitude();
    const double lon = longitude();
    return isFiniteNumber(lat) && isFiniteNumber(lon);
}
