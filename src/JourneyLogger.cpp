#include "JourneyLogger.h"

#include "StorageManager.h"

#include <SD.h>

#include <cmath>
#include <cstdio>
#include <cstring>

namespace {

constexpr const char* kJourneysDir = "/roadbook/journeys";
constexpr const char* kCsvHeader =
    "utc_date,utc_time,latitude,longitude,altitude_m,speed_kmh,satellites,location_age_ms\n";

constexpr double kEarthRadiusMeters = 6371000.0;
constexpr double kPi = 3.14159265358979323846;
constexpr double kMinWriteDistanceMeters = 10.0;
constexpr double kMaxSpeedKmph = 250.0;
constexpr uint32_t kMaxLocationAgeMs = 3000;
constexpr uint32_t kMinWriteIntervalMs = 5000;
constexpr uint32_t kMaxProcessIntervalMs = 1000;
constexpr uint32_t kFlushEveryPoints = 10;
constexpr uint32_t kFlushIntervalMs = 15000;
constexpr uint32_t kSerialDiagIntervalMs = 30000;

File gActiveJourneyFile;

double toRadians(double degrees)
{
    return degrees * kPi / 180.0;
}

}  // namespace

bool JourneyLogger::isFiniteNumber(double value)
{
    return !std::isnan(value) && !std::isinf(value);
}

double JourneyLogger::haversineMeters(double lat1, double lon1, double lat2, double lon2)
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

void JourneyLogger::begin(StorageManager& storage)
{
    closeFile(true);
    gActiveJourneyFile = File();

    storage_ = &storage;
    currentFilePath_[0] = '\0';
    currentFileName_[0] = '\0';
    writtenPointCount_ = 0;
    skippedPointCount_ = 0;
    writeErrorCount_ = 0;
    lastProcessMs_ = 0;
    lastWrittenMs_ = 0;
    lastFlushMs_ = 0;
    lastSerialDiagMs_ = 0;
    pointsSinceFlush_ = 0;
    hasLastWrittenPoint_ = false;
    fileOpen_ = false;

    if (storage_ != nullptr && storage_->ready()) {
        status_ = JourneyStatus::Disabled;
    } else {
        status_ = JourneyStatus::Disabled;
    }
}

void JourneyLogger::prepareForRecording()
{
    closeFile(true);
    gActiveJourneyFile = File();

    currentFilePath_[0] = '\0';
    currentFileName_[0] = '\0';
    writtenPointCount_ = 0;
    skippedPointCount_ = 0;
    writeErrorCount_ = 0;
    lastProcessMs_ = 0;
    lastWrittenMs_ = 0;
    lastFlushMs_ = 0;
    lastSerialDiagMs_ = 0;
    pointsSinceFlush_ = 0;
    hasLastWrittenPoint_ = false;
    fileOpen_ = false;

    if (storage_ != nullptr && storage_->ready()) {
        status_ = JourneyStatus::WaitingForTime;
    } else {
        status_ = JourneyStatus::Disabled;
    }
}

void JourneyLogger::syncStorageAvailability()
{
    if (storage_ == nullptr) {
        status_ = JourneyStatus::Disabled;
        return;
    }

    if (!storage_->ready()) {
        if (fileOpen_) {
            closeFile(true);
        }
        currentFilePath_[0] = '\0';
        currentFileName_[0] = '\0';
        if (status_ != JourneyStatus::StorageError) {
            status_ = JourneyStatus::Disabled;
        }
        return;
    }
}

bool JourneyLogger::flush()
{
    if (!fileOpen_ || !gActiveJourneyFile) {
        return true;
    }

    gActiveJourneyFile.flush();
    lastFlushMs_ = millis();
    pointsSinceFlush_ = 0;
    return true;
}

bool JourneyLogger::ensureJourneysDirectory()
{
    if (!SD.exists(kJourneysDir) && !SD.mkdir(kJourneysDir)) {
        return false;
    }
    return true;
}

bool JourneyLogger::openNewFile(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute,
                                uint8_t second)
{
    if (!ensureJourneysDirectory()) {
        return false;
    }

    std::snprintf(currentFileName_, sizeof(currentFileName_), "%04u%02u%02u_%02u%02u%02u.csv", year, month, day, hour,
                  minute, second);
    std::snprintf(currentFilePath_, sizeof(currentFilePath_), "%s/%s", kJourneysDir, currentFileName_);

    gActiveJourneyFile = SD.open(currentFilePath_, FILE_WRITE);
    if (!gActiveJourneyFile) {
        currentFilePath_[0] = '\0';
        currentFileName_[0] = '\0';
        return false;
    }

    fileOpen_ = true;
    pointsSinceFlush_ = 0;
    lastFlushMs_ = 0;

    const size_t headerLength = std::strlen(kCsvHeader);
    if (gActiveJourneyFile.write(reinterpret_cast<const uint8_t*>(kCsvHeader), headerLength) != headerLength) {
        gActiveJourneyFile.close();
        gActiveJourneyFile = File();
        fileOpen_ = false;
        SD.remove(currentFilePath_);
        currentFilePath_[0] = '\0';
        currentFileName_[0] = '\0';
        return false;
    }

    return true;
}

void JourneyLogger::closeFile(bool flushFirst)
{
    if (!fileOpen_) {
        return;
    }

    if (gActiveJourneyFile) {
        if (flushFirst) {
            gActiveJourneyFile.flush();
        }
        gActiveJourneyFile.close();
    }

    gActiveJourneyFile = File();
    fileOpen_ = false;
    pointsSinceFlush_ = 0;
}

bool JourneyLogger::writePoint(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute,
                               uint8_t second, double latitude, double longitude, double altitudeMeters,
                               double speedKmph, uint32_t satellites, uint32_t locationAgeMs)
{
    if (!gActiveJourneyFile) {
        gActiveJourneyFile = SD.open(currentFilePath_, FILE_APPEND);
        if (!gActiveJourneyFile) {
            return false;
        }
        fileOpen_ = true;
    }

    char line[128];
    std::snprintf(line, sizeof(line), "%04u-%02u-%02u,%02u:%02u:%02u,%.6f,%.6f,%.1f,%.1f,%u,%u\n", year, month, day,
                  hour, minute, second, latitude, longitude, altitudeMeters, speedKmph, satellites, locationAgeMs);

    const size_t lineLength = std::strlen(line);
    return gActiveJourneyFile.write(reinterpret_cast<const uint8_t*>(line), lineLength) == lineLength;
}

void JourneyLogger::flushIfNeeded(uint32_t nowMs, bool force)
{
    if (!fileOpen_ || !gActiveJourneyFile) {
        return;
    }

    if (!force && pointsSinceFlush_ < kFlushEveryPoints &&
        (lastFlushMs_ == 0 || (nowMs - lastFlushMs_) < kFlushIntervalMs)) {
        return;
    }

    gActiveJourneyFile.flush();
    lastFlushMs_ = nowMs;
    pointsSinceFlush_ = 0;
}

void JourneyLogger::handleWriteError()
{
    ++writeErrorCount_;
    Serial.printf("JOURNEY write error count=%u\n", writeErrorCount_);
    closeFile(true);
    status_ = JourneyStatus::StorageError;
}

bool JourneyLogger::isPointEligible(bool locationValid, double latitude, double longitude, double altitudeMeters,
                                    double speedKmph, uint32_t locationAgeMs) const
{
    if (!locationValid || locationAgeMs > kMaxLocationAgeMs) {
        return false;
    }
    if (!isFiniteNumber(latitude) || !isFiniteNumber(longitude)) {
        return false;
    }
    if (!isFiniteNumber(altitudeMeters)) {
        return false;
    }
    if (!isFiniteNumber(speedKmph) || speedKmph < 0.0 || speedKmph > kMaxSpeedKmph) {
        return false;
    }
    return true;
}

bool JourneyLogger::sameUtcSecond(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute,
                                  uint8_t second) const
{
    return hasLastWrittenPoint_ && year == lastWrittenYear_ && month == lastWrittenMonth_ && day == lastWrittenDay_ &&
           hour == lastWrittenHour_ && minute == lastWrittenMinute_ && second == lastWrittenSecond_;
}

bool JourneyLogger::shouldWritePoint(double latitude, double longitude, uint32_t nowMs) const
{
    if (!hasLastWrittenPoint_) {
        return true;
    }

    if ((nowMs - lastWrittenMs_) >= kMinWriteIntervalMs) {
        return true;
    }

    const double distanceMeters = haversineMeters(lastWrittenLat_, lastWrittenLon_, latitude, longitude);
    return distanceMeters >= kMinWriteDistanceMeters;
}

const char* JourneyLogger::currentFileName() const
{
    if (currentFileName_[0] == '\0') {
        return "--";
    }
    return currentFileName_;
}

void JourneyLogger::printPeriodicDiagnostics(uint32_t nowMs)
{
    if (status_ != JourneyStatus::Recording && status_ != JourneyStatus::Paused) {
        return;
    }

    if (lastSerialDiagMs_ != 0 && (nowMs - lastSerialDiagMs_) < kSerialDiagIntervalMs) {
        return;
    }

    lastSerialDiagMs_ = nowMs;

    const char* statusLabel = "REC";
    if (status_ == JourneyStatus::Paused) {
        statusLabel = "PAUSED";
    }

    Serial.printf("JOURNEY status=%s points=%u skipped=%u errors=%u file=%s\n", statusLabel, writtenPointCount_,
                  skippedPointCount_, writeErrorCount_, currentFileName());
}

void JourneyLogger::update(bool locationValid, bool dateValid, bool timeValid, double latitude, double longitude,
                           double altitudeMeters, double speedKmph, uint32_t satellites, uint32_t locationAgeMs,
                           uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second,
                           uint32_t nowMs, bool sessionRecording, bool accurateFix)
{
    syncStorageAvailability();

    if (!sessionRecording || !accurateFix || status_ == JourneyStatus::Disabled ||
        status_ == JourneyStatus::StorageError) {
        return;
    }

    if (status_ == JourneyStatus::Paused) {
        printPeriodicDiagnostics(nowMs);
        return;
    }

    if (lastProcessMs_ != 0 && (nowMs - lastProcessMs_) < kMaxProcessIntervalMs) {
        return;
    }
    lastProcessMs_ = nowMs;

    flushIfNeeded(nowMs, false);

    const bool gnssReady = locationValid && dateValid && timeValid;
    if (status_ == JourneyStatus::WaitingForTime) {
        if (!gnssReady || storage_ == nullptr || !storage_->ready()) {
            return;
        }

        if (!openNewFile(year, month, day, hour, minute, second)) {
            handleWriteError();
            return;
        }

        status_ = JourneyStatus::Recording;
        lastSerialDiagMs_ = nowMs;
        Serial.printf("JOURNEY started file=%s\n", currentFilePath_);
    }

    if (status_ != JourneyStatus::Recording) {
        return;
    }

    printPeriodicDiagnostics(nowMs);

    if (!isPointEligible(locationValid, latitude, longitude, altitudeMeters, speedKmph, locationAgeMs)) {
        return;
    }

    if (sameUtcSecond(year, month, day, hour, minute, second)) {
        return;
    }

    if (!shouldWritePoint(latitude, longitude, nowMs)) {
        ++skippedPointCount_;
        return;
    }

    if (!writePoint(year, month, day, hour, minute, second, latitude, longitude, altitudeMeters, speedKmph,
                    satellites, locationAgeMs)) {
        handleWriteError();
        return;
    }

    ++writtenPointCount_;
    lastWrittenMs_ = nowMs;
    hasLastWrittenPoint_ = true;
    lastWrittenLat_ = latitude;
    lastWrittenLon_ = longitude;
    lastWrittenYear_ = year;
    lastWrittenMonth_ = month;
    lastWrittenDay_ = day;
    lastWrittenHour_ = hour;
    lastWrittenMinute_ = minute;
    lastWrittenSecond_ = second;

    ++pointsSinceFlush_;
    flushIfNeeded(nowMs, false);
}

void JourneyLogger::pause()
{
    if (status_ != JourneyStatus::Recording) {
        return;
    }

    flushIfNeeded(millis(), true);
    status_ = JourneyStatus::Paused;
    Serial.printf("JOURNEY paused points=%u\n", writtenPointCount_);
}

void JourneyLogger::resume()
{
    if (status_ != JourneyStatus::Paused) {
        return;
    }

    status_ = JourneyStatus::Recording;
    lastSerialDiagMs_ = millis();
    Serial.println("JOURNEY resumed");
}

bool JourneyLogger::stop()
{
    if (status_ != JourneyStatus::Recording && status_ != JourneyStatus::Paused &&
        status_ != JourneyStatus::StorageError) {
        return false;
    }

    closeFile(true);

    Serial.printf("JOURNEY stopped points=%u\n", writtenPointCount_);

    currentFilePath_[0] = '\0';
    currentFileName_[0] = '\0';
    hasLastWrittenPoint_ = false;
    pointsSinceFlush_ = 0;
    lastFlushMs_ = 0;
    fileOpen_ = false;

    if (storage_ != nullptr && storage_->ready()) {
        status_ = JourneyStatus::Disabled;
    } else {
        status_ = JourneyStatus::Disabled;
    }

    return true;
}

JourneyStatus JourneyLogger::status() const
{
    return status_;
}

bool JourneyLogger::recording() const
{
    return status_ == JourneyStatus::Recording;
}

bool JourneyLogger::paused() const
{
    return status_ == JourneyStatus::Paused;
}

uint32_t JourneyLogger::writtenPointCount() const
{
    return writtenPointCount_;
}

uint32_t JourneyLogger::skippedPointCount() const
{
    return skippedPointCount_;
}

uint32_t JourneyLogger::writeErrorCount() const
{
    return writeErrorCount_;
}

const char* JourneyLogger::currentFilePath() const
{
    return currentFilePath_;
}

void JourneyLogger::resetPositionReference()
{
    hasLastWrittenPoint_ = false;
}

bool JourneyLogger::verifyOpenFile(uint64_t& sizeBytesOut) const
{
    sizeBytesOut = 0;

    if (currentFilePath_[0] == '\0') {
        return true;
    }

    if (!SD.exists(currentFilePath_)) {
        return false;
    }

    File file = SD.open(currentFilePath_, FILE_READ);
    if (!file) {
        return false;
    }

    sizeBytesOut = file.size();
    file.close();
    return true;
}
