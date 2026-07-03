#pragma once

#include <cstdint>

#include "JourneyLogger.h"
#include "RecordingSessionManager.h"

#include <M5GFX.h>

namespace lgfx {
inline namespace v1 {
class LGFXBase;
}
}  // namespace lgfx

namespace Theme {

constexpr int kScreenWidth = 240;
constexpr int kScreenHeight = 135;
constexpr int kHeaderBottom = 20;
constexpr int kDividerY = 21;
constexpr int kContentTop = 24;
constexpr int kContentBottom = 109;
constexpr int kFooterTop = 112;
constexpr int kLabelX = 6;
constexpr int kValueX = 76;
constexpr int kContentStartY = kContentTop;
constexpr int kBadgeRowHeight = 14;

constexpr uint16_t kBackground = TFT_BLACK;
constexpr uint16_t kPrimaryText = TFT_WHITE;
constexpr uint16_t kSecondaryText = TFT_LIGHTGREY;
constexpr uint16_t kTitle = TFT_CYAN;
constexpr uint16_t kSuccess = TFT_GREEN;
constexpr uint16_t kWarning = TFT_YELLOW;
constexpr uint16_t kPaused = TFT_ORANGE;
constexpr uint16_t kError = TFT_RED;
constexpr uint16_t kIdle = TFT_DARKGREY;
constexpr uint16_t kInfo = TFT_BLUE;
constexpr uint16_t kSelected = TFT_BLUE;
constexpr uint16_t kBorder = TFT_DARKGREY;

constexpr uint16_t kBadgeTextDark = TFT_BLACK;
constexpr uint16_t kBadgeTextLight = TFT_WHITE;
constexpr uint16_t kSuccessBorder = TFT_DARKGREEN;
constexpr uint16_t kWarningBorder = TFT_ORANGE;
constexpr uint16_t kPausedBorder = 0xA000;
constexpr uint16_t kErrorBorder = TFT_MAROON;
constexpr uint16_t kIdleBorder = TFT_LIGHTGREY;
constexpr uint16_t kInfoBorder = TFT_NAVY;

struct BadgeColors {
    uint16_t background;
    uint16_t text;
    uint16_t border;
};

uint16_t sessionStateColor(RecordingSessionState state);
uint16_t gnssFixColor(bool valid, bool accurate);
uint16_t satelliteCountColor(uint32_t satellites);
uint16_t hdopColor(bool valid, double hdop);
uint16_t ageColor(bool valid, uint32_t ageMs);
uint16_t sdStateColor(bool ready, bool selfTestPassed);
uint16_t journeyStateColor(JourneyStatus state);
uint16_t journeyStatusLabelColor(const char* label);
uint16_t explorerStateColor(const char* stateLabel);
uint16_t explorerModeColor(bool sdReady);
uint16_t driveStateColor(const char* statusLabel);
uint16_t driveSpeedColor(bool speedValid, double speedKmph);
uint16_t driveRejectedPointsColor(uint32_t rejectedCount);
uint16_t batteryColor(bool valid, int32_t levelPercent);
uint16_t transientMessageColor(const char* message);
uint16_t sdBrowserEntryColor(bool isDirectory, bool selected);
uint16_t sdSelfTestTextColor(bool passed);

BadgeColors badgeColorsSuccess();
BadgeColors badgeColorsWarning();
BadgeColors badgeColorsPaused();
BadgeColors badgeColorsError();
BadgeColors badgeColorsIdle();
BadgeColors badgeColorsInfo();

BadgeColors sessionStateBadgeColors(RecordingSessionState state);
BadgeColors gnssFixBadgeColors(bool valid, bool accurate);
BadgeColors explorerStateBadgeColors(const char* stateLabel);
BadgeColors explorerModeBadgeColors(bool sdReady);
BadgeColors driveStateBadgeColors(const char* statusLabel);
BadgeColors journeyStatusBadgeColors(const char* label);
BadgeColors sdReadyBadgeColors(bool ready);
BadgeColors sdSelfTestBadgeColors(bool passed);
BadgeColors readOnlyBadgeColors();
BadgeColors transientMessageBadgeColors(const char* message);

void drawPageHeader(lgfx::LGFXBase& canvas, const char* pageName);
int16_t standardRowHeight(lgfx::LGFXBase& canvas);
int16_t drawDataRow(lgfx::LGFXBase& canvas, int16_t y, const char* label, const char* value, uint16_t valueColor);
int16_t drawBadgeRow(lgfx::LGFXBase& canvas, int16_t y, const char* label, const char* value, BadgeColors colors);
void drawInfoCard(lgfx::LGFXBase& canvas, int16_t x, int16_t y, int16_t w, int16_t h);
void drawPageFooter(lgfx::LGFXBase& canvas, const char* leftText, const char* centerText, const char* rightText);
void drawControlHint(lgfx::LGFXBase& canvas, int16_t y, const char* hintA, const char* hintB, uint32_t nowMs);
void formatDisplayPath(const char* path, char* out, size_t outSize);
void truncateText(lgfx::LGFXBase& canvas, const char* text, char* out, size_t outSize, int16_t maxWidth);
void drawStatusValue(lgfx::LGFXBase& canvas, int16_t x, int16_t y, const char* label, const char* value,
                     uint16_t valueColor);
void drawStatusBadge(lgfx::LGFXBase& canvas, int16_t x, int16_t y, const char* text, uint16_t bgColor,
                     uint16_t textColor, uint16_t borderColor);
int16_t drawLabeledStatusBadge(lgfx::LGFXBase& canvas, int16_t x, int16_t y, const char* label,
                               const char* value, BadgeColors colors);
void drawTransientBox(lgfx::LGFXBase& canvas, const char* message, BadgeColors colors);

}  // namespace Theme
