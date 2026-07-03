#include "App.h"

#include "Theme.h"
#include "Version.h"

#include <M5Cardputer.h>
#include <M5Unified.h>

#include <cstdio>
#include <cstring>

#include <cmath>

namespace {

constexpr uint32_t kSplashDurationMs = 1500;
constexpr uint32_t kDashboardRefreshMs = 500;
constexpr uint32_t kSerialDiagIntervalMs = 1000;
constexpr uint32_t kDriveSerialDiagIntervalMs = 5000;
constexpr uint32_t kBatterySerialDiagIntervalMs = 30000;
constexpr uint32_t kSessionSerialDiagIntervalMs = 30000;
constexpr uint32_t kFixWaitDiagIntervalMs = 5000;
constexpr uint32_t kTransientMessageMs = 1500;
constexpr uint32_t kNewTileDisplayMs = 1500;
constexpr uint32_t kDriveResetConfirmMs = 3000;
constexpr int kLineHeight = 12;

M5Canvas& sharedCanvas()
{
    static M5Canvas canvas(&M5.Display);
    static bool ready = false;
    if (!ready) {
        canvas.createSprite(M5.Display.width(), M5.Display.height());
        ready = true;
    }
    return canvas;
}

void printKeyValue(uint8_t value)
{
    if (value >= 32 && value < 127) {
        Serial.printf("Key: '%c' (0x%02X)\n", static_cast<char>(value), value);
        return;
    }

    switch (value) {
        case KEY_TAB:
            Serial.println("Key: Tab (0x2B)");
            break;
        case KEY_ENTER:
            Serial.println("Key: Enter (0x28)");
            break;
        case KEY_BACKSPACE:
            Serial.println("Key: Backspace (0x2A)");
            break;
        case KEY_FN:
            Serial.println("Key: Fn (0xFF)");
            break;
        case KEY_LEFT_CTRL:
            Serial.println("Key: Ctrl (0x80)");
            break;
        case KEY_LEFT_SHIFT:
            Serial.println("Key: Shift (0x81)");
            break;
        case KEY_LEFT_ALT:
            Serial.println("Key: Alt (0x82)");
            break;
        default:
            Serial.printf("Key: 0x%02X\n", value);
            break;
    }
}

const char* sessionResultLabel(SessionTileResult result)
{
    switch (result) {
        case SessionTileResult::Added:
            return "ADDED";
        case SessionTileResult::AlreadyPresent:
            return "VISITED";
        case SessionTileResult::Full:
            return "FULL";
    }
    return "VISITED";
}

void formatDuration(uint32_t totalSeconds, char* buffer, size_t bufferSize)
{
    const uint32_t hours = totalSeconds / 3600U;
    const uint32_t minutes = (totalSeconds % 3600U) / 60U;
    const uint32_t seconds = totalSeconds % 60U;
    std::snprintf(buffer, bufferSize, "%u:%02u:%02u", hours, minutes, seconds);
}

}  // namespace

void App::setup()
{
    gps_.begin();
    sessionTileCache_.clear();
    driveSession_.begin(millis());
    batteryManager_.begin(millis());
    recordingSession_.begin(millis());

    if (storage_.begin()) {
        Serial.printf("EXPLORER mode=SD persistentTotal=%u\n", storage_.totalTiles());
    } else {
        Serial.println("EXPLORER mode=RAM persistentTotal=unavailable");
    }

    journeyLogger_.begin(storage_);
    photoLocationLogger_.begin(storage_);
    sdBrowser_.begin(storage_);

    drawSplash();
    splashStartMs_ = millis();
    lastSerialDiagMs_ = splashStartMs_;
    lastDriveSerialDiagMs_ = splashStartMs_;
    lastBatterySerialDiagMs_ = splashStartMs_;
    lastSessionSerialDiagMs_ = splashStartMs_;
    lastFixWaitDiagMs_ = splashStartMs_;
    transientMessage_[0] = '\0';
    transientMessageUntilMs_ = 0;
    wasAccurateFix_ = false;
    needsFreshReference_ = false;
    fixWaitEntryLogged_ = false;
}

void App::drawSplash()
{
    auto& display = M5.Display;

    display.fillScreen(Theme::kBackground);
    display.setTextDatum(middle_center);

    const int centerX = display.width() / 2;
    const int centerY = display.height() / 2;

    char versionLine[16];
    std::snprintf(versionLine, sizeof(versionLine), "v%s", ROADBOOK_VERSION);

    display.setTextSize(2);
    display.setTextColor(Theme::kTitle);
    display.drawString("RoadBook", centerX, centerY - 28);

    display.setTextSize(1);
    display.setTextColor(Theme::kPrimaryText);
    display.drawString(versionLine, centerX, centerY - 4);
    display.setTextColor(Theme::kSecondaryText);
    display.drawString("Drive - Explore - Capture", centerX, centerY + 12);
}

const char* App::gnssStatusLabel() const
{
    if (!gps_.hasReceivedData()) {
        return "STARTING";
    }
    if (!gps_.hasFix()) {
        return "SEARCH";
    }
    return "FIX";
}

const char* App::gnssFixQualityLabel() const
{
    if (!gps_.hasFix()) {
        return "NO";
    }
    if (gps_.hasAccurateFix()) {
        return "GOOD";
    }
    return "BASIC";
}

const char* App::explorerStateLabel() const
{
    if (recordingSession_.waitingForFix()) {
        return "WAIT FIX";
    }
    if (recordingSession_.paused()) {
        return "PAUSED";
    }
    if (!recordingSession_.recording()) {
        return "IDLE";
    }
    if (!gps_.hasFix() || !hasCurrentTile_) {
        return "WAITING";
    }
    if (lastSessionResult_ == SessionTileResult::Full) {
        return "CACHE FULL";
    }
    if (lastSessionResult_ == SessionTileResult::Added && (millis() - lastNewTileMs_) < kNewTileDisplayMs) {
        return "NEW";
    }
    if (lastSessionResult_ == SessionTileResult::Added ||
        lastSessionResult_ == SessionTileResult::AlreadyPresent) {
        return "VISITED";
    }
    return "WAITING";
}

const char* App::driveStatusLabel() const
{
    if (recordingSession_.waitingForFix()) {
        return "WAIT FIX";
    }
    if (recordingSession_.paused() || driveSession_.paused()) {
        return "PAUSED";
    }
    if (!recordingSession_.active()) {
        return "IDLE";
    }
    if (!driveSession_.started()) {
        return "WAITING";
    }
    if (gps_.hasFix() && gps_.speedValid() && gps_.speedKmph() >= 3.0) {
        return "DRIVING";
    }
    return "STOPPED";
}

const char* App::journeyStatusLabel() const
{
    if (recordingSession_.waitingForFix()) {
        return "WAIT FIX";
    }
    switch (journeyLogger_.status()) {
        case JourneyStatus::Disabled:
            return "DISABLED";
        case JourneyStatus::WaitingForTime:
            return "WAITING";
        case JourneyStatus::Recording:
            return "REC";
        case JourneyStatus::Paused:
            return "PAUSED";
        case JourneyStatus::StorageError:
            return "ERROR";
    }
    return "DISABLED";
}

const char* App::sessionStateLabel() const
{
    switch (recordingSession_.state()) {
        case RecordingSessionState::Idle:
            return "IDLE";
        case RecordingSessionState::WaitingForFix:
            return "WAIT FIX";
        case RecordingSessionState::Recording:
            return "REC";
        case RecordingSessionState::Paused:
            return "PAUSED";
        case RecordingSessionState::Stopped:
            return "STOPPED";
        case RecordingSessionState::StorageError:
            return "ERROR";
    }
    return "IDLE";
}

const char* App::journeyFileDisplayName() const
{
    const char* path = journeyLogger_.currentFilePath();
    if (path[0] == '\0') {
        return "--";
    }

    const char* slash = std::strrchr(path, '/');
    if (slash != nullptr && slash[1] != '\0') {
        return slash + 1;
    }

    return path;
}

bool App::sessionCollectingData() const
{
    return recordingSession_.recording() && gps_.hasAccurateFix();
}

uint8_t App::dashboardSubpageCount() const
{
    return 2;
}

void App::drawDashboardFooter(const char* hintA, const char* hintB, uint8_t subpage, uint8_t subpageCount)
{
    M5Canvas& canvas = sharedCanvas();
    char pageLabel[12];
    std::snprintf(pageLabel, sizeof(pageLabel), "%u/%u", subpage + 1, subpageCount);
    Theme::drawPageFooter(canvas, pageLabel, nullptr, nullptr);
    Theme::drawControlHint(canvas, Theme::kFooterTop + 12, hintA, hintB, millis());
}

void App::handleDashboardSubpageNav(int8_t direction)
{
    const uint8_t count = dashboardSubpageCount();
    if (direction < 0) {
        if (dashboardSubpage_ > 0) {
            --dashboardSubpage_;
        }
    } else if (direction > 0) {
        if (dashboardSubpage_ + 1 < count) {
            ++dashboardSubpage_;
        }
    }
    lastDashboardRefreshMs_ = 0;
}

void App::handlePhotoCapture(uint32_t nowMs)
{
    switch (photoLocationLogger_.capture(gps_, recordingSession_, sessionStateLabel())) {
        case PhotoCaptureResult::Saved:
            showTransientMessage("PHOTO SAVED", nowMs);
            sdBrowser_.requestRefresh();
            break;
        case PhotoCaptureResult::NoFix:
            showTransientMessage("PHOTO NO FIX", nowMs);
            break;
        case PhotoCaptureResult::SdError:
            showTransientMessage("PHOTO SD ERROR", nowMs);
            break;
        case PhotoCaptureResult::WriteError:
            showTransientMessage("PHOTO SAVE ERROR", nowMs);
            break;
    }
}

void App::drawGnssDashboard()
{
    M5Canvas& canvas = sharedCanvas();

    canvas.fillScreen(Theme::kBackground);
    canvas.setTextDatum(TL_DATUM);
    canvas.setTextSize(1);

    Theme::drawPageHeader(canvas, "GNSS");

    char line[48];
    char value[24];
    int y = Theme::kContentTop + 2;
    const int rowH = Theme::standardRowHeight(canvas);

    Theme::drawInfoCard(canvas, 2, Theme::kContentTop, Theme::kScreenWidth - 4,
                        Theme::kFooterTop - Theme::kContentTop - 2);

    if (dashboardSubpage_ == 0) {
        y += Theme::drawBadgeRow(canvas, y, "FIX:", gnssFixQualityLabel(),
                                 Theme::gnssFixBadgeColors(gps_.hasFix(), gps_.hasAccurateFix()));
        std::snprintf(value, sizeof(value), "%u", gps_.satellites());
        y += Theme::drawDataRow(canvas, y, "SAT:", value, Theme::satelliteCountColor(gps_.satellites()));
        if (gps_.hdopValid()) {
            std::snprintf(value, sizeof(value), "%.1f", gps_.hdop());
            y += Theme::drawDataRow(canvas, y, "HDOP:", value, Theme::hdopColor(true, gps_.hdop()));
        } else {
            y += Theme::drawDataRow(canvas, y, "HDOP:", "--", Theme::kSecondaryText);
        }
        if (gps_.hasFix()) {
            std::snprintf(value, sizeof(value), "%u", gps_.locationAgeMs());
            y += Theme::drawDataRow(canvas, y, "AGE:", value, Theme::ageColor(true, gps_.locationAgeMs()));
        } else {
            y += Theme::drawDataRow(canvas, y, "AGE:", "--", Theme::kSecondaryText);
        }
        if (gps_.hasFix()) {
            std::snprintf(value, sizeof(value), "%.6f", gps_.latitude());
            y += Theme::drawDataRow(canvas, y, "LAT:", value, Theme::kPrimaryText);
            std::snprintf(value, sizeof(value), "%.6f", gps_.longitude());
            y += Theme::drawDataRow(canvas, y, "LON:", value, Theme::kPrimaryText);
        } else {
            y += Theme::drawDataRow(canvas, y, "LAT:", "--", Theme::kSecondaryText);
            y += Theme::drawDataRow(canvas, y, "LON:", "--", Theme::kSecondaryText);
        }
    } else {
        if (gps_.hasFix()) {
            std::snprintf(value, sizeof(value), "%.1f m", gps_.altitudeMeters());
            y += Theme::drawDataRow(canvas, y, "ALT:", value, Theme::kPrimaryText);
            std::snprintf(value, sizeof(value), "%.1f", gps_.speedKmph());
            y += Theme::drawDataRow(canvas, y, "SPD:", value, Theme::kPrimaryText);
        } else {
            y += Theme::drawDataRow(canvas, y, "ALT:", "--", Theme::kSecondaryText);
            y += Theme::drawDataRow(canvas, y, "SPD:", "--", Theme::kSecondaryText);
        }
        if (gps_.dateValid() && gps_.timeValid()) {
            std::snprintf(line, sizeof(line), "%04u-%02u-%02u", gps_.year(), gps_.month(), gps_.day());
            y += Theme::drawDataRow(canvas, y, "DATE:", line, Theme::kPrimaryText);
            std::snprintf(line, sizeof(line), "%02u:%02u:%02u", gps_.hour(), gps_.minute(), gps_.second());
            y += Theme::drawDataRow(canvas, y, "UTC:", line, Theme::kPrimaryText);
        } else {
            y += Theme::drawDataRow(canvas, y, "DATE:", "--", Theme::kSecondaryText);
            y += Theme::drawDataRow(canvas, y, "UTC:", "--", Theme::kSecondaryText);
        }
        y += Theme::drawBadgeRow(canvas, y, "SESSION:", sessionStateLabel(),
                                 Theme::sessionStateBadgeColors(recordingSession_.state()));
    }
    (void)rowH;

    drawDashboardFooter("B Start E End N Photo", "; Prev  ' Next", dashboardSubpage_, dashboardSubpageCount());
    drawSessionOverlay();
    drawBatteryIndicator();
    canvas.pushSprite(0, 0);
}

void App::drawExplorerDashboard()
{
    M5Canvas& canvas = sharedCanvas();

    canvas.fillScreen(Theme::kBackground);
    canvas.setTextDatum(TL_DATUM);
    canvas.setTextSize(1);

    Theme::drawPageHeader(canvas, "Explorer");

    char line[48];
    char value[24];
    int y = Theme::kContentTop + 2;

    Theme::drawInfoCard(canvas, 2, Theme::kContentTop, Theme::kScreenWidth - 4,
                        Theme::kFooterTop - Theme::kContentTop - 2);

    if (dashboardSubpage_ == 0) {
        y += Theme::drawBadgeRow(canvas, y, "MODE:", storage_.ready() ? "SD" : "RAM",
                                 Theme::explorerModeBadgeColors(storage_.ready()));
        y += Theme::drawBadgeRow(canvas, y, "STATE:", explorerStateLabel(),
                                 Theme::explorerStateBadgeColors(explorerStateLabel()));
        if (hasCurrentTile_ && gps_.hasFix()) {
            std::snprintf(value, sizeof(value), "%ld", static_cast<long>(currentTile_.x));
            y += Theme::drawDataRow(canvas, y, "TILE X:", value, Theme::kPrimaryText);
            std::snprintf(value, sizeof(value), "%ld", static_cast<long>(currentTile_.y));
            y += Theme::drawDataRow(canvas, y, "TILE Y:", value, Theme::kPrimaryText);
        } else {
            y += Theme::drawDataRow(canvas, y, "TILE X:", "--", Theme::kSecondaryText);
            y += Theme::drawDataRow(canvas, y, "TILE Y:", "--", Theme::kSecondaryText);
        }
        std::snprintf(value, sizeof(value), "%u", sessionTileCache_.count());
        y += Theme::drawDataRow(canvas, y, "UNIQUE:", value, Theme::kInfo);
    } else {
        if (storage_.ready()) {
            std::snprintf(value, sizeof(value), "%u", storage_.totalTiles());
            y += Theme::drawDataRow(canvas, y, "TOTAL:", value, Theme::kPrimaryText);
        } else {
            y += Theme::drawDataRow(canvas, y, "TOTAL:", "--", Theme::kSecondaryText);
        }
        y += Theme::drawDataRow(canvas, y, "GPS:", gps_.hasFix() ? "FIX" : "SEARCH", Theme::kPrimaryText);
        y += Theme::drawDataRow(canvas, y, "GRID:", "100m", Theme::kSecondaryText);
        y += Theme::drawDataRow(canvas, y, "STORE:", storage_.ready() ? "SD OK" : "RAM", Theme::kPrimaryText);
        y += Theme::drawBadgeRow(canvas, y, "SESSION:", sessionStateLabel(),
                                 Theme::sessionStateBadgeColors(recordingSession_.state()));
    }

    drawDashboardFooter("B Start E End S Save", "Tab Page  ; ' Sub", dashboardSubpage_, dashboardSubpageCount());
    drawSessionOverlay();
    drawBatteryIndicator();
    canvas.pushSprite(0, 0);
}

void App::drawDriveDashboard()
{
    M5Canvas& canvas = sharedCanvas();
    const uint32_t now = millis();

    canvas.fillScreen(Theme::kBackground);
    canvas.setTextDatum(TL_DATUM);
    canvas.setTextSize(1);

    Theme::drawPageHeader(canvas, "Drive");

    char line[48];
    char value[24];
    char timeBuffer[16];
    int y = Theme::kContentTop + 2;

    Theme::drawInfoCard(canvas, 2, Theme::kContentTop, Theme::kScreenWidth - 4,
                        Theme::kFooterTop - Theme::kContentTop - 2);

    if (dashboardSubpage_ == 0) {
        std::snprintf(value, sizeof(value), "%.1f km", driveSession_.distanceKm());
        y += Theme::drawDataRow(canvas, y, "DIST:", value, Theme::kPrimaryText);
        formatDuration(driveSession_.elapsedSeconds(now), timeBuffer, sizeof(timeBuffer));
        y += Theme::drawDataRow(canvas, y, "TIME:", timeBuffer, Theme::kPrimaryText);
        formatDuration(driveSession_.movingSeconds(), timeBuffer, sizeof(timeBuffer));
        y += Theme::drawDataRow(canvas, y, "MOVING:", timeBuffer, Theme::kPrimaryText);
        if (gps_.speedValid()) {
            std::snprintf(value, sizeof(value), "%.1f", driveSession_.currentSpeedKmph());
            y += Theme::drawDataRow(canvas, y, "SPEED:", value,
                                    Theme::driveSpeedColor(true, driveSession_.currentSpeedKmph()));
        } else {
            y += Theme::drawDataRow(canvas, y, "SPEED:", "--", Theme::kSecondaryText);
        }
    } else {
        std::snprintf(value, sizeof(value), "%.1f", driveSession_.averageMovingSpeedKmph());
        y += Theme::drawDataRow(canvas, y, "AVG:", value, Theme::kPrimaryText);
        std::snprintf(value, sizeof(value), "%.1f", driveSession_.maxSpeedKmph());
        y += Theme::drawDataRow(canvas, y, "MAX:", value, Theme::kPrimaryText);
        std::snprintf(line, sizeof(line), "%u/%u", driveSession_.acceptedPointCount(),
                      driveSession_.rejectedPointCount());
        y += Theme::drawDataRow(canvas, y, "POINTS:", line, Theme::kPrimaryText);
        if (driveResetPending_ && (now - driveResetRequestMs_) <= kDriveResetConfirmMs) {
            y += Theme::drawDataRow(canvas, y, "RESET:", "Press X", Theme::kWarning);
        } else {
            y += Theme::drawBadgeRow(canvas, y, "STATUS:", driveStatusLabel(),
                                     Theme::driveStateBadgeColors(driveStatusLabel()));
        }
        y += Theme::drawBadgeRow(canvas, y, "SESSION:", sessionStateLabel(),
                                 Theme::sessionStateBadgeColors(recordingSession_.state()));
    }

    drawDashboardFooter("B Start P Pause E End", "; Prev  ' Next", dashboardSubpage_, dashboardSubpageCount());
    drawSessionOverlay();
    drawBatteryIndicator();
    canvas.pushSprite(0, 0);
}

void App::drawJourneyDashboard()
{
    M5Canvas& canvas = sharedCanvas();

    canvas.fillScreen(Theme::kBackground);
    canvas.setTextDatum(TL_DATUM);
    canvas.setTextSize(1);

    Theme::drawPageHeader(canvas, "Journey");

    char line[48];
    char value[24];
    int y = Theme::kContentTop + 2;

    Theme::drawInfoCard(canvas, 2, Theme::kContentTop, Theme::kScreenWidth - 4,
                        Theme::kFooterTop - Theme::kContentTop - 2);

    const bool sdReady = storage_.ready();
    const bool selfTestPassed = storage_.selfTestResult() == SdSelfTestResult::Passed;
    const char* statusLabel = journeyStatusLabel();

    if (dashboardSubpage_ == 0) {
        y += Theme::drawBadgeRow(canvas, y, "SD:", sdReady ? "READY" : "FAIL", Theme::sdReadyBadgeColors(sdReady));
        y += Theme::drawBadgeRow(canvas, y, "TEST:", selfTestPassed ? "PASS" : "FAIL",
                                 Theme::sdSelfTestBadgeColors(selfTestPassed));
        y += Theme::drawBadgeRow(canvas, y, "STATUS:", statusLabel, Theme::journeyStatusBadgeColors(statusLabel));
        Theme::truncateText(canvas, journeyFileDisplayName(), line, sizeof(line), 150);
        y += Theme::drawDataRow(canvas, y, "FILE:", line, Theme::kPrimaryText);
    } else {
        std::snprintf(value, sizeof(value), "%u", journeyLogger_.writtenPointCount());
        y += Theme::drawDataRow(canvas, y, "POINTS:", value, Theme::kPrimaryText);
        std::snprintf(value, sizeof(value), "%u", journeyLogger_.skippedPointCount());
        y += Theme::drawDataRow(canvas, y, "SKIP:", value, Theme::kPrimaryText);
        std::snprintf(value, sizeof(value), "%u", journeyLogger_.writeErrorCount());
        y += Theme::drawDataRow(canvas, y, "ERR:", value, Theme::kPrimaryText);
        y += Theme::drawDataRow(canvas, y, "RULE:", "10m/5s", Theme::kSecondaryText);
        y += Theme::drawBadgeRow(canvas, y, "SESSION:", sessionStateLabel(),
                                 Theme::sessionStateBadgeColors(recordingSession_.state()));
    }

    drawDashboardFooter("B Start S Save E End", "; Prev  ' Next", dashboardSubpage_, dashboardSubpageCount());
    drawSessionOverlay();
    drawBatteryIndicator();
    canvas.pushSprite(0, 0);
}

void App::drawSdBrowserDashboard()
{
    M5Canvas& canvas = sharedCanvas();

    canvas.fillScreen(Theme::kBackground);
    canvas.setTextDatum(TL_DATUM);
    canvas.setTextSize(1);

    Theme::drawPageHeader(canvas, "SD");

    char line[64];
    char pathLine[64];
    const int rowH = Theme::standardRowHeight(canvas);
    const int listStartY = Theme::kContentTop + rowH;
    const int listEndY = Theme::kFooterTop - 2;
    int y = Theme::kContentTop;

    if (!sdBrowser_.storageReady()) {
        const Theme::BadgeColors errColors = Theme::badgeColorsError();
        Theme::drawStatusBadge(canvas, Theme::kLabelX, y, "SD UNAVAILABLE", errColors.background, errColors.text,
                               errColors.border);
        drawDashboardFooter("Enter Open  Back Parent", "R Refresh  I Info", 0, 1);
        drawBatteryIndicator();
        canvas.pushSprite(0, 0);
        return;
    }

    const SdBrowserView view = sdBrowser_.view();

    if (view == SdBrowserView::SdInfo) {
        y += 2;
        Theme::drawInfoCard(canvas, 2, Theme::kContentTop, Theme::kScreenWidth - 4,
                            Theme::kFooterTop - Theme::kContentTop - 2);
        std::snprintf(line, sizeof(line), "%llu MB",
                      static_cast<unsigned long long>(sdBrowser_.sdCardSizeBytes() / (1024ULL * 1024ULL)));
        y += Theme::drawDataRow(canvas, y + 2, "CARD:", line, Theme::kPrimaryText);
        std::snprintf(line, sizeof(line), "%llu MB",
                      static_cast<unsigned long long>(sdBrowser_.sdTotalBytes() / (1024ULL * 1024ULL)));
        y += Theme::drawDataRow(canvas, y, "TOTAL:", line, Theme::kPrimaryText);
        std::snprintf(line, sizeof(line), "%llu MB",
                      static_cast<unsigned long long>(sdBrowser_.sdUsedBytes() / (1024ULL * 1024ULL)));
        y += Theme::drawDataRow(canvas, y, "USED:", line, Theme::kPrimaryText);
        const bool selfTestPassed = std::strcmp(sdBrowser_.selfTestLabel(), "PASS") == 0;
        y += Theme::drawBadgeRow(canvas, y, "TEST:", sdBrowser_.selfTestLabel(),
                                 Theme::sdSelfTestBadgeColors(selfTestPassed));
        std::snprintf(line, sizeof(line), "%u", sdBrowser_.journeyFileCount());
        y += Theme::drawDataRow(canvas, y, "JOURNEYS:", line, Theme::kPrimaryText);
        std::snprintf(line, sizeof(line), "%u", sdBrowser_.tileBucketFileCount());
        y += Theme::drawDataRow(canvas, y, "TILES:", line, Theme::kPrimaryText);
        drawDashboardFooter("Enter Open  Back Parent", "Backspace=back", 0, 1);
        drawBatteryIndicator();
        canvas.pushSprite(0, 0);
        return;
    }

    if (view == SdBrowserView::TextPreview) {
        Theme::truncateText(canvas, sdBrowser_.previewDisplayName(), line, sizeof(line), 220);
        canvas.setTextColor(Theme::kPrimaryText);
        canvas.drawString(line, Theme::kLabelX, y);
        y = listStartY;
        for (uint8_t index = 0; index < sdBrowser_.previewLineCount() && (y + rowH) <= listEndY; ++index) {
            std::snprintf(line, sizeof(line), "%02u %.34s", index + 1, sdBrowser_.previewLine(index));
            canvas.drawString(line, Theme::kLabelX, y);
            y += rowH;
        }
        const Theme::BadgeColors readOnlyColors = Theme::readOnlyBadgeColors();
        Theme::drawStatusBadge(canvas, Theme::kLabelX, Theme::kFooterTop, "READ ONLY", readOnlyColors.background,
                               readOnlyColors.text, readOnlyColors.border);
        std::snprintf(line, sizeof(line), "PAGE %u/%u", sdBrowser_.previewPage() + 1, sdBrowser_.previewTotalPages());
        Theme::drawPageFooter(canvas, line, nullptr, nullptr);
        drawDashboardFooter("Enter Open  Back Parent", "Backspace=back", 0, 1);
        drawBatteryIndicator();
        canvas.pushSprite(0, 0);
        return;
    }

    if (view == SdBrowserView::FileDetail) {
        Theme::truncateText(canvas, sdBrowser_.selectedName(), line, sizeof(line), 220);
        y += Theme::drawDataRow(canvas, y, "FILE:", line, Theme::kPrimaryText);
        std::snprintf(line, sizeof(line), "%llu B", static_cast<unsigned long long>(sdBrowser_.selectedSizeBytes()));
        y += Theme::drawDataRow(canvas, y, "SIZE:", line, Theme::kPrimaryText);
        y += Theme::drawDataRow(canvas, y, "TYPE:", sdBrowser_.selectedTypeLabel(), Theme::kPrimaryText);
        if (SdBrowser::endsWithIgnoreCase(sdBrowser_.selectedName(), ".csv") ||
            SdBrowser::endsWithIgnoreCase(sdBrowser_.selectedName(), ".txt") ||
            SdBrowser::endsWithIgnoreCase(sdBrowser_.selectedName(), ".log") ||
            SdBrowser::endsWithIgnoreCase(sdBrowser_.selectedName(), ".json") ||
            SdBrowser::endsWithIgnoreCase(sdBrowser_.selectedName(), ".gpx")) {
            y += Theme::drawDataRow(canvas, y, "PREVIEW:", "Enter", Theme::kSecondaryText);
        } else {
            y += Theme::drawDataRow(canvas, y, "PREVIEW:", "N/A", Theme::kSecondaryText);
        }
        drawDashboardFooter("Enter Open  Back Parent", "Backspace=back", 0, 1);
        drawBatteryIndicator();
        canvas.pushSprite(0, 0);
        return;
    }

    std::snprintf(pathLine, sizeof(pathLine), "PATH: %s", sdBrowser_.displayPath());
    Theme::truncateText(canvas, pathLine, line, sizeof(line), 228);
    canvas.setTextColor(Theme::kSecondaryText);
    canvas.drawString(line, Theme::kLabelX, y);

    y = listStartY;
    if (sdBrowser_.directoryState() == SdBrowserDirectoryState::OpenError ||
        sdBrowser_.directoryState() == SdBrowserDirectoryState::PathTooLong) {
        const Theme::BadgeColors errColors = Theme::badgeColorsError();
        const char* statusText = sdBrowser_.directoryStatusText();
        Theme::drawStatusBadge(canvas, Theme::kLabelX, y, statusText != nullptr ? statusText : "OPEN ERROR",
                               errColors.background, errColors.text, errColors.border);
    } else if (sdBrowser_.isEmpty()) {
        canvas.setTextColor(Theme::kSecondaryText);
        canvas.drawString("EMPTY FOLDER", Theme::kLabelX, y);
    } else {
        const uint8_t count = sdBrowser_.entryCount();
        for (uint8_t index = 0; index < count && (y + rowH) <= listEndY; ++index) {
            SdBrowserEntry item{};
            if (!sdBrowser_.getEntry(index, item)) {
                continue;
            }
            const bool selected = index == sdBrowser_.selectedIndex();
            if (selected) {
                canvas.fillRect(0, y - 1, canvas.width(), rowH, 0x2104);
            }
            const char prefix = selected ? '>' : ' ';
            canvas.setTextColor(Theme::sdBrowserEntryColor(item.isDirectory, selected));
            if (item.isDirectory) {
                Theme::truncateText(canvas, item.name, line, sizeof(line), 210);
                std::snprintf(pathLine, sizeof(pathLine), "%c %s/", prefix, line);
            } else {
                Theme::truncateText(canvas, item.name, line, sizeof(line), 210);
                std::snprintf(pathLine, sizeof(pathLine), "%c %s", prefix, line);
            }
            canvas.drawString(pathLine, Theme::kLabelX, y);
            y += rowH;
        }
    }

    char footerLeft[24];
    if (sdBrowser_.isEmpty()) {
        std::snprintf(footerLeft, sizeof(footerLeft), "EMPTY FOLDER");
    } else {
        std::snprintf(footerLeft, sizeof(footerLeft), "PAGE %u/%u", sdBrowser_.currentPage() + 1,
                      sdBrowser_.totalPages());
    }
    Theme::drawPageFooter(canvas, footerLeft, nullptr, nullptr);
    const Theme::BadgeColors sdReadyColors = Theme::badgeColorsSuccess();
    Theme::drawStatusBadge(canvas, Theme::kLabelX, Theme::kFooterTop, "SD READY", sdReadyColors.background,
                           sdReadyColors.text, sdReadyColors.border);
    drawDashboardFooter("Enter Open  Back Parent", "R Refresh  I Info", 0, 1);
    drawBatteryIndicator();
    canvas.pushSprite(0, 0);
}

void App::drawSessionOverlay()
{
    const uint32_t now = millis();
    if (transientMessage_[0] == '\0' || now >= transientMessageUntilMs_) {
        return;
    }

    M5Canvas& canvas = sharedCanvas();
    Theme::drawTransientBox(canvas, transientMessage_, Theme::transientMessageBadgeColors(transientMessage_));
}

void App::drawBatteryIndicator()
{
    M5Canvas& canvas = sharedCanvas();

    constexpr int kBodyWidth = 14;
    constexpr int kBodyHeight = 8;
    constexpr int kTerminalWidth = 2;
    constexpr int kMarginRight = 4;
    constexpr int kTop = 2;

    char label[8];
    if (batteryManager_.valid()) {
        std::snprintf(label, sizeof(label), "%ld%%", static_cast<long>(batteryManager_.levelPercent()));
    } else {
        std::snprintf(label, sizeof(label), "--%%");
    }

    canvas.setTextSize(1);
    canvas.setTextDatum(TR_DATUM);

    const int textX = canvas.width() - kMarginRight;
    const int textWidth = canvas.textWidth(label);
    const int iconX = textX - textWidth - kTerminalWidth - kBodyWidth - 3;

    const uint32_t color = Theme::batteryColor(batteryManager_.valid(), batteryManager_.levelPercent());

    canvas.drawRect(iconX, kTop, kBodyWidth, kBodyHeight, color);

    const int terminalHeight = kBodyHeight / 2;
    canvas.fillRect(iconX + kBodyWidth, kTop + (kBodyHeight - terminalHeight) / 2, kTerminalWidth, terminalHeight,
                    color);

    if (batteryManager_.valid()) {
        const int innerWidth = kBodyWidth - 4;
        const int innerHeight = kBodyHeight - 4;
        const int fillWidth = (innerWidth * static_cast<int>(batteryManager_.levelPercent())) / 100;
        if (fillWidth > 0) {
            canvas.fillRect(iconX + 2, kTop + 2, fillWidth, innerHeight, color);
        }
    }

    canvas.setTextColor(color);
    canvas.drawString(label, textX, kTop + 1);
    canvas.setTextColor(Theme::kPrimaryText);
    canvas.setTextDatum(TL_DATUM);

    lastDisplayedBatteryPercent_ = batteryManager_.valid() ? batteryManager_.levelPercent() : -1;
}

void App::printGnssDiagnostics()
{
    const char* status = gnssStatusLabel();

    if (gps_.hasFix()) {
        Serial.printf("GNSS chars=%u status=%s sat=%u lat=%.6f lon=%.6f speed=%.1f\n", gps_.charactersProcessed(),
                      status, gps_.satellites(), gps_.latitude(), gps_.longitude(), gps_.speedKmph());
        return;
    }

    Serial.printf("GNSS chars=%u status=%s sat=%u lat=-- lon=--\n", gps_.charactersProcessed(), status,
                  gps_.satellites());
}

void App::printDriveDiagnostics()
{
    Serial.printf("DRIVE status=%s distance=%.1f moving=%u avg=%.1f max=%.1f points=%u rejected=%u\n",
                  driveStatusLabel(), driveSession_.distanceKm(), driveSession_.movingSeconds(),
                  driveSession_.averageMovingSpeedKmph(), driveSession_.maxSpeedKmph(),
                  driveSession_.acceptedPointCount(), driveSession_.rejectedPointCount());
}

void App::printBatteryDiagnostics()
{
    if (batteryManager_.valid()) {
        Serial.printf("BAT level=%ld voltage=%d valid=1\n", static_cast<long>(batteryManager_.levelPercent()),
                      static_cast<int>(batteryManager_.voltageMillivolts()));
        return;
    }

    Serial.println("BAT level=-- voltage=-- valid=0");
}

void App::printSessionDiagnostics()
{
    Serial.printf("SESSION state=%s driveKm=%.1f explorerTiles=%u journeyPoints=%u\n", sessionStateLabel(),
                  driveSession_.distanceKm(), sessionTileCache_.count(), journeyLogger_.writtenPointCount());
}

void App::printFixWaitDiagnostics(uint32_t nowMs)
{
    if (!recordingSession_.waitingForFix()) {
        return;
    }

    if ((nowMs - lastFixWaitDiagMs_) < kFixWaitDiagIntervalMs) {
        return;
    }

    lastFixWaitDiagMs_ = nowMs;

    const double hdopValue = gps_.hdopValid() ? gps_.hdop() : 0.0;
    Serial.printf("FIXWAIT sat=%u hdop=%.1f age=%u valid=%d\n", gps_.satellites(), hdopValue, gps_.locationAgeMs(),
                  gps_.hasFix() ? 1 : 0);
}

void App::showTransientMessage(const char* message, uint32_t nowMs)
{
    std::snprintf(transientMessage_, sizeof(transientMessage_), "%s", message);
    transientMessageUntilMs_ = nowMs + kTransientMessageMs;
    lastDashboardRefreshMs_ = 0;
}

void App::handleSessionStart(uint32_t nowMs)
{
    driveSession_.reset(nowMs);
    sessionTileCache_.clear();
    hasLastProcessedTile_ = false;
    cacheFullLogged_ = false;
    lastSessionResult_ = SessionTileResult::AlreadyPresent;
    needsFreshReference_ = false;
    wasAccurateFix_ = gps_.hasAccurateFix();
    journeyLogger_.prepareForRecording();

    if (gps_.hasAccurateFix()) {
        if (!recordingSession_.startWaitingForFix(nowMs)) {
            return;
        }
        recordingSession_.promoteToRecording(nowMs);
        fixWaitEntryLogged_ = false;
        Serial.printf("SESSION recording started fix sat=%u hdop=%.1f\n", gps_.satellites(), gps_.hdop());
        showTransientMessage("SESSION STARTED", nowMs);
        return;
    }

    if (!recordingSession_.startWaitingForFix(nowMs)) {
        return;
    }

    fixWaitEntryLogged_ = true;
    lastFixWaitDiagMs_ = nowMs;
    Serial.println("SESSION waiting for accurate GNSS fix");
    Serial.printf("SESSION waiting fix sat=%u hdop=%.1f age=%u\n", gps_.satellites(),
                  gps_.hdopValid() ? gps_.hdop() : 0.0, gps_.locationAgeMs());
    showTransientMessage("WAIT FIX", nowMs);
}

void App::handleSessionPauseToggle(uint32_t nowMs)
{
    if (recordingSession_.waitingForFix()) {
        Serial.println("SESSION pause ignored while waiting for fix");
        return;
    }

    if (recordingSession_.paused()) {
        if (!recordingSession_.resume(nowMs)) {
            return;
        }
        driveSession_.resume(nowMs);
        journeyLogger_.resume();
        Serial.println("SESSION resumed");
        showTransientMessage("RESUMED", nowMs);
        return;
    }

    if (!recordingSession_.recording()) {
        return;
    }

    if (!recordingSession_.pause(nowMs)) {
        return;
    }
    driveSession_.pause();
    journeyLogger_.pause();
    Serial.println("SESSION paused");
    showTransientMessage("PAUSED", nowMs);
}

void App::handleSessionStop(uint32_t nowMs)
{
    if (recordingSession_.waitingForFix()) {
        if (!recordingSession_.cancelWaiting(nowMs)) {
            return;
        }
        fixWaitEntryLogged_ = false;
        Serial.println("SESSION stopped");
        showTransientMessage("SESSION STOPPED", nowMs);
        return;
    }

    if (!recordingSession_.stop(nowMs)) {
        return;
    }

    journeyLogger_.stop();
    storage_.flushPersistentState();
    driveSession_.pause();
    fixWaitEntryLogged_ = false;

    Serial.println("SESSION stopped");
    showTransientMessage("SESSION STOPPED", nowMs);
}

void App::handleManualSave()
{
    bool saveOk = true;
    uint64_t journeySizeBytes = 0;

    if (recordingSession_.active()) {
        if (!journeyLogger_.flush()) {
            saveOk = false;
        }
    }

    if (!storage_.flushPersistentState()) {
        saveOk = false;
    }

    if (saveOk && !journeyLogger_.verifyOpenFile(journeySizeBytes)) {
        saveOk = false;
    }

    if (saveOk) {
        Serial.printf("SESSION manual save result=OK journeySize=%llu\n",
                      static_cast<unsigned long long>(journeySizeBytes));
        char message[32];
        std::snprintf(message, sizeof(message), "SAVED %lluB", static_cast<unsigned long long>(journeySizeBytes));
        showTransientMessage(message, millis());
        sdBrowser_.requestRefresh();
    } else {
        Serial.println("SESSION manual save result=FAIL");
        showTransientMessage("SAVE ERROR", millis());
        recordingSession_.setStorageError();
    }
}

void App::updateRecordingFixGate(uint32_t nowMs)
{
    if (recordingSession_.waitingForFix() && gps_.hasAccurateFix()) {
        if (recordingSession_.promoteToRecording(nowMs)) {
            wasAccurateFix_ = true;
            needsFreshReference_ = false;
            fixWaitEntryLogged_ = false;
            Serial.printf("SESSION recording started fix sat=%u hdop=%.1f\n", gps_.satellites(), gps_.hdop());
            Serial.printf("FIXWAIT completed sat=%u hdop=%.1f\n", gps_.satellites(), gps_.hdop());
            lastDashboardRefreshMs_ = 0;
        }
    }

    printFixWaitDiagnostics(nowMs);

    const bool accurate = gps_.hasAccurateFix();

    if (recordingSession_.recording()) {
        if (wasAccurateFix_ && !accurate) {
            needsFreshReference_ = true;
        }

        if (accurate && needsFreshReference_) {
            driveSession_.resetReferencePoint(gps_.latitude(), gps_.longitude(), nowMs);
            journeyLogger_.resetPositionReference();
            needsFreshReference_ = false;
        }
    }

    wasAccurateFix_ = accurate;
}

void App::updateDriveSession(uint32_t nowMs)
{
    driveSession_.update(gps_.hasFix(), gps_.latitude(), gps_.longitude(),
                         gps_.speedValid() ? gps_.speedKmph() : 0.0, gps_.locationAgeMs(), nowMs,
                         sessionCollectingData());
}

void App::updateJourneyLogger(uint32_t nowMs)
{
    const double speedKmph = gps_.speedValid() ? gps_.speedKmph() : NAN;

    journeyLogger_.update(gps_.hasFix(), gps_.dateValid(), gps_.timeValid(), gps_.latitude(), gps_.longitude(),
                          gps_.altitudeMeters(), speedKmph, gps_.satellites(), gps_.locationAgeMs(), gps_.year(),
                          gps_.month(), gps_.day(), gps_.hour(), gps_.minute(), gps_.second(), nowMs,
                          recordingSession_.recording(), gps_.hasAccurateFix());

    if (journeyLogger_.status() == JourneyStatus::StorageError &&
        recordingSession_.state() != RecordingSessionState::StorageError) {
        recordingSession_.setStorageError();
    }
}

void App::processExplorer()
{
    if (gps_.hasFix()) {
        currentTile_ = grid_.calculateTile(gps_.latitude(), gps_.longitude());
        hasCurrentTile_ = true;
    }

    if (!sessionCollectingData()) {
        return;
    }

    if (!gps_.hasFix()) {
        return;
    }

    if (hasLastProcessedTile_ && grid_.sameTile(currentTile_, lastProcessedTile_)) {
        return;
    }

    lastProcessedTile_ = currentTile_;
    hasLastProcessedTile_ = true;

    lastSessionResult_ = sessionTileCache_.add(currentTile_.x, currentTile_.y);

    if (lastSessionResult_ == SessionTileResult::Added) {
        lastNewTileMs_ = millis();
    }

    Serial.printf("EXPLORER tile x=%ld y=%ld sessionResult=%s sessionUnique=%u\n", static_cast<long>(currentTile_.x),
                  static_cast<long>(currentTile_.y), sessionResultLabel(lastSessionResult_),
                  sessionTileCache_.count());

    if (lastSessionResult_ == SessionTileResult::Full && !cacheFullLogged_) {
        Serial.printf("EXPLORER cache full capacity=%u\n", sessionTileCache_.capacity());
        cacheFullLogged_ = true;
    }

    if (storage_.ready()) {
        storage_.markVisited(currentTile_);
    }
}

void App::handleKeyboard()
{
    M5Cardputer.update();

    if (!M5Cardputer.Keyboard.isChange() || !M5Cardputer.Keyboard.isPressed()) {
        return;
    }

    if (M5Cardputer.Keyboard.keysState().tab) {
        dashboardSubpage_ = 0;
        if (state_ == AppState::GnssDashboard) {
            state_ = AppState::ExplorerDashboard;
            lastDashboardRefreshMs_ = 0;
        } else if (state_ == AppState::ExplorerDashboard) {
            state_ = AppState::DriveDashboard;
            lastDashboardRefreshMs_ = 0;
        } else if (state_ == AppState::DriveDashboard) {
            state_ = AppState::JourneyDashboard;
            lastDashboardRefreshMs_ = 0;
        } else if (state_ == AppState::JourneyDashboard) {
            state_ = AppState::SdBrowserDashboard;
            lastDashboardRefreshMs_ = 0;
            sdBrowser_.setStorageReady(storage_.ready());
            sdBrowser_.refresh();
        } else if (state_ == AppState::SdBrowserDashboard) {
            state_ = AppState::GnssDashboard;
            lastDashboardRefreshMs_ = 0;
        }
    }

    if (state_ == AppState::GnssDashboard || state_ == AppState::ExplorerDashboard ||
        state_ == AppState::DriveDashboard || state_ == AppState::JourneyDashboard) {
        const uint32_t now = millis();

        for (const auto& keyPos : M5Cardputer.Keyboard.keyList()) {
            const uint8_t keyValue = M5Cardputer.Keyboard.getKey(keyPos);

            if (keyValue == 'n' || keyValue == 'N') {
                handlePhotoCapture(now);
            }

            if (keyValue == ';') {
                handleDashboardSubpageNav(-1);
            }
            if (keyValue == '\'') {
                handleDashboardSubpageNav(1);
            }
        }

        for (uint8_t hidKey : M5Cardputer.Keyboard.keysState().hid_keys) {
            if (hidKey == 0x50) {
                handleDashboardSubpageNav(-1);
            } else if (hidKey == 0x4F) {
                handleDashboardSubpageNav(1);
            }
        }
    }

    if (state_ == AppState::GnssDashboard || state_ == AppState::ExplorerDashboard ||
        state_ == AppState::DriveDashboard || state_ == AppState::JourneyDashboard ||
        state_ == AppState::SdBrowserDashboard) {
        const uint32_t now = millis();

        for (const auto& keyPos : M5Cardputer.Keyboard.keyList()) {
            const uint8_t keyValue = M5Cardputer.Keyboard.getKey(keyPos);

            if (keyValue == 'b' || keyValue == 'B') {
                handleSessionStart(now);
            }

            if (keyValue == 'p' || keyValue == 'P') {
                handleSessionPauseToggle(now);
            }

            if (keyValue == 'e' || keyValue == 'E') {
                handleSessionStop(now);
            }

            if (keyValue == 's' || keyValue == 'S') {
                handleManualSave();
            }

            if (keyValue == 'r' || keyValue == 'R') {
                if (!storage_.ready()) {
                    const bool sdReady = storage_.retry();
                    if (sdReady) {
                        Serial.printf("SD retry OK frequency=%u persistentTotal=%u\n",
                                      storage_.lastRetryFrequencyHz(), storage_.totalTiles());
                    } else {
                        Serial.println("SD retry FAIL");
                    }
                    journeyLogger_.begin(storage_);
                    photoLocationLogger_.begin(storage_);
                    sdBrowser_.setStorageReady(sdReady);
                    sdBrowser_.requestRefresh();
                    lastDashboardRefreshMs_ = 0;
                }
            }

            if (keyValue == 'd' || keyValue == 'D') {
                storage_.printDiagnostics();
            }
        }
    }

    if (state_ == AppState::SdBrowserDashboard) {
        handleSdBrowserKeyboard();
    }

    if (state_ == AppState::DriveDashboard) {
        const uint32_t now = millis();

        for (const auto& keyPos : M5Cardputer.Keyboard.keyList()) {
            const uint8_t keyValue = M5Cardputer.Keyboard.getKey(keyPos);

            if (keyValue == 'x' || keyValue == 'X') {
                if (recordingSession_.active() || recordingSession_.waitingForFix()) {
                    Serial.println("DRIVE reset blocked while session active");
                    driveResetPending_ = false;
                    continue;
                }

                if (driveResetPending_ && (now - driveResetRequestMs_) <= kDriveResetConfirmMs) {
                    driveSession_.reset(now);
                    driveResetPending_ = false;
                } else {
                    driveResetPending_ = true;
                    driveResetRequestMs_ = now;
                }
            }
        }
    }

    for (const auto& keyPos : M5Cardputer.Keyboard.keyList()) {
        printKeyValue(M5Cardputer.Keyboard.getKey(keyPos));
    }
}

void App::handleSdBrowserKeyboard()
{
    if (M5Cardputer.Keyboard.keysState().del) {
        if (sdBrowser_.view() == SdBrowserView::TextPreview) {
            sdBrowser_.backFromPreview();
        } else {
            sdBrowser_.goParent();
        }
        lastDashboardRefreshMs_ = 0;
    }

    const bool fnHeld = M5Cardputer.Keyboard.keysState().fn;
    const bool inTextPreview = sdBrowser_.previewMode();

    for (const auto& keyPos : M5Cardputer.Keyboard.keyList()) {
        const uint8_t keyValue = M5Cardputer.Keyboard.getKey(keyPos);

        if ((keyValue == 'n' || keyValue == 'N') && !inTextPreview) {
            handlePhotoCapture(millis());
            continue;
        }

        if (keyValue == KEY_BACKSPACE) {
            if (sdBrowser_.view() == SdBrowserView::TextPreview) {
                sdBrowser_.backFromPreview();
            } else {
                sdBrowser_.goParent();
            }
            lastDashboardRefreshMs_ = 0;
            continue;
        }

        if (keyValue == KEY_ENTER) {
            sdBrowser_.enterSelection();
            lastDashboardRefreshMs_ = 0;
            continue;
        }

        if (fnHeld && keyValue == ';') {
            sdBrowser_.moveSelectionUp();
            lastDashboardRefreshMs_ = 0;
            continue;
        }
        if (fnHeld && keyValue == '\'') {
            sdBrowser_.moveSelectionDown();
            lastDashboardRefreshMs_ = 0;
            continue;
        }
        if (fnHeld && keyValue == ',') {
            sdBrowser_.previousPage();
            lastDashboardRefreshMs_ = 0;
            continue;
        }
        if (fnHeld && keyValue == '/') {
            sdBrowser_.nextPage();
            lastDashboardRefreshMs_ = 0;
            continue;
        }

        if (keyValue == ',') {
            sdBrowser_.moveSelectionUp();
            lastDashboardRefreshMs_ = 0;
            continue;
        }
        if (keyValue == '/') {
            sdBrowser_.moveSelectionDown();
            lastDashboardRefreshMs_ = 0;
            continue;
        }
        if (keyValue == ';') {
            sdBrowser_.previousPage();
            lastDashboardRefreshMs_ = 0;
            continue;
        }
        if (keyValue == '\'') {
            sdBrowser_.nextPage();
            lastDashboardRefreshMs_ = 0;
            continue;
        }

        if (keyValue == 'r' || keyValue == 'R') {
            sdBrowser_.refresh();
            lastDashboardRefreshMs_ = 0;
            continue;
        }

        if (keyValue == 'i' || keyValue == 'I') {
            sdBrowser_.showSdInfo();
            lastDashboardRefreshMs_ = 0;
            continue;
        }
    }

    for (uint8_t hidKey : M5Cardputer.Keyboard.keysState().hid_keys) {
        if (hidKey == 0x52) {
            sdBrowser_.moveSelectionUp();
            lastDashboardRefreshMs_ = 0;
        } else if (hidKey == 0x51) {
            sdBrowser_.moveSelectionDown();
            lastDashboardRefreshMs_ = 0;
        } else if (hidKey == 0x50) {
            sdBrowser_.previousPage();
            lastDashboardRefreshMs_ = 0;
        } else if (hidKey == 0x4F) {
            sdBrowser_.nextPage();
            lastDashboardRefreshMs_ = 0;
        }
    }
}

void App::loop()
{
    const uint32_t now = millis();

    if (driveResetPending_ && (now - driveResetRequestMs_) > kDriveResetConfirmMs) {
        driveResetPending_ = false;
    }

    if (transientMessage_[0] != '\0' && now >= transientMessageUntilMs_) {
        transientMessage_[0] = '\0';
        lastDashboardRefreshMs_ = 0;
    }

    handleKeyboard();
    gps_.update();
    batteryManager_.update(now);
    updateRecordingFixGate(now);
    processExplorer();
    updateDriveSession(now);
    updateJourneyLogger(now);

    sdBrowser_.setStorageReady(storage_.ready());

    if (state_ == AppState::GnssDashboard || state_ == AppState::ExplorerDashboard ||
        state_ == AppState::DriveDashboard || state_ == AppState::JourneyDashboard ||
        state_ == AppState::SdBrowserDashboard) {
        const int32_t displayLevel = batteryManager_.valid() ? batteryManager_.levelPercent() : -1;
        if (displayLevel != lastDisplayedBatteryPercent_) {
            lastDashboardRefreshMs_ = 0;
        }
    }

    if (state_ == AppState::Splash && (now - splashStartMs_) >= kSplashDurationMs) {
        state_ = AppState::GnssDashboard;
        lastDashboardRefreshMs_ = now;
        drawGnssDashboard();
    }

    if (state_ == AppState::SdBrowserDashboard && sdBrowser_.needsRefresh()) {
        sdBrowser_.refresh();
        sdBrowser_.clearNeedsRefresh();
        lastDashboardRefreshMs_ = 0;
    }

    if ((state_ == AppState::GnssDashboard || state_ == AppState::ExplorerDashboard ||
         state_ == AppState::DriveDashboard || state_ == AppState::JourneyDashboard ||
         state_ == AppState::SdBrowserDashboard) &&
        (now - lastDashboardRefreshMs_) >= kDashboardRefreshMs) {
        lastDashboardRefreshMs_ = now;
        if (state_ == AppState::GnssDashboard) {
            drawGnssDashboard();
        } else if (state_ == AppState::ExplorerDashboard) {
            drawExplorerDashboard();
        } else if (state_ == AppState::DriveDashboard) {
            drawDriveDashboard();
        } else if (state_ == AppState::JourneyDashboard) {
            drawJourneyDashboard();
        } else {
            drawSdBrowserDashboard();
        }
    }

    if ((now - lastSerialDiagMs_) >= kSerialDiagIntervalMs) {
        lastSerialDiagMs_ = now;
        printGnssDiagnostics();
    }

    if ((now - lastDriveSerialDiagMs_) >= kDriveSerialDiagIntervalMs) {
        lastDriveSerialDiagMs_ = now;
        printDriveDiagnostics();
    }

    if ((now - lastBatterySerialDiagMs_) >= kBatterySerialDiagIntervalMs) {
        lastBatterySerialDiagMs_ = now;
        printBatteryDiagnostics();
    }

    if ((now - lastSessionSerialDiagMs_) >= kSessionSerialDiagIntervalMs) {
        lastSessionSerialDiagMs_ = now;
        printSessionDiagnostics();
    }
}
