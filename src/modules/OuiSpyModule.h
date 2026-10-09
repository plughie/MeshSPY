#pragma once

#include "configuration.h"

#if defined(T_DECK) && !defined(MESHTASTIC_EXCLUDE_OUI_SPY)

#include "SinglePortModule.h"
#include "concurrency/OSThread.h"
#include "input/InputBroker.h"

class BLEAdvertisedDevice;

/**
 * MeshSPY integration boundary for OUI-SPY Unified Blue.
 *
 * The detector services will publish normalized observations to this module.
 * Collection, local persistence, ATAK export, and mesh notification are kept
 * separate so changing an alert policy cannot disable local collection.
 */
class OuiSpyModule : public SinglePortModule, private concurrency::OSThread
{
  public:
    OuiSpyModule();

    void openSettings();
    int handleUiInputEvent(const InputEvent *event);

    bool uiSettingEnabled(uint8_t index) const;
    void uiToggleSetting(uint8_t index);
    void uiSaveSettings();
    void uiSendTestAlert();
    const char *uiAlertChannelName() const;
    void uiCycleAlertChannel();
    void recordBleAdvertisement(BLEAdvertisedDevice &device);
    void recordDroneId(const char *droneId, int rssi);
    void recordFlockObservation(int rssi);

    bool wantUIFrame() override { return true; }
    bool interceptingKeyboardInput() override { return screenMode == ScreenMode::SETTINGS; }
    void drawFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y) override;

  protected:
    void setup() override;
    ProcessMessage handleReceived(const meshtastic_MeshPacket &mp) override;
    int32_t runOnce() override;

  private:
    struct Config {
        bool scannerEnabled = false;
        bool flockYouEnabled = false;
        bool foxhunterEnabled = false;
        bool skySpyEnabled = false;
        bool bleSniffEnabled = false;
        bool axonWatchlistEnabled = false;
        bool droneIdsEnabled = false;
        bool meshAlertsEnabled = false;
        bool includeGps = true;
        uint8_t alertChannel = 0;
    };

    struct BlePresence {
        char address[18] = {};
        uint32_t lastSeenMs = 0;
        int8_t lastRssi = 0;
        bool active = false;
        bool announced = false;
        bool watchlistMatched = false;
        bool watchlistAnnounced = false;
    };

    struct WatchlistEntry {
        char type[8] = {};
        char value[40] = {};
        char label[32] = {};
    };

    struct PendingBleObservation {
        char event[24] = {};
        char identifier[40] = {};
        char label[32] = {};
        int8_t rssi = 0;
    };

    struct DronePresence {
        char identifier[40] = {};
        uint32_t lastSeenMs = 0;
        int8_t lastRssi = 0;
        bool active = false;
        bool announced = false;
        bool watchlistMatched = false;
        bool watchlistAnnounced = false;
    };

    static constexpr uint8_t MAX_BLE_PRESENCE = 32;
    static constexpr uint8_t MAX_DRONE_PRESENCE = 16;
    static constexpr uint8_t MAX_WATCHLIST_ENTRIES = 64;
    static constexpr uint8_t MAX_PENDING_BLE_OBSERVATIONS = 8;

    enum class ScreenMode : uint8_t { STATUS, SETTINGS };

    int handleInputEvent(const InputEvent *event);
    void loadConfig();
    void saveConfig();
    void configureBleScanner();
    void configureFlockMonitor();
    void loadWatchlistFromSd();
    void queueBleObservation(const char *event, const char *address, int rssi, const char *label = nullptr);
    void flushBleObservation();
    void writeBleObservation(const char *event, const char *address, int rssi, const char *label = nullptr);
    void drawSettings(OLEDDisplay *display, int16_t x, int16_t y);
    bool sendMeshAlert(const char *message);
    void processBlePresence();
    void processDronePresence();
    bool matchesWatchlist(BLEAdvertisedDevice &device, const char *address, const char **label) const;
    bool matchesDroneWatchlist(const char *droneId, const char **label) const;
    bool frameWasRecentlyDrawn() const;

    CallbackObserver<OuiSpyModule, const InputEvent *> inputObserver =
        CallbackObserver<OuiSpyModule, const InputEvent *>(this, &OuiSpyModule::handleInputEvent);
    Config config;
    Config savedConfig;
    ScreenMode screenMode = ScreenMode::STATUS;
    uint8_t selectedSetting = 0;
    uint32_t lastFrameDrawMs = 0;
    uint32_t observations = 0;
    uint32_t meshAlerts = 0;
    uint32_t lastObservationMs = 0;
    bool sdReady = false;
    bool gpsTagged = false;
    uint32_t bleAdvertisements = 0;
    uint32_t bleDevicesSeen = 0;
    int8_t lastBleRssi = 0;
    char lastBleAddress[18] = "none";
    uint32_t nextBleScanMs = 0;
    bool bleScanReady = false;
    bool bleScanPausedForPhone = false;
    bool bleScanStartWarningLogged = false;
    bool bleAdvertisingPausedForScan = false;
    BlePresence blePresence[MAX_BLE_PRESENCE];
    DronePresence dronePresence[MAX_DRONE_PRESENCE];
    WatchlistEntry watchlist[MAX_WATCHLIST_ENTRIES];
    uint8_t watchlistCount = 0;
    bool watchlistLoaded = false;
    bool sdLogFailureLogged = false;
    bool sdLogQueueWarningLogged = false;
    PendingBleObservation pendingBleObservations[MAX_PENDING_BLE_OBSERVATIONS];
    uint8_t pendingBleObservationCount = 0;
    uint32_t flockObservations = 0;
    uint32_t flockMatchesTotal = 0;
    int8_t lastFlockRssi = 0;
    bool flockPromiscuousEnabled = false;
    bool flockCallbackReady = false;
    bool moduleInitialized = false;
    uint32_t lastMeshAlertMs = 0;
};

extern OuiSpyModule *ouiSpyModule;

#endif
