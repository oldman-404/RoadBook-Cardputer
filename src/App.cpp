#include "App.h"

#include <M5Cardputer.h>
#include <M5Unified.h>

#include <cstdio>

namespace {

constexpr const char* kVersion = "0.0.1";
constexpr uint32_t kSplashDurationMs = 1500;
constexpr uint32_t kDashboardRefreshMs = 500;
constexpr uint32_t kSerialDiagIntervalMs = 1000;
constexpr uint32_t kDriveSerialDiagIntervalMs = 5000;
constexpr uint32_t kBatterySerialDiagIntervalMs = 30000;
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

    if (storage_.begin()) {
        Serial.printf("EXPLORER mode=SD persistentTotal=%u\n", storage_.totalTiles());
    } else {
        Serial.println("EXPLORER mode=RAM persistentTotal=unavailable");
    }

    drawSplash();
    splashStartMs_ = millis();
    lastSerialDiagMs_ = splashStartMs_;
    lastDriveSerialDiagMs_ = splashStartMs_;
    lastBatterySerialDiagMs_ = splashStartMs_;
}

void App::drawSplash()
{
    auto& display = M5.Display;

    display.fillScreen(TFT_BLACK);
    display.setTextColor(TFT_WHITE);
    display.setTextDatum(middle_center);

    const int centerX = display.width() / 2;
    const int centerY = display.height() / 2;

    char versionLine[32];
    std::snprintf(versionLine, sizeof(versionLine), "Version %s", kVersion);

    display.setTextSize(2);
    display.drawString("RoadBook", centerX, centerY - 24);

    display.setTextSize(1);
    display.drawString(versionLine, centerX, centerY);
    display.drawString("Hello Explorer", centerX, centerY + 16);
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

const char* App::explorerStateLabel() const
{
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
    if (driveSession_.paused()) {
        return "PAUSED";
    }
    if (!driveSession_.started()) {
        return "WAITING";
    }
    if (gps_.hasFix() && gps_.speedValid() && gps_.speedKmph() >= 3.0) {
        return "DRIVING";
    }
    return "STOPPED";
}

void App::drawGnssDashboard()
{
    M5Canvas& canvas = sharedCanvas();

    canvas.fillScreen(TFT_BLACK);
    canvas.setTextColor(TFT_WHITE);
    canvas.setTextDatum(TL_DATUM);
    canvas.setTextSize(1);

    char line[48];
    int y = 4;

    canvas.drawString("RoadBook GNSS", 4, y);
    y += kLineHeight;

    std::snprintf(line, sizeof(line), "Status: %s", gnssStatusLabel());
    canvas.drawString(line, 4, y);
    y += kLineHeight;

    std::snprintf(line, sizeof(line), "SAT: %u", gps_.satellites());
    canvas.drawString(line, 4, y);
    y += kLineHeight;

    if (gps_.hasFix()) {
        std::snprintf(line, sizeof(line), "SPD: %.1f km/h", gps_.speedKmph());
    } else {
        std::snprintf(line, sizeof(line), "SPD: --");
    }
    canvas.drawString(line, 4, y);
    y += kLineHeight;

    if (gps_.hasFix()) {
        std::snprintf(line, sizeof(line), "LAT: %.6f", gps_.latitude());
    } else {
        std::snprintf(line, sizeof(line), "LAT: --");
    }
    canvas.drawString(line, 4, y);
    y += kLineHeight;

    if (gps_.hasFix()) {
        std::snprintf(line, sizeof(line), "LON: %.6f", gps_.longitude());
    } else {
        std::snprintf(line, sizeof(line), "LON: --");
    }
    canvas.drawString(line, 4, y);
    y += kLineHeight;

    if (gps_.hasFix()) {
        std::snprintf(line, sizeof(line), "ALT: %.1f m", gps_.altitudeMeters());
    } else {
        std::snprintf(line, sizeof(line), "ALT: --");
    }
    canvas.drawString(line, 4, y);
    y += kLineHeight;

    if (gps_.dateValid() && gps_.timeValid()) {
        std::snprintf(line, sizeof(line), "UTC: %04u-%02u-%02u %02u:%02u:%02u", gps_.year(), gps_.month(),
                      gps_.day(), gps_.hour(), gps_.minute(), gps_.second());
    } else {
        std::snprintf(line, sizeof(line), "UTC: --");
    }
    canvas.drawString(line, 4, y);

    drawBatteryIndicator();
    canvas.pushSprite(0, 0);
}

void App::drawExplorerDashboard()
{
    M5Canvas& canvas = sharedCanvas();

    canvas.fillScreen(TFT_BLACK);
    canvas.setTextColor(TFT_WHITE);
    canvas.setTextDatum(TL_DATUM);
    canvas.setTextSize(1);

    char line[48];
    int y = 4;

    canvas.drawString("RoadBook Explorer", 4, y);
    y += kLineHeight;

    std::snprintf(line, sizeof(line), "GPS: %s", gps_.hasFix() ? "FIX" : "SEARCH");
    canvas.drawString(line, 4, y);
    y += kLineHeight;

    std::snprintf(line, sizeof(line), "MODE: %s", storage_.ready() ? "SD" : "RAM");
    canvas.drawString(line, 4, y);
    y += kLineHeight;

    canvas.drawString("GRID: 100m", 4, y);
    y += kLineHeight;

    if (hasCurrentTile_ && gps_.hasFix()) {
        std::snprintf(line, sizeof(line), "TILE X: %ld", static_cast<long>(currentTile_.x));
    } else {
        std::snprintf(line, sizeof(line), "TILE X: --");
    }
    canvas.drawString(line, 4, y);
    y += kLineHeight;

    if (hasCurrentTile_ && gps_.hasFix()) {
        std::snprintf(line, sizeof(line), "TILE Y: %ld", static_cast<long>(currentTile_.y));
    } else {
        std::snprintf(line, sizeof(line), "TILE Y: --");
    }
    canvas.drawString(line, 4, y);
    y += kLineHeight;

    std::snprintf(line, sizeof(line), "SESSION UNIQUE: %u", sessionTileCache_.count());
    canvas.drawString(line, 4, y);
    y += kLineHeight;

    if (storage_.ready()) {
        std::snprintf(line, sizeof(line), "TOTAL: %u", storage_.totalTiles());
    } else {
        std::snprintf(line, sizeof(line), "TOTAL: --");
    }
    canvas.drawString(line, 4, y);
    y += kLineHeight;

    std::snprintf(line, sizeof(line), "STATE: %s", explorerStateLabel());
    canvas.drawString(line, 4, y);

    drawBatteryIndicator();
    canvas.pushSprite(0, 0);
}

void App::drawDriveDashboard()
{
    M5Canvas& canvas = sharedCanvas();
    const uint32_t now = millis();

    canvas.fillScreen(TFT_BLACK);
    canvas.setTextColor(TFT_WHITE);
    canvas.setTextDatum(TL_DATUM);
    canvas.setTextSize(1);

    char line[48];
    char timeBuffer[16];
    int y = 4;

    canvas.drawString("RoadBook Drive", 4, y);
    y += kLineHeight;

    if (driveResetPending_ && (now - driveResetRequestMs_) <= kDriveResetConfirmMs) {
        canvas.drawString("Press X again to reset", 4, y);
    } else {
        std::snprintf(line, sizeof(line), "STATUS: %s", driveStatusLabel());
        canvas.drawString(line, 4, y);
    }
    y += kLineHeight;

    std::snprintf(line, sizeof(line), "DIST: %.1f km", driveSession_.distanceKm());
    canvas.drawString(line, 4, y);
    y += kLineHeight;

    formatDuration(driveSession_.elapsedSeconds(now), timeBuffer, sizeof(timeBuffer));
    std::snprintf(line, sizeof(line), "TIME: %s", timeBuffer);
    canvas.drawString(line, 4, y);
    y += kLineHeight;

    formatDuration(driveSession_.movingSeconds(), timeBuffer, sizeof(timeBuffer));
    std::snprintf(line, sizeof(line), "MOVING: %s", timeBuffer);
    canvas.drawString(line, 4, y);
    y += kLineHeight;

    if (gps_.speedValid()) {
        std::snprintf(line, sizeof(line), "SPEED: %.1f km/h", driveSession_.currentSpeedKmph());
    } else {
        std::snprintf(line, sizeof(line), "SPEED: --");
    }
    canvas.drawString(line, 4, y);
    y += kLineHeight;

    std::snprintf(line, sizeof(line), "AVG: %.1f km/h", driveSession_.averageMovingSpeedKmph());
    canvas.drawString(line, 4, y);
    y += kLineHeight;

    std::snprintf(line, sizeof(line), "MAX: %.1f km/h", driveSession_.maxSpeedKmph());
    canvas.drawString(line, 4, y);
    y += kLineHeight;

    std::snprintf(line, sizeof(line), "POINTS: %u/%u", driveSession_.acceptedPointCount(),
                  driveSession_.rejectedPointCount());
    canvas.drawString(line, 4, y);

    drawBatteryIndicator();
    canvas.pushSprite(0, 0);
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

    uint32_t color = TFT_WHITE;
    if (batteryManager_.valid()) {
        const int32_t level = batteryManager_.levelPercent();
        if (level <= 10) {
            color = TFT_RED;
        } else if (level <= 20) {
            color = TFT_YELLOW;
        }
    }

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
    canvas.setTextColor(TFT_WHITE);
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

void App::updateDriveSession(uint32_t nowMs)
{
    driveSession_.update(gps_.hasFix(), gps_.latitude(), gps_.longitude(),
                         gps_.speedValid() ? gps_.speedKmph() : 0.0, gps_.locationAgeMs(), nowMs);
}

void App::processExplorer()
{
    if (!gps_.hasFix()) {
        return;
    }

    currentTile_ = grid_.calculateTile(gps_.latitude(), gps_.longitude());
    hasCurrentTile_ = true;

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
        if (state_ == AppState::GnssDashboard) {
            state_ = AppState::ExplorerDashboard;
            lastDashboardRefreshMs_ = 0;
        } else if (state_ == AppState::ExplorerDashboard) {
            state_ = AppState::DriveDashboard;
            lastDashboardRefreshMs_ = 0;
        } else if (state_ == AppState::DriveDashboard) {
            state_ = AppState::GnssDashboard;
            lastDashboardRefreshMs_ = 0;
        }
    }

    if (state_ == AppState::GnssDashboard || state_ == AppState::ExplorerDashboard ||
        state_ == AppState::DriveDashboard) {
        for (const auto& keyPos : M5Cardputer.Keyboard.keyList()) {
            const uint8_t keyValue = M5Cardputer.Keyboard.getKey(keyPos);

            if (keyValue == 'd' || keyValue == 'D') {
                storage_.printDiagnostics();
            }

            if (keyValue == 's' || keyValue == 'S') {
                const bool sdReady = storage_.retry();
                if (sdReady) {
                    Serial.printf("SD retry OK frequency=%u persistentTotal=%u\n",
                                  storage_.lastRetryFrequencyHz(), storage_.totalTiles());
                } else {
                    Serial.println("SD retry FAIL");
                }
                lastDashboardRefreshMs_ = 0;
            }
        }
    }

    if (state_ == AppState::DriveDashboard) {
        const uint32_t now = millis();

        for (const auto& keyPos : M5Cardputer.Keyboard.keyList()) {
            const uint8_t keyValue = M5Cardputer.Keyboard.getKey(keyPos);

            if (keyValue == 'p' || keyValue == 'P') {
                if (driveSession_.paused()) {
                    driveSession_.resume(now);
                } else {
                    driveSession_.pause();
                }
            }

            if (keyValue == 'x' || keyValue == 'X') {
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

void App::loop()
{
    const uint32_t now = millis();

    if (driveResetPending_ && (now - driveResetRequestMs_) > kDriveResetConfirmMs) {
        driveResetPending_ = false;
    }

    handleKeyboard();
    gps_.update();
    batteryManager_.update(now);
    processExplorer();
    updateDriveSession(now);

    if (state_ == AppState::GnssDashboard || state_ == AppState::ExplorerDashboard ||
        state_ == AppState::DriveDashboard) {
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

    if ((state_ == AppState::GnssDashboard || state_ == AppState::ExplorerDashboard ||
         state_ == AppState::DriveDashboard) &&
        (now - lastDashboardRefreshMs_) >= kDashboardRefreshMs) {
        lastDashboardRefreshMs_ = now;
        if (state_ == AppState::GnssDashboard) {
            drawGnssDashboard();
        } else if (state_ == AppState::ExplorerDashboard) {
            drawExplorerDashboard();
        } else {
            drawDriveDashboard();
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
}
