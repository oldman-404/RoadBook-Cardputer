#include "Theme.h"

#include <lgfx/v1/LGFXBase.hpp>

#include <cstdio>
#include <cstring>

namespace Theme {

uint16_t sessionStateColor(RecordingSessionState state)
{
    switch (state) {
        case RecordingSessionState::Idle:
            return kIdle;
        case RecordingSessionState::WaitingForFix:
            return kWarning;
        case RecordingSessionState::Recording:
            return kSuccess;
        case RecordingSessionState::Paused:
            return kPaused;
        case RecordingSessionState::Stopped:
            return kIdle;
        case RecordingSessionState::StorageError:
            return kError;
    }
    return kIdle;
}

uint16_t gnssFixColor(bool valid, bool accurate)
{
    if (!valid) {
        return kError;
    }
    if (accurate) {
        return kSuccess;
    }
    return kWarning;
}

uint16_t satelliteCountColor(uint32_t satellites)
{
    if (satellites <= 3) {
        return kError;
    }
    if (satellites <= 7) {
        return kWarning;
    }
    return kSuccess;
}

uint16_t hdopColor(bool valid, double hdop)
{
    if (!valid) {
        return kSecondaryText;
    }
    if (hdop <= 1.5) {
        return kSuccess;
    }
    if (hdop <= 3.0) {
        return kWarning;
    }
    return kError;
}

uint16_t ageColor(bool valid, uint32_t ageMs)
{
    if (!valid) {
        return kSecondaryText;
    }
    if (ageMs <= 1000) {
        return kSuccess;
    }
    if (ageMs <= 3000) {
        return kWarning;
    }
    return kError;
}

uint16_t sdStateColor(bool ready, bool selfTestPassed)
{
    if (!ready) {
        return kError;
    }
    if (!selfTestPassed) {
        return kWarning;
    }
    return kSuccess;
}

uint16_t journeyStateColor(JourneyStatus state)
{
    switch (state) {
        case JourneyStatus::Disabled:
            return kIdle;
        case JourneyStatus::WaitingForTime:
            return kWarning;
        case JourneyStatus::Recording:
            return kSuccess;
        case JourneyStatus::Paused:
            return kPaused;
        case JourneyStatus::StorageError:
            return kError;
    }
    return kIdle;
}

uint16_t journeyStatusLabelColor(const char* label)
{
    if (label == nullptr) {
        return kIdle;
    }
    if (std::strcmp(label, "WAIT FIX") == 0) {
        return kWarning;
    }
    if (std::strcmp(label, "WAITING") == 0) {
        return kWarning;
    }
    if (std::strcmp(label, "REC") == 0) {
        return kSuccess;
    }
    if (std::strcmp(label, "PAUSED") == 0) {
        return kPaused;
    }
    if (std::strcmp(label, "ERROR") == 0) {
        return kError;
    }
    if (std::strcmp(label, "DISABLED") == 0) {
        return kIdle;
    }
    return kPrimaryText;
}

uint16_t explorerStateColor(const char* stateLabel)
{
    if (stateLabel == nullptr) {
        return kIdle;
    }
    if (std::strcmp(stateLabel, "NEW") == 0) {
        return kSuccess;
    }
    if (std::strcmp(stateLabel, "VISITED") == 0) {
        return kInfo;
    }
    if (std::strcmp(stateLabel, "WAITING") == 0 || std::strcmp(stateLabel, "WAIT FIX") == 0) {
        return kWarning;
    }
    if (std::strcmp(stateLabel, "CACHE FULL") == 0 || std::strcmp(stateLabel, "ERROR") == 0) {
        return kError;
    }
    if (std::strcmp(stateLabel, "PAUSED") == 0) {
        return kPaused;
    }
    return kIdle;
}

uint16_t explorerModeColor(bool sdReady)
{
    return sdReady ? kSuccess : kWarning;
}

uint16_t driveStateColor(const char* statusLabel)
{
    if (statusLabel == nullptr) {
        return kIdle;
    }
    if (std::strcmp(statusLabel, "WAIT FIX") == 0 || std::strcmp(statusLabel, "WAITING") == 0) {
        return kWarning;
    }
    if (std::strcmp(statusLabel, "DRIVING") == 0) {
        return kSuccess;
    }
    if (std::strcmp(statusLabel, "STOPPED") == 0 || std::strcmp(statusLabel, "IDLE") == 0) {
        return kIdle;
    }
    if (std::strcmp(statusLabel, "PAUSED") == 0) {
        return kPaused;
    }
    if (std::strcmp(statusLabel, "ERROR") == 0) {
        return kError;
    }
    return kPrimaryText;
}

uint16_t driveSpeedColor(bool speedValid, double speedKmph)
{
    if (!speedValid || speedKmph < 3.0) {
        return kSecondaryText;
    }
    if (speedKmph > 180.0) {
        return kError;
    }
    if (speedKmph > 120.0) {
        return kWarning;
    }
    return kPrimaryText;
}

uint16_t driveRejectedPointsColor(uint32_t rejectedCount)
{
    if (rejectedCount == 0) {
        return kSuccess;
    }
    if (rejectedCount <= 10) {
        return kWarning;
    }
    return kError;
}

uint16_t batteryColor(bool valid, int32_t levelPercent)
{
    if (!valid) {
        return kSecondaryText;
    }
    if (levelPercent <= 10) {
        return kError;
    }
    if (levelPercent <= 20) {
        return kWarning;
    }
    return kSuccess;
}

uint16_t transientMessageColor(const char* message)
{
    if (message == nullptr || message[0] == '\0') {
        return kPrimaryText;
    }
    if (std::strncmp(message, "SAVE ERROR", 10) == 0) {
        return kError;
    }
    if (std::strncmp(message, "SAVED", 5) == 0) {
        return kSuccess;
    }
    if (std::strncmp(message, "SESSION STARTED", 15) == 0 || std::strcmp(message, "RESUMED") == 0) {
        return kSuccess;
    }
    if (std::strcmp(message, "PAUSED") == 0) {
        return kPaused;
    }
    if (std::strncmp(message, "WAIT FIX", 8) == 0) {
        return kWarning;
    }
    if (std::strncmp(message, "SESSION STOPPED", 15) == 0) {
        return kSecondaryText;
    }
    return kPrimaryText;
}

uint16_t sdBrowserEntryColor(bool isDirectory, bool selected)
{
    if (selected) {
        return kSelected;
    }
    if (isDirectory) {
        return kInfo;
    }
    return kPrimaryText;
}

uint16_t sdSelfTestTextColor(bool passed)
{
    return passed ? kSuccess : kError;
}

BadgeColors badgeColorsSuccess()
{
    return {kSuccess, kBadgeTextDark, kSuccessBorder};
}

BadgeColors badgeColorsWarning()
{
    return {kWarning, kBadgeTextDark, kWarningBorder};
}

BadgeColors badgeColorsPaused()
{
    return {kPaused, kBadgeTextDark, kPausedBorder};
}

BadgeColors badgeColorsError()
{
    return {kError, kBadgeTextLight, kErrorBorder};
}

BadgeColors badgeColorsIdle()
{
    return {kIdle, kBadgeTextLight, kIdleBorder};
}

BadgeColors badgeColorsInfo()
{
    return {kInfo, kBadgeTextLight, kInfoBorder};
}

BadgeColors sessionStateBadgeColors(RecordingSessionState state)
{
    switch (state) {
        case RecordingSessionState::Idle:
        case RecordingSessionState::Stopped:
            return badgeColorsIdle();
        case RecordingSessionState::WaitingForFix:
            return badgeColorsWarning();
        case RecordingSessionState::Recording:
            return badgeColorsSuccess();
        case RecordingSessionState::Paused:
            return badgeColorsPaused();
        case RecordingSessionState::StorageError:
            return badgeColorsError();
    }
    return badgeColorsIdle();
}

BadgeColors gnssFixBadgeColors(bool valid, bool accurate)
{
    if (!valid) {
        return badgeColorsError();
    }
    if (accurate) {
        return badgeColorsSuccess();
    }
    return badgeColorsWarning();
}

BadgeColors explorerStateBadgeColors(const char* stateLabel)
{
    if (stateLabel == nullptr) {
        return badgeColorsIdle();
    }
    if (std::strcmp(stateLabel, "NEW") == 0) {
        return badgeColorsSuccess();
    }
    if (std::strcmp(stateLabel, "VISITED") == 0) {
        return badgeColorsInfo();
    }
    if (std::strcmp(stateLabel, "WAITING") == 0 || std::strcmp(stateLabel, "WAIT FIX") == 0) {
        return badgeColorsWarning();
    }
    if (std::strcmp(stateLabel, "CACHE FULL") == 0 || std::strcmp(stateLabel, "ERROR") == 0) {
        return badgeColorsError();
    }
    if (std::strcmp(stateLabel, "PAUSED") == 0) {
        return badgeColorsPaused();
    }
    return badgeColorsIdle();
}

BadgeColors explorerModeBadgeColors(bool sdReady)
{
    return sdReady ? badgeColorsSuccess() : badgeColorsWarning();
}

BadgeColors driveStateBadgeColors(const char* statusLabel)
{
    if (statusLabel == nullptr) {
        return badgeColorsIdle();
    }
    if (std::strcmp(statusLabel, "WAIT FIX") == 0 || std::strcmp(statusLabel, "WAITING") == 0) {
        return badgeColorsWarning();
    }
    if (std::strcmp(statusLabel, "DRIVING") == 0) {
        return badgeColorsSuccess();
    }
    if (std::strcmp(statusLabel, "STOPPED") == 0 || std::strcmp(statusLabel, "IDLE") == 0) {
        return badgeColorsIdle();
    }
    if (std::strcmp(statusLabel, "PAUSED") == 0) {
        return badgeColorsPaused();
    }
    if (std::strcmp(statusLabel, "ERROR") == 0) {
        return badgeColorsError();
    }
    return badgeColorsIdle();
}

BadgeColors journeyStatusBadgeColors(const char* label)
{
    if (label == nullptr) {
        return badgeColorsIdle();
    }
    if (std::strcmp(label, "WAIT FIX") == 0 || std::strcmp(label, "WAITING") == 0) {
        return badgeColorsWarning();
    }
    if (std::strcmp(label, "REC") == 0) {
        return badgeColorsSuccess();
    }
    if (std::strcmp(label, "PAUSED") == 0) {
        return badgeColorsPaused();
    }
    if (std::strcmp(label, "ERROR") == 0) {
        return badgeColorsError();
    }
    if (std::strcmp(label, "DISABLED") == 0) {
        return badgeColorsIdle();
    }
    return badgeColorsIdle();
}

BadgeColors sdReadyBadgeColors(bool ready)
{
    return ready ? badgeColorsSuccess() : badgeColorsError();
}

BadgeColors sdSelfTestBadgeColors(bool passed)
{
    return passed ? badgeColorsSuccess() : badgeColorsError();
}

BadgeColors readOnlyBadgeColors()
{
    return badgeColorsWarning();
}

BadgeColors transientMessageBadgeColors(const char* message)
{
    if (message == nullptr || message[0] == '\0') {
        return badgeColorsIdle();
    }
    if (std::strncmp(message, "SAVE ERROR", 10) == 0 || std::strncmp(message, "PHOTO SAVE ERROR", 16) == 0 ||
        std::strncmp(message, "PHOTO SD ERROR", 14) == 0) {
        return badgeColorsError();
    }
    if (std::strncmp(message, "SAVED", 5) == 0 || std::strncmp(message, "PHOTO SAVED", 11) == 0) {
        return badgeColorsSuccess();
    }
    if (std::strncmp(message, "SESSION STARTED", 15) == 0 || std::strcmp(message, "RESUMED") == 0) {
        return badgeColorsSuccess();
    }
    if (std::strcmp(message, "PAUSED") == 0) {
        return badgeColorsPaused();
    }
    if (std::strncmp(message, "WAIT FIX", 8) == 0 || std::strncmp(message, "PHOTO NO FIX", 12) == 0) {
        return badgeColorsWarning();
    }
    if (std::strncmp(message, "SESSION STOPPED", 15) == 0) {
        return badgeColorsIdle();
    }
    return badgeColorsIdle();
}

void drawPageHeader(lgfx::LGFXBase& canvas, const char* pageName)
{
    char title[32];
    std::snprintf(title, sizeof(title), "RoadBook %s", pageName);

    canvas.setTextSize(1);
    canvas.setTextDatum(TL_DATUM);
    canvas.setTextColor(kTitle);
    canvas.drawString(title, kLabelX, 4);
    canvas.drawFastHLine(0, kDividerY, canvas.width(), kBorder);
    canvas.setTextColor(kPrimaryText);
}

int16_t standardRowHeight(lgfx::LGFXBase& canvas)
{
    return static_cast<int16_t>(canvas.fontHeight() + 4);
}

void truncateText(lgfx::LGFXBase& canvas, const char* text, char* out, size_t outSize, int16_t maxWidth)
{
    if (outSize == 0) {
        return;
    }

    if (text == nullptr) {
        out[0] = '\0';
        return;
    }

    std::snprintf(out, outSize, "%s", text);
    size_t length = std::strlen(out);
    while (length > 4 && canvas.textWidth(out) > maxWidth) {
        out[length - 1] = '\0';
        --length;
    }

    if (length > 3 && canvas.textWidth(out) > maxWidth) {
        out[length - 3] = '.';
        out[length - 2] = '.';
        out[length - 1] = '.';
        out[length] = '\0';
    }
}

void formatDisplayPath(const char* path, char* out, size_t outSize)
{
    if (path == nullptr || outSize == 0) {
        return;
    }

    if (std::strlen(path) <= 28) {
        std::snprintf(out, outSize, "%s", path);
        return;
    }

    const size_t prefixLen = 12;
    const size_t suffixLen = 12;
    const size_t pathLen = std::strlen(path);
    if (pathLen <= prefixLen + suffixLen + 3) {
        std::snprintf(out, outSize, "%s", path);
        return;
    }

    char prefix[16];
    char suffix[16];
    std::memcpy(prefix, path, prefixLen);
    prefix[prefixLen] = '\0';
    std::snprintf(suffix, sizeof(suffix), "%s", path + (pathLen - suffixLen));
    std::snprintf(out, outSize, "%s...%s", prefix, suffix);
}

int16_t drawDataRow(lgfx::LGFXBase& canvas, int16_t y, const char* label, const char* value, uint16_t valueColor)
{
    canvas.setTextSize(1);
    canvas.setTextDatum(TL_DATUM);
    canvas.setTextColor(kSecondaryText);
    canvas.drawString(label, kLabelX, y);
    canvas.setTextColor(valueColor);
    canvas.drawString(value, kValueX, y);
    canvas.setTextColor(kPrimaryText);
    return standardRowHeight(canvas);
}

int16_t drawBadgeRow(lgfx::LGFXBase& canvas, int16_t y, const char* label, const char* value, BadgeColors colors)
{
    canvas.setTextSize(1);
    canvas.setTextDatum(TL_DATUM);
    canvas.setTextColor(kSecondaryText);
    canvas.drawString(label, kLabelX, y + 1);

    const int textHeight = canvas.fontHeight();
    const int badgeHeight = textHeight + 4;
    const int badgeY = y + ((standardRowHeight(canvas) - badgeHeight) / 2);
    drawStatusBadge(canvas, kValueX, static_cast<int16_t>(badgeY), value, colors.background, colors.text,
                    colors.border);
    return standardRowHeight(canvas);
}

void drawInfoCard(lgfx::LGFXBase& canvas, int16_t x, int16_t y, int16_t w, int16_t h)
{
    canvas.fillRoundRect(x, y, w, h, 2, 0x2104);
    canvas.drawRoundRect(x, y, w, h, 2, kBorder);
}

void drawPageFooter(lgfx::LGFXBase& canvas, const char* leftText, const char* centerText, const char* rightText)
{
    canvas.setTextSize(1);
    canvas.setTextDatum(TL_DATUM);
    canvas.setTextColor(kSecondaryText);

    if (leftText != nullptr && leftText[0] != '\0') {
        canvas.drawString(leftText, kLabelX, kFooterTop);
    }
    if (centerText != nullptr && centerText[0] != '\0') {
        canvas.setTextDatum(top_center);
        canvas.drawString(centerText, canvas.width() / 2, kFooterTop);
        canvas.setTextDatum(TL_DATUM);
    }
    if (rightText != nullptr && rightText[0] != '\0') {
        canvas.setTextDatum(TR_DATUM);
        canvas.drawString(rightText, canvas.width() - kLabelX, kFooterTop);
        canvas.setTextDatum(TL_DATUM);
    }

    canvas.setTextColor(kPrimaryText);
}

void drawControlHint(lgfx::LGFXBase& canvas, int16_t y, const char* hintA, const char* hintB, uint32_t nowMs)
{
    const char* hint = ((nowMs / 2000U) % 2U) == 0U ? hintA : hintB;
    if (hint == nullptr || hint[0] == '\0') {
        return;
    }

    canvas.setTextSize(1);
    canvas.setTextDatum(TL_DATUM);
    canvas.setTextColor(kSecondaryText);
    canvas.drawString(hint, kLabelX, y);
    canvas.setTextColor(kPrimaryText);
}

void drawStatusValue(lgfx::LGFXBase& canvas, int16_t x, int16_t y, const char* label, const char* value,
                     uint16_t valueColor)
{
    canvas.setTextSize(1);
    canvas.setTextDatum(TL_DATUM);
    canvas.setTextColor(kSecondaryText);
    canvas.drawString(label, x, y);

    const int labelWidth = canvas.textWidth(label);
    canvas.setTextColor(valueColor);
    canvas.drawString(value, static_cast<int16_t>(x + labelWidth), y);
    canvas.setTextColor(kPrimaryText);
}

void drawStatusBadge(lgfx::LGFXBase& canvas, int16_t x, int16_t y, const char* text, uint16_t bgColor,
                     uint16_t textColor, uint16_t borderColor)
{
    constexpr int kPaddingH = 3;
    constexpr int kPaddingV = 2;
    constexpr int kRadius = 2;

    canvas.setTextSize(1);
    canvas.setTextDatum(TL_DATUM);

    const int textWidth = canvas.textWidth(text);
    const int textHeight = canvas.fontHeight();
    const int badgeWidth = textWidth + (kPaddingH * 2);
    const int badgeHeight = textHeight + (kPaddingV * 2);

    canvas.fillRoundRect(x, y, badgeWidth, badgeHeight, kRadius, bgColor);
    canvas.drawRoundRect(x, y, badgeWidth, badgeHeight, kRadius, borderColor);
    canvas.setTextColor(textColor);
    canvas.drawString(text, static_cast<int16_t>(x + kPaddingH), static_cast<int16_t>(y + kPaddingV));
    canvas.setTextColor(kPrimaryText);
}

int16_t drawLabeledStatusBadge(lgfx::LGFXBase& canvas, int16_t x, int16_t y, const char* label, const char* value,
                               BadgeColors colors)
{
    (void)x;
    return drawBadgeRow(canvas, y, label, value, colors);
}

void drawTransientBox(lgfx::LGFXBase& canvas, const char* message, BadgeColors colors)
{
    constexpr int kPadding = 4;
    constexpr int kRadius = 2;

    canvas.setTextSize(1);
    canvas.setTextDatum(middle_center);

    const int textWidth = canvas.textWidth(message);
    const int textHeight = canvas.fontHeight();
    const int boxWidth = textWidth + (kPadding * 2);
    const int boxHeight = textHeight + (kPadding * 2);
    const int centerX = canvas.width() / 2;
    const int centerY = canvas.height() - (boxHeight / 2) - 4;
    const int boxX = centerX - (boxWidth / 2);
    const int boxY = centerY - (boxHeight / 2);

    canvas.fillRoundRect(boxX, boxY, boxWidth, boxHeight, kRadius, kBackground);
    canvas.drawRoundRect(boxX, boxY, boxWidth, boxHeight, kRadius, colors.border);
    const uint16_t messageTextColor =
        (colors.text == kBadgeTextDark) ? colors.background : colors.text;
    canvas.setTextColor(messageTextColor);
    canvas.drawString(message, centerX, centerY);
    canvas.setTextColor(kPrimaryText);
    canvas.setTextDatum(TL_DATUM);
}

}  // namespace Theme
