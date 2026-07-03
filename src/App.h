#pragma once

#include <cstdint>

#include "BatteryManager.h"
#include "DriveSessionManager.h"
#include "GPSManager.h"
#include "GridManager.h"
#include "JourneyLogger.h"
#include "PhotoLocationLogger.h"
#include "RecordingSessionManager.h"
#include "SdBrowser.h"
#include "SessionTileCache.h"
#include "StorageManager.h"

enum class AppState {
    Splash,
    GnssDashboard,
    ExplorerDashboard,
    DriveDashboard,
    JourneyDashboard,
    SdBrowserDashboard,
};

class App {
public:
    void setup();
    void loop();

private:
    void drawSplash();
    void drawGnssDashboard();
    void drawExplorerDashboard();
    void drawDriveDashboard();
    void drawJourneyDashboard();
    void drawSdBrowserDashboard();
    void drawSessionOverlay();
    void drawDashboardFooter(const char* hintA, const char* hintB, uint8_t subpage, uint8_t subpageCount);
    void handleKeyboard();
    void handleSdBrowserKeyboard();
    void handleDashboardSubpageNav(int8_t direction);
    void handleSessionStart(uint32_t nowMs);
    void handleSessionPauseToggle(uint32_t nowMs);
    void handleSessionStop(uint32_t nowMs);
    void handleManualSave();
    void handlePhotoCapture(uint32_t nowMs);
    void showTransientMessage(const char* message, uint32_t nowMs);
    void updateRecordingFixGate(uint32_t nowMs);
    void processExplorer();
    void updateDriveSession(uint32_t nowMs);
    void updateJourneyLogger(uint32_t nowMs);
    void printGnssDiagnostics();
    void printDriveDiagnostics();
    void printBatteryDiagnostics();
    void printSessionDiagnostics();
    void printFixWaitDiagnostics(uint32_t nowMs);
    void drawBatteryIndicator();
    const char* gnssStatusLabel() const;
    const char* gnssFixQualityLabel() const;
    const char* explorerStateLabel() const;
    const char* driveStatusLabel() const;
    const char* journeyStatusLabel() const;
    const char* sessionStateLabel() const;
    const char* journeyFileDisplayName() const;
    bool sessionCollectingData() const;
    uint8_t dashboardSubpageCount() const;

    GPSManager gps_;
    GridManager grid_;
    StorageManager storage_;
    SessionTileCache sessionTileCache_;
    DriveSessionManager driveSession_;
    BatteryManager batteryManager_;
    JourneyLogger journeyLogger_;
    RecordingSessionManager recordingSession_;
    SdBrowser sdBrowser_;
    PhotoLocationLogger photoLocationLogger_;

    AppState state_{AppState::Splash};
    uint8_t dashboardSubpage_{0};
    uint32_t splashStartMs_{0};
    uint32_t lastDashboardRefreshMs_{0};
    uint32_t lastSerialDiagMs_{0};
    uint32_t lastDriveSerialDiagMs_{0};
    uint32_t lastBatterySerialDiagMs_{0};
    uint32_t lastSessionSerialDiagMs_{0};
    uint32_t lastFixWaitDiagMs_{0};
    int32_t lastDisplayedBatteryPercent_{-2};

    GridTile currentTile_{};
    GridTile lastProcessedTile_{};
    bool hasCurrentTile_{false};
    bool hasLastProcessedTile_{false};
    SessionTileResult lastSessionResult_{SessionTileResult::AlreadyPresent};
    uint32_t lastNewTileMs_{0};
    bool cacheFullLogged_{false};

    bool driveResetPending_{false};
    uint32_t driveResetRequestMs_{0};
    bool wasAccurateFix_{false};
    bool needsFreshReference_{false};
    bool fixWaitEntryLogged_{false};

    char transientMessage_[32];
    uint32_t transientMessageUntilMs_{0};
};
