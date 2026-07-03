#pragma once

#include <cstdint>

class GPSManager;
class RecordingSessionManager;
class StorageManager;

enum class PhotoCaptureResult {
    Saved,
    NoFix,
    SdError,
    WriteError,
};

class PhotoLocationLogger {
public:
    void begin(StorageManager& storage);

    PhotoCaptureResult capture(const GPSManager& gps, const RecordingSessionManager& session,
                               const char* sessionStateLabel);

private:
    bool ensurePhotosDirectory() const;
    bool ensureFileWithHeader();
    bool writeRecord(const GPSManager& gps, const char* sessionStateLabel);

    StorageManager* storage_{nullptr};
    bool fileInitialized_{false};
};
