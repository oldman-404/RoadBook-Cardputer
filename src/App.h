#pragma once

#include <cstdint>

#include "GPSManager.h"
#include "GridManager.h"
#include "SessionTileCache.h"
#include "StorageManager.h"

enum class AppState {
    Splash,
    GnssDashboard,
    ExplorerDashboard,
};

class App {
public:
    /** Initializes GNSS, storage, keyboard input, and draws the splash screen. */
    void setup();

    /** Polls keyboard input, GNSS data, explorer logic, and refreshes the active view. */
    void loop();

private:
    void drawSplash();
    void drawGnssDashboard();
    void drawExplorerDashboard();
    void handleKeyboard();
    void processExplorer();
    void printGnssDiagnostics();
    const char* gnssStatusLabel() const;
    const char* explorerStateLabel() const;

    GPSManager gps_;
    GridManager grid_;
    StorageManager storage_;
    SessionTileCache sessionTileCache_;

    AppState state_{AppState::Splash};
    uint32_t splashStartMs_{0};
    uint32_t lastDashboardRefreshMs_{0};
    uint32_t lastSerialDiagMs_{0};

    GridTile currentTile_{};
    GridTile lastProcessedTile_{};
    bool hasCurrentTile_{false};
    bool hasLastProcessedTile_{false};
    SessionTileResult lastSessionResult_{SessionTileResult::AlreadyPresent};
    uint32_t lastNewTileMs_{0};
    bool cacheFullLogged_{false};
};
