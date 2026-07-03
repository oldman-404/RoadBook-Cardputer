#pragma once

#include <cstdint>

enum class RecordingSessionState {
    Idle,
    WaitingForFix,
    Recording,
    Paused,
    Stopped,
    StorageError,
};

class RecordingSessionManager {
public:
    void begin(uint32_t nowMs);

    bool startWaitingForFix(uint32_t nowMs);
    bool promoteToRecording(uint32_t nowMs);
    bool pause(uint32_t nowMs);
    bool resume(uint32_t nowMs);
    bool stop(uint32_t nowMs);
    bool cancelWaiting(uint32_t nowMs);
    void setStorageError();

    RecordingSessionState state() const;

    bool active() const;
    bool waitingForFix() const;
    bool recording() const;
    bool paused() const;

    uint32_t sessionStartMs() const;

private:
    RecordingSessionState state_{RecordingSessionState::Idle};
    uint32_t sessionStartMs_{0};
};
