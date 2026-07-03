#include "PhotoLocationLogger.h"

#include "GPSManager.h"
#include "RecordingSessionManager.h"
#include "StorageManager.h"

#include <SD.h>

#include <cstdio>
#include <cstring>

namespace {

constexpr const char* kPhotosDir = "/roadbook/photos";
constexpr const char* kPhotoLocationsPath = "/roadbook/photos/photo_locations.csv";
constexpr const char* kCsvHeader =
    "utc_date,utc_time,latitude,longitude,altitude_m,satellites,hdop,session_state\n";

}  // namespace

void PhotoLocationLogger::begin(StorageManager& storage)
{
    storage_ = &storage;
    fileInitialized_ = false;
}

bool PhotoLocationLogger::ensurePhotosDirectory() const
{
    if (!SD.exists(kPhotosDir) && !SD.mkdir(kPhotosDir)) {
        return false;
    }
    return true;
}

bool PhotoLocationLogger::ensureFileWithHeader()
{
    if (SD.exists(kPhotoLocationsPath)) {
        fileInitialized_ = true;
        return true;
    }

    if (!ensurePhotosDirectory()) {
        return false;
    }

    File file = SD.open(kPhotoLocationsPath, FILE_WRITE);
    if (!file) {
        return false;
    }

    const size_t headerLength = std::strlen(kCsvHeader);
    const bool ok = file.write(reinterpret_cast<const uint8_t*>(kCsvHeader), headerLength) == headerLength;
    file.close();
    fileInitialized_ = ok;
    return ok;
}

bool PhotoLocationLogger::writeRecord(const GPSManager& gps, const char* sessionStateLabel)
{
    if (!ensureFileWithHeader()) {
        return false;
    }

    File file = SD.open(kPhotoLocationsPath, FILE_APPEND);
    if (!file) {
        return false;
    }

    char line[160];
    const int length = std::snprintf(
        line, sizeof(line), "%04u-%02u-%02u,%02u:%02u:%02u,%.6f,%.6f,%.1f,%u,%.1f,%s\n", gps.year(), gps.month(),
        gps.day(), gps.hour(), gps.minute(), gps.second(), gps.latitude(), gps.longitude(), gps.altitudeMeters(),
        gps.satellites(), gps.hdop(), sessionStateLabel);

    if (length <= 0 || static_cast<size_t>(length) >= sizeof(line)) {
        file.close();
        return false;
    }

    const bool ok = file.write(reinterpret_cast<const uint8_t*>(line), static_cast<size_t>(length)) ==
                    static_cast<size_t>(length);
    file.flush();
    file.close();
    return ok;
}

PhotoCaptureResult PhotoLocationLogger::capture(const GPSManager& gps, const RecordingSessionManager& session,
                                                const char* sessionStateLabel)
{
    (void)session;

    if (storage_ == nullptr || !storage_->ready() ||
        storage_->selfTestResult() != SdSelfTestResult::Passed) {
        return PhotoCaptureResult::SdError;
    }

    if (!gps.hasAccurateFix() || !gps.dateValid() || !gps.timeValid()) {
        Serial.println("PHOTO capture rejected reason=no_fix");
        return PhotoCaptureResult::NoFix;
    }

    if (!writeRecord(gps, sessionStateLabel)) {
        return PhotoCaptureResult::WriteError;
    }

    Serial.printf("PHOTO saved lat=%.6f lon=%.6f file=%s\n", gps.latitude(), gps.longitude(), kPhotoLocationsPath);
    return PhotoCaptureResult::Saved;
}
