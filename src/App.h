#pragma once

#include <cstdint>

#include "GPSManager.h"

enum class AppState {
    Splash,
    GnssDashboard,
};

class App {
public:
    /** Initializes GNSS, keyboard input, and draws the splash screen. */
    void setup();

    /** Polls keyboard input, GNSS data, and refreshes the active view. */
    void loop();

private:
    void drawSplash();
    void drawGnssDashboard();
    void handleKeyboard();
    void printGnssDiagnostics();
    const char* gnssStatusLabel() const;

    GPSManager gps_;
    AppState state_{AppState::Splash};
    uint32_t splashStartMs_{0};
    uint32_t lastDashboardRefreshMs_{0};
    uint32_t lastSerialDiagMs_{0};
};
