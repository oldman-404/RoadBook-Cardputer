#include "RecordingSessionManager.h"

void RecordingSessionManager::begin(uint32_t nowMs)
{
    state_ = RecordingSessionState::Idle;
    sessionStartMs_ = nowMs;
}

bool RecordingSessionManager::startWaitingForFix(uint32_t nowMs)
{
    if (state_ != RecordingSessionState::Idle && state_ != RecordingSessionState::Stopped &&
        state_ != RecordingSessionState::StorageError) {
        return false;
    }

    state_ = RecordingSessionState::WaitingForFix;
    sessionStartMs_ = nowMs;
    return true;
}

bool RecordingSessionManager::promoteToRecording(uint32_t nowMs)
{
    if (state_ != RecordingSessionState::WaitingForFix) {
        return false;
    }

    state_ = RecordingSessionState::Recording;
    (void)nowMs;
    return true;
}

bool RecordingSessionManager::pause(uint32_t nowMs)
{
    if (state_ != RecordingSessionState::Recording) {
        return false;
    }

    state_ = RecordingSessionState::Paused;
    (void)nowMs;
    return true;
}

bool RecordingSessionManager::resume(uint32_t nowMs)
{
    if (state_ != RecordingSessionState::Paused) {
        return false;
    }

    state_ = RecordingSessionState::Recording;
    (void)nowMs;
    return true;
}

bool RecordingSessionManager::stop(uint32_t nowMs)
{
    if (state_ != RecordingSessionState::Recording && state_ != RecordingSessionState::Paused) {
        return false;
    }

    state_ = RecordingSessionState::Stopped;
    (void)nowMs;
    return true;
}

bool RecordingSessionManager::cancelWaiting(uint32_t nowMs)
{
    if (state_ != RecordingSessionState::WaitingForFix) {
        return false;
    }

    state_ = RecordingSessionState::Stopped;
    (void)nowMs;
    return true;
}

void RecordingSessionManager::setStorageError()
{
    state_ = RecordingSessionState::StorageError;
}

RecordingSessionState RecordingSessionManager::state() const
{
    return state_;
}

bool RecordingSessionManager::active() const
{
    return state_ == RecordingSessionState::Recording || state_ == RecordingSessionState::Paused;
}

bool RecordingSessionManager::waitingForFix() const
{
    return state_ == RecordingSessionState::WaitingForFix;
}

bool RecordingSessionManager::recording() const
{
    return state_ == RecordingSessionState::Recording;
}

bool RecordingSessionManager::paused() const
{
    return state_ == RecordingSessionState::Paused;
}

uint32_t RecordingSessionManager::sessionStartMs() const
{
    return sessionStartMs_;
}
