#include "App.h"

#include <M5Cardputer.h>
#include <M5Unified.h>

#include <cstdio>

namespace {

constexpr const char* kVersion = "0.0.1";
constexpr uint32_t kSplashDurationMs = 1500;
constexpr uint32_t kDashboardRefreshMs = 500;
constexpr uint32_t kSerialDiagIntervalMs = 1000;
constexpr int kLineHeight = 12;

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

}  // namespace

void App::setup()
{
    gps_.begin();
    drawSplash();
    splashStartMs_ = millis();
    lastSerialDiagMs_ = splashStartMs_;
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

void App::drawGnssDashboard()
{
    static M5Canvas canvas(&M5.Display);
    static bool canvasReady = false;

    if (!canvasReady) {
        canvas.createSprite(M5.Display.width(), M5.Display.height());
        canvasReady = true;
    }

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

    canvas.pushSprite(0, 0);
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

void App::handleKeyboard()
{
    M5Cardputer.update();

    if (!M5Cardputer.Keyboard.isChange() || !M5Cardputer.Keyboard.isPressed()) {
        return;
    }

    for (const auto& keyPos : M5Cardputer.Keyboard.keyList()) {
        printKeyValue(M5Cardputer.Keyboard.getKey(keyPos));
    }
}

void App::loop()
{
    const uint32_t now = millis();

    handleKeyboard();
    gps_.update();

    if (state_ == AppState::Splash && (now - splashStartMs_) >= kSplashDurationMs) {
        state_ = AppState::GnssDashboard;
        lastDashboardRefreshMs_ = now;
        drawGnssDashboard();
    }

    if (state_ == AppState::GnssDashboard && (now - lastDashboardRefreshMs_) >= kDashboardRefreshMs) {
        lastDashboardRefreshMs_ = now;
        drawGnssDashboard();
    }

    if ((now - lastSerialDiagMs_) >= kSerialDiagIntervalMs) {
        lastSerialDiagMs_ = now;
        printGnssDiagnostics();
    }
}
