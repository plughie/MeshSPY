#include "configuration.h"

#if defined(T_DECK) && !defined(MESHTASTIC_EXCLUDE_OUI_SPY)

#include "OuiSpyModule.h"

#include "GPSStatus.h"
#include "MeshService.h"
#include "NodeDB.h"
#include "main.h"
#include "mesh/Channels.h"
#include "SPILock.h"

#include <Arduino.h>
#include <BLEAdvertisedDevice.h>
#include <BLEAdvertising.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <Preferences.h>
#include <SD.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <cstring>

OuiSpyModule *ouiSpyModule = nullptr;

namespace {
constexpr const char *WATCHLIST_PATH = "/OUI-SPY/watchlist.csv";
constexpr const char *OBSERVATION_DIR = "/OUI-SPY";
constexpr const char *OBSERVATION_LOG_PATH = "/OUI-SPY/observations.jsonl";

String normalizeWatchlistValue(const char *value)
{
    String normalized = value != nullptr ? value : "";
    normalized.trim();
    normalized.toLowerCase();
    normalized.replace(":", "");
    normalized.replace("-", "");
    normalized.replace(" ", "");
    return normalized;
}

bool containsIgnoreCase(const String &haystack, const char *needle)
{
    if (needle == nullptr || needle[0] == '\0')
        return false;
    String lowerHaystack = haystack;
    String lowerNeedle = needle;
    lowerHaystack.toLowerCase();
    lowerNeedle.toLowerCase();
    return lowerHaystack.indexOf(lowerNeedle) >= 0;
}
} // namespace

namespace
{
// These are the target prefixes currently shipped by OUI-SPY Unified Blue's
// Flock-You detector. Keep the list local to the integration until the shared
// OUI database/persistence layer is in place.
constexpr uint8_t flockOuis[][3] = {
    {0x70, 0xc9, 0x4e}, {0x3c, 0x91, 0x80}, {0xd8, 0xf3, 0xbc}, {0x80, 0x30, 0x49},
    {0xb8, 0x35, 0x32}, {0x14, 0x5a, 0xfc}, {0x74, 0x4c, 0xa1}, {0x08, 0x3a, 0x88},
    {0x9c, 0x2f, 0x9d}, {0xc0, 0x35, 0x32}, {0x94, 0x08, 0x53}, {0xe4, 0xaa, 0xea},
    {0xf4, 0x6a, 0xdd}, {0xe0, 0x0a, 0xf6}, {0x24, 0xb2, 0xb9}, {0x00, 0xf4, 0x8d},
    {0xd0, 0x39, 0x57}, {0xe8, 0xd0, 0xfc}, {0xe0, 0x4f, 0x43}, {0xb8, 0x1e, 0xa4},
    {0x70, 0x08, 0x94}, {0x58, 0x8e, 0x81}, {0xec, 0x1b, 0xbd}, {0x3c, 0x71, 0xbf},
    {0x58, 0x00, 0xe3}, {0x90, 0x35, 0xea}, {0x5c, 0x93, 0xa2}, {0x64, 0x6e, 0x69},
    {0x48, 0x27, 0xea}, {0xa4, 0xcf, 0x12}, {0x14, 0xb5, 0xcd}, {0x82, 0x6b, 0xf2},
    {0xb4, 0x1e, 0x52}, {0x00, 0x03, 0x7f},
};

bool matchesFlockOui(const uint8_t *mac)
{
    for (const auto &oui : flockOuis) {
        if (mac[0] == oui[0] && mac[1] == oui[1] && mac[2] == oui[2])
            return true;
    }
    return false;
}

struct WifiMacHeader {
    uint16_t frameControl;
    uint16_t duration;
    uint8_t addr1[6];
    uint8_t addr2[6];
    uint8_t addr3[6];
    uint16_t sequence;
} __attribute__((packed));

void ouiSpyWifiPromiscuous(void *buffer, wifi_promiscuous_pkt_type_t type)
{
    if (ouiSpyModule == nullptr || buffer == nullptr ||
        (type != WIFI_PKT_MGMT && type != WIFI_PKT_DATA))
        return;

    const auto *packet = static_cast<const wifi_promiscuous_pkt_t *>(buffer);
    if (packet->rx_ctrl.sig_len < sizeof(WifiMacHeader))
        return;

    const auto *header = reinterpret_cast<const WifiMacHeader *>(packet->payload);
    // Flock-You starts with broad transmitter-OUI matching. Address 1/3
    // echo matching is added once normalized records and deduplication land.
    if (matchesFlockOui(header->addr2))
        ouiSpyModule->recordFlockObservation(packet->rx_ctrl.rssi);
}

class OuiSpyBleCallbacks : public BLEAdvertisedDeviceCallbacks
{
  public:
    void onResult(BLEAdvertisedDevice advertisedDevice) override
    {
        if (ouiSpyModule != nullptr) {
            ouiSpyModule->recordBleAdvertisement(advertisedDevice);
        }
    }
};

OuiSpyBleCallbacks bleCallbacks;

void ouiSpyBleScanComplete(BLEScanResults)
{
    // Meshtastic uses the same NimBLE controller for its phone service. Keep
    // that service discoverable after OUI-SPY temporarily stopped advertising
    // to obtain a scan window.
    if (nimbleBluetooth != nullptr && nimbleBluetooth->isActive() && !nimbleBluetooth->isConnected())
        nimbleBluetooth->startAdvertising();
    if (ouiSpyModule != nullptr)
        LOG_DEBUG("OUI-SPY BLE scan complete; Meshtastic advertising restored");
}
} // namespace

OuiSpyModule::OuiSpyModule()
    : SinglePortModule("OUI-SPY", meshtastic_PortNum_PRIVATE_APP), concurrency::OSThread("OUI-SPY", 1000)
{
    ouiSpyModule = this;
    if (inputBroker)
        inputObserver.observe(inputBroker);
}

void OuiSpyModule::openSettings()
{
    savedConfig = config;
    screenMode = ScreenMode::SETTINGS;
    selectedSetting = 0;
    requestFocus();
}

bool OuiSpyModule::uiSettingEnabled(uint8_t index) const
{
    switch (index) {
    case 0:
        return config.scannerEnabled;
    case 1:
        return config.flockYouEnabled;
    case 2:
        return config.foxhunterEnabled;
    case 3:
        return config.skySpyEnabled;
    case 4:
        return config.bleSniffEnabled;
    case 5:
        return config.axonWatchlistEnabled;
    case 6:
        return config.droneIdsEnabled;
    case 7:
        return config.meshAlertsEnabled;
    case 8:
        return config.includeGps;
    default:
        return false;
    }
}

void OuiSpyModule::uiToggleSetting(uint8_t index)
{
    switch (index) {
    case 0:
        config.scannerEnabled = !config.scannerEnabled;
        break;
    case 1:
        config.flockYouEnabled = !config.flockYouEnabled;
        break;
    case 2:
        config.foxhunterEnabled = !config.foxhunterEnabled;
        break;
    case 3:
        config.skySpyEnabled = !config.skySpyEnabled;
        break;
    case 4:
        config.bleSniffEnabled = !config.bleSniffEnabled;
        break;
    case 5:
        config.axonWatchlistEnabled = !config.axonWatchlistEnabled;
        break;
    case 6:
        config.droneIdsEnabled = !config.droneIdsEnabled;
        break;
    case 7:
        config.meshAlertsEnabled = !config.meshAlertsEnabled;
        break;
    case 8:
        config.includeGps = !config.includeGps;
        break;
    default:
        break;
    }
}

void OuiSpyModule::uiSaveSettings()
{
    saveConfig();
    savedConfig = config;
}

void OuiSpyModule::uiSendTestAlert()
{
    LOG_INFO("OUI-SPY sending test mesh alert on channel %u (%s)", config.alertChannel, uiAlertChannelName());
    if (!sendMeshAlert("OUI-SPY TEST: alert manager and channel path are working"))
        LOG_WARN("OUI-SPY test alert was not queued");
}

const char *OuiSpyModule::uiAlertChannelName() const
{
    if (config.alertChannel >= channels.getNumChannels())
        return "invalid";
    return channels.getName(config.alertChannel);
}

void OuiSpyModule::uiCycleAlertChannel()
{
    const uint8_t count = channels.getNumChannels();
    if (count == 0)
        return;
    config.alertChannel = (config.alertChannel + 1) % count;
}

bool OuiSpyModule::matchesWatchlist(BLEAdvertisedDevice &device, const char *address, const char **label) const
{
    if (!config.axonWatchlistEnabled || address == nullptr)
        return false;

    const String normalizedAddress = normalizeWatchlistValue(address);
    if (config.axonWatchlistEnabled && normalizedAddress.startsWith("0025df")) {
        if (label != nullptr)
            *label = "Axon OUI";
        return true;
    }

    if (device.haveManufacturerData()) {
        const String manufacturer = device.getManufacturerData();
        if (manufacturer.length() >= 2) {
            const uint16_t companyId = static_cast<uint8_t>(manufacturer[0]) |
                                       (static_cast<uint16_t>(static_cast<uint8_t>(manufacturer[1])) << 8);
            if (config.axonWatchlistEnabled && companyId == 0x034D) {
                if (label != nullptr)
                    *label = "Axon company ID";
                return true;
            }
        }
    }

    for (int i = 0; i < device.getServiceUUIDCount(); ++i) {
        String uuid = device.getServiceUUID(i).toString().c_str();
        uuid.toLowerCase();
        if (config.axonWatchlistEnabled && (uuid == "fc81" || (uuid.length() >= 8 && uuid.substring(4, 8) == "fc81"))) {
            if (label != nullptr)
                *label = "Axon service UUID";
            return true;
        }
    }

    const String deviceName = device.haveName() ? device.getName().c_str() : "";
    for (uint8_t i = 0; i < watchlistCount; ++i) {
        const auto &entry = watchlist[i];
        if (strcmp(entry.type, "DRONE") == 0)
            continue;
        if (strcmp(entry.type, "OUI") == 0 && normalizedAddress.startsWith(entry.value)) {
            if (label != nullptr)
                *label = entry.label[0] != '\0' ? entry.label : "SD OUI";
            return true;
        }
        if (strcmp(entry.type, "MAC") == 0 && normalizedAddress == entry.value) {
            if (label != nullptr)
                *label = entry.label[0] != '\0' ? entry.label : "SD MAC";
            return true;
        }
        if (strcmp(entry.type, "NAME") == 0 && containsIgnoreCase(deviceName, entry.value)) {
            if (label != nullptr)
                *label = entry.label[0] != '\0' ? entry.label : "SD name";
            return true;
        }
        if (strcmp(entry.type, "CID") == 0 && device.haveManufacturerData()) {
            const String manufacturer = device.getManufacturerData();
            if (manufacturer.length() >= 2) {
                char cid[5];
                snprintf(cid, sizeof(cid), "%02x%02x", static_cast<uint8_t>(manufacturer[1]),
                         static_cast<uint8_t>(manufacturer[0]));
                if (strcmp(cid, entry.value) == 0) {
                    if (label != nullptr)
                        *label = entry.label[0] != '\0' ? entry.label : "SD company ID";
                    return true;
                }
            }
        }
        if (strcmp(entry.type, "SVC") == 0) {
            for (int serviceIndex = 0; serviceIndex < device.getServiceUUIDCount(); ++serviceIndex) {
                String uuid = device.getServiceUUID(serviceIndex).toString().c_str();
                uuid.toLowerCase();
                if (uuid == entry.value || (uuid.length() >= 8 && uuid.substring(4, 8) == entry.value)) {
                    if (label != nullptr)
                        *label = entry.label[0] != '\0' ? entry.label : "SD service UUID";
                    return true;
                }
            }
        }
    }
    return false;
}

bool OuiSpyModule::matchesDroneWatchlist(const char *droneId, const char **label) const
{
    if (!config.droneIdsEnabled || droneId == nullptr || droneId[0] == '\0')
        return false;

    const String normalizedId = normalizeWatchlistValue(droneId);
    for (uint8_t i = 0; i < watchlistCount; ++i) {
        const auto &entry = watchlist[i];
        if (strcmp(entry.type, "DRONE") == 0 && normalizedId == entry.value) {
            if (label != nullptr)
                *label = entry.label[0] != '\0' ? entry.label : "SD drone ID";
            return true;
        }
    }
    return false;
}

void OuiSpyModule::recordBleAdvertisement(BLEAdvertisedDevice &device)
{
    const String addressString = device.getAddress().toString().c_str();
    const char *address = addressString.c_str();
    const int rssi = device.getRSSI();
    ++bleAdvertisements;
    lastBleRssi = static_cast<int8_t>(rssi);
    if (address != nullptr && strncmp(lastBleAddress, address, sizeof(lastBleAddress) - 1) != 0) {
        strncpy(lastBleAddress, address, sizeof(lastBleAddress) - 1);
        lastBleAddress[sizeof(lastBleAddress) - 1] = '\0';
        ++bleDevicesSeen;
        LOG_INFO("OUI-SPY BLE new device %s RSSI %d", lastBleAddress, rssi);
    }

    if (address == nullptr || address[0] == '\0')
        return;

    const char *watchlistLabel = nullptr;
    const bool axonMatch = matchesWatchlist(device, address, &watchlistLabel);

    BlePresence *entry = nullptr;
    BlePresence *freeEntry = nullptr;
    for (auto &candidate : blePresence) {
        if (strncmp(candidate.address, address, sizeof(candidate.address)) == 0) {
            entry = &candidate;
            break;
        }
        if (!candidate.active && freeEntry == nullptr)
            freeEntry = &candidate;
    }
    if (entry == nullptr)
        entry = freeEntry;
    if (entry == nullptr)
        entry = &blePresence[bleDevicesSeen % MAX_BLE_PRESENCE];

    const bool addressChanged = strncmp(entry->address, address, sizeof(entry->address)) != 0;
    const bool wasActive = entry->active;
    if (addressChanged) {
        strncpy(entry->address, address, sizeof(entry->address) - 1);
        entry->address[sizeof(entry->address) - 1] = '\0';
        entry->announced = false;
        entry->watchlistMatched = false;
        entry->watchlistAnnounced = false;
    }
    if (!wasActive) {
        entry->announced = false;
        entry->watchlistAnnounced = false;
    }
    entry->lastSeenMs = millis();
    entry->lastRssi = static_cast<int8_t>(rssi);
    entry->active = true;
    if (addressChanged || !wasActive)
        queueBleObservation(wasActive ? "ble_returned" : "ble_new", address, rssi);

    if (axonMatch && !entry->watchlistMatched) {
        entry->watchlistMatched = true;
        LOG_INFO("OUI-SPY watchlist match %s %s RSSI %d", watchlistLabel != nullptr ? watchlistLabel : "SD", address,
                 rssi);
        queueBleObservation("ble_watchlist_match", address, rssi, watchlistLabel);
    }
}

void OuiSpyModule::recordFlockObservation(int rssi)
{
    ++flockObservations;
    ++flockMatchesTotal;
    lastFlockRssi = static_cast<int8_t>(rssi);
}

void OuiSpyModule::recordDroneId(const char *droneId, int rssi)
{
    if (!config.scannerEnabled || !config.skySpyEnabled || !config.droneIdsEnabled || droneId == nullptr ||
        droneId[0] == '\0')
        return;

    const String normalizedId = normalizeWatchlistValue(droneId);
    if (normalizedId.length() == 0)
        return;

    DronePresence *entry = nullptr;
    DronePresence *freeEntry = nullptr;
    for (auto &candidate : dronePresence) {
        if (strcmp(candidate.identifier, normalizedId.c_str()) == 0) {
            entry = &candidate;
            break;
        }
        if (!candidate.active && freeEntry == nullptr)
            freeEntry = &candidate;
    }
    if (entry == nullptr)
        entry = freeEntry;
    if (entry == nullptr)
        entry = &dronePresence[observations % MAX_DRONE_PRESENCE];

    const bool identifierChanged = strcmp(entry->identifier, normalizedId.c_str()) != 0;
    const bool wasActive = entry->active;
    if (identifierChanged) {
        strncpy(entry->identifier, normalizedId.c_str(), sizeof(entry->identifier) - 1);
        entry->identifier[sizeof(entry->identifier) - 1] = '\0';
        entry->announced = false;
        entry->watchlistMatched = false;
        entry->watchlistAnnounced = false;
    }
    if (!wasActive) {
        entry->announced = false;
        entry->watchlistAnnounced = false;
    }
    entry->lastSeenMs = millis();
    entry->lastRssi = static_cast<int8_t>(rssi);
    entry->active = true;

    const char *watchlistLabel = nullptr;
    const bool watchlistMatch = matchesDroneWatchlist(normalizedId.c_str(), &watchlistLabel);
    if (identifierChanged || !wasActive)
        queueBleObservation(wasActive ? "drone_id_returned" : "drone_id_new", normalizedId.c_str(), rssi);
    if (watchlistMatch && !entry->watchlistMatched) {
        entry->watchlistMatched = true;
        LOG_INFO("OUI-SPY drone ID watchlist match %s %s RSSI %d", watchlistLabel != nullptr ? watchlistLabel : "SD",
                 normalizedId.c_str(), rssi);
        queueBleObservation("drone_id_watchlist_match", normalizedId.c_str(), rssi, watchlistLabel);
    }
}

int OuiSpyModule::handleUiInputEvent(const InputEvent *event)
{
    return handleInputEvent(event);
}

void OuiSpyModule::setup()
{
    LOG_INFO("OUI-SPY module initialized");
    loadConfig();
    // The Meshtastic filesystem layer owns SD initialization.  Scanner
    // services must use that shared layer rather than calling SD.begin(),
    // because TFT, SD, and SX1262 share one SPI bus on T-Deck Plus.
    sdReady = false;
    gpsTagged = gpsStatus != nullptr && gpsStatus->getHasLock();
    configureBleScanner();
    configureFlockMonitor();
    loadWatchlistFromSd();
    setIntervalFromNow(1000);
}

void OuiSpyModule::loadWatchlistFromSd()
{
    watchlistCount = 0;
    watchlistLoaded = false;

    concurrency::LockGuard guard(spiLock);
    if (SD.cardType() == CARD_NONE) {
        LOG_WARN("OUI-SPY watchlist: no SD card");
        return;
    }

    File file = SD.open(WATCHLIST_PATH, FILE_READ);
    if (!file) {
        LOG_INFO("OUI-SPY watchlist: %s not found", WATCHLIST_PATH);
        return;
    }

    while (file.available() && watchlistCount < MAX_WATCHLIST_ENTRIES) {
        String line = file.readStringUntil('\n');
        line.trim();
        if (line.length() == 0 || line.startsWith("#"))
            continue;

        const int firstComma = line.indexOf(',');
        const int secondComma = firstComma < 0 ? -1 : line.indexOf(',', firstComma + 1);
        if (firstComma <= 0 || secondComma <= firstComma + 1)
            continue;

        String type = line.substring(0, firstComma);
        String value = line.substring(firstComma + 1, secondComma);
        String label = line.substring(secondComma + 1);
        type.trim();
        value.trim();
        label.trim();
        type.toUpperCase();
        if (type != "NAME")
            value = normalizeWatchlistValue(value.c_str());
        else
            value.toLowerCase();

        if (type != "OUI" && type != "MAC" && type != "CID" && type != "SVC" && type != "NAME" && type != "DRONE")
            continue;
        auto &entry = watchlist[watchlistCount++];
        type.toCharArray(entry.type, sizeof(entry.type));
        value.toCharArray(entry.value, sizeof(entry.value));
        label.toCharArray(entry.label, sizeof(entry.label));
    }
    file.close();
    watchlistLoaded = true;
    LOG_INFO("OUI-SPY watchlist: loaded %u entries from %s", watchlistCount, WATCHLIST_PATH);
}

void OuiSpyModule::queueBleObservation(const char *event, const char *address, int rssi, const char *label)
{
    if (event == nullptr || address == nullptr || event[0] == '\0' || address[0] == '\0')
        return;
    if (pendingBleObservationCount >= MAX_PENDING_BLE_OBSERVATIONS) {
        if (!sdLogQueueWarningLogged) {
            LOG_WARN("OUI-SPY SD log: observation queue full");
            sdLogQueueWarningLogged = true;
        }
        return;
    }

    auto &pending = pendingBleObservations[pendingBleObservationCount++];
    pending = PendingBleObservation{};
    strncpy(pending.event, event, sizeof(pending.event) - 1);
    strncpy(pending.identifier, address, sizeof(pending.identifier) - 1);
    pending.rssi = static_cast<int8_t>(rssi);
    if (label != nullptr)
        strncpy(pending.label, label, sizeof(pending.label) - 1);
}

void OuiSpyModule::flushBleObservation()
{
    if (pendingBleObservationCount == 0)
        return;

    const PendingBleObservation pending = pendingBleObservations[0];
    for (uint8_t index = 1; index < pendingBleObservationCount; ++index)
        pendingBleObservations[index - 1] = pendingBleObservations[index];
    --pendingBleObservationCount;
    sdLogQueueWarningLogged = false;
    writeBleObservation(pending.event, pending.identifier, pending.rssi, pending.label);
}

void OuiSpyModule::writeBleObservation(const char *event, const char *address, int rssi, const char *label)
{
    if (event == nullptr || address == nullptr || event[0] == '\0' || address[0] == '\0')
        return;

    concurrency::LockGuard guard(spiLock);
    if (SD.cardType() == CARD_NONE)
        return;

    if (!SD.exists(OBSERVATION_DIR) && !SD.mkdir(OBSERVATION_DIR)) {
        if (!sdLogFailureLogged) {
            LOG_WARN("OUI-SPY SD log: unable to create %s", OBSERVATION_DIR);
            sdLogFailureLogged = true;
        }
        return;
    }

    File file = SD.open(OBSERVATION_LOG_PATH, FILE_APPEND);
    if (!file) {
        if (!sdLogFailureLogged) {
            LOG_WARN("OUI-SPY SD log: unable to open %s", OBSERVATION_LOG_PATH);
            sdLogFailureLogged = true;
        }
        return;
    }

    String line = "{\"event\":\"";
    line += event;
    line += "\",\"uptime_ms\":";
    line += String(millis());
    line += ",\"address\":\"";
    line += address;
    line += "\",\"rssi\":";
    line += String(rssi);

    if (label != nullptr && label[0] != '\0') {
        line += ",\"label\":\"";
        for (const char *cursor = label; *cursor != '\0'; ++cursor) {
            if (*cursor == '\\' || *cursor == '"')
                line += '\\';
            line += *cursor;
        }
        line += '"';
    }

    const bool includePosition = config.includeGps && gpsStatus != nullptr && gpsStatus->getHasLock();
    line += ",\"gps\":";
    line += includePosition ? "true" : "false";
    if (includePosition) {
        line += ",\"latitude\":";
        line += String(gpsStatus->getLatitude() * 1e-7, 7);
        line += ",\"longitude\":";
        line += String(gpsStatus->getLongitude() * 1e-7, 7);
        line += ",\"altitude_m\":";
        line += String(gpsStatus->getAltitude());
    }
    line += "}\n";

    if (file.print(line) == 0 && !sdLogFailureLogged) {
        LOG_WARN("OUI-SPY SD log: write failed for %s", OBSERVATION_LOG_PATH);
        sdLogFailureLogged = true;
    } else {
        sdLogFailureLogged = false;
    }
    file.close();
}

void OuiSpyModule::configureBleScanner()
{
    if (bleScanReady || !BLEDevice::getInitialized())
        return;

    BLEScan *scan = BLEDevice::getScan();
    if (scan == nullptr)
        return;

    scan->setAdvertisedDeviceCallbacks(&bleCallbacks, false);
    scan->setActiveScan(false);
    scan->setInterval(160);
    scan->setWindow(80);
#if defined(CONFIG_NIMBLE_ENABLED)
    scan->setDuplicateFilter(false);
#endif
    bleScanReady = true;
    nextBleScanMs = millis();
    LOG_INFO("OUI-SPY BLE scanner ready");
}

void OuiSpyModule::configureFlockMonitor()
{
    if (flockCallbackReady || WiFi.getMode() == WIFI_OFF)
        return;

    wifi_promiscuous_filter_t filter = {};
    filter.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA;
    if (esp_wifi_set_promiscuous_filter(&filter) == ESP_OK &&
        esp_wifi_set_promiscuous_rx_cb(&ouiSpyWifiPromiscuous) == ESP_OK)
        flockCallbackReady = true;
}

void OuiSpyModule::loadConfig()
{
    Preferences preferences;
    if (!preferences.begin("meshspy", true))
        return;

    config.scannerEnabled = preferences.getBool("scanner", config.scannerEnabled);
    config.flockYouEnabled = preferences.getBool("flock", config.flockYouEnabled);
    config.foxhunterEnabled = preferences.getBool("fox", config.foxhunterEnabled);
    config.skySpyEnabled = preferences.getBool("sky", config.skySpyEnabled);
    config.bleSniffEnabled = preferences.getBool("ble", config.bleSniffEnabled);
    config.axonWatchlistEnabled = preferences.getBool("axon", config.axonWatchlistEnabled);
    config.droneIdsEnabled = preferences.getBool("drone", config.droneIdsEnabled);
    config.meshAlertsEnabled = preferences.getBool("alerts", config.meshAlertsEnabled);
    config.includeGps = preferences.getBool("gps", config.includeGps);
    config.alertChannel = preferences.getUChar("channel", config.alertChannel);
    preferences.end();
    savedConfig = config;
}

void OuiSpyModule::saveConfig()
{
    Preferences preferences;
    if (!preferences.begin("meshspy", false))
        return;

    preferences.putBool("scanner", config.scannerEnabled);
    preferences.putBool("flock", config.flockYouEnabled);
    preferences.putBool("fox", config.foxhunterEnabled);
    preferences.putBool("sky", config.skySpyEnabled);
    preferences.putBool("ble", config.bleSniffEnabled);
    preferences.putBool("axon", config.axonWatchlistEnabled);
    preferences.putBool("drone", config.droneIdsEnabled);
    preferences.putBool("alerts", config.meshAlertsEnabled);
    preferences.putBool("gps", config.includeGps);
    preferences.putUChar("channel", config.alertChannel);
    preferences.end();
}

bool OuiSpyModule::frameWasRecentlyDrawn() const
{
    // Module frames are redrawn periodically. This keeps the input observer
    // from consuming navigation events while another Meshtastic frame is
    // active, without reaching into Screen's private frame bookkeeping.
    return lastFrameDrawMs != 0 && (millis() - lastFrameDrawMs) < 1500;
}

int OuiSpyModule::handleInputEvent(const InputEvent *event)
{
    if (!event || !frameWasRecentlyDrawn())
        return 0;

    if (screenMode == ScreenMode::STATUS) {
        if (event->inputEvent == INPUT_BROKER_SELECT_LONG) {
            savedConfig = config;
            screenMode = ScreenMode::SETTINGS;
            selectedSetting = 0;
            setIntervalFromNow(0);
            return 1;
        }
        return 0;
    }

    constexpr uint8_t settingCount = 10;
    switch (event->inputEvent) {
    case INPUT_BROKER_UP:
        selectedSetting = (selectedSetting + settingCount - 1) % settingCount;
        return 1;
    case INPUT_BROKER_DOWN:
        selectedSetting = (selectedSetting + 1) % settingCount;
        return 1;
    case INPUT_BROKER_SELECT:
        switch (selectedSetting) {
        case 0:
            config.scannerEnabled = !config.scannerEnabled;
            break;
        case 1:
            config.flockYouEnabled = !config.flockYouEnabled;
            break;
        case 2:
            config.foxhunterEnabled = !config.foxhunterEnabled;
            break;
        case 3:
            config.skySpyEnabled = !config.skySpyEnabled;
            break;
        case 4:
            config.bleSniffEnabled = !config.bleSniffEnabled;
            break;
        case 5:
            config.axonWatchlistEnabled = !config.axonWatchlistEnabled;
            break;
        case 6:
            config.droneIdsEnabled = !config.droneIdsEnabled;
            break;
        case 7:
            config.meshAlertsEnabled = !config.meshAlertsEnabled;
            break;
        case 8:
            config.includeGps = !config.includeGps;
            break;
        case 9:
            config.alertChannel = (config.alertChannel + 1) % 8;
            break;
        }
        return 1;
    case INPUT_BROKER_LEFT:
    case INPUT_BROKER_RIGHT:
        if (selectedSetting == 9) {
            if (event->inputEvent == INPUT_BROKER_LEFT)
                config.alertChannel = (config.alertChannel + 7) % 8;
            else
                config.alertChannel = (config.alertChannel + 1) % 8;
        } else {
            // LEFT and RIGHT are both convenient toggles for boolean rows.
            switch (selectedSetting) {
            case 0:
                config.scannerEnabled = !config.scannerEnabled;
                break;
            case 1:
                config.flockYouEnabled = !config.flockYouEnabled;
                break;
            case 2:
                config.foxhunterEnabled = !config.foxhunterEnabled;
                break;
            case 3:
                config.skySpyEnabled = !config.skySpyEnabled;
                break;
            case 4:
                config.bleSniffEnabled = !config.bleSniffEnabled;
                break;
            case 5:
                config.axonWatchlistEnabled = !config.axonWatchlistEnabled;
                break;
            case 6:
                config.droneIdsEnabled = !config.droneIdsEnabled;
                break;
            case 7:
                config.meshAlertsEnabled = !config.meshAlertsEnabled;
                break;
            case 8:
                config.includeGps = !config.includeGps;
                break;
            }
        }
        return 1;
    case INPUT_BROKER_SELECT_LONG:
        saveConfig();
        screenMode = ScreenMode::STATUS;
        setIntervalFromNow(0);
        return 1;
    case INPUT_BROKER_CANCEL:
    case INPUT_BROKER_BACK:
        config = savedConfig;
        screenMode = ScreenMode::STATUS;
        setIntervalFromNow(0);
        return 1;
    default:
        return 0;
    }
}

ProcessMessage OuiSpyModule::handleReceived(const meshtastic_MeshPacket &mp)
{
    // Remote status/control commands are intentionally not enabled in this
    // first slice.  The port boundary is reserved so command handling can be
    // added without intercepting ordinary Meshtastic text messages.
    (void)mp;
    return ProcessMessage::CONTINUE;
}

bool OuiSpyModule::sendMeshAlert(const char *message)
{
    if (!config.meshAlertsEnabled) {
        LOG_WARN("OUI-SPY mesh alert suppressed: mesh alerts are disabled");
        return false;
    }
    if (message == nullptr) {
        LOG_WARN("OUI-SPY mesh alert suppressed: empty message");
        return false;
    }
    if (config.alertChannel >= channels.getNumChannels()) {
        LOG_WARN("OUI-SPY mesh alert suppressed: invalid channel %u (count %u)", config.alertChannel,
                 channels.getNumChannels());
        return false;
    }

    const meshtastic_Channel &channel = channels.getByIndex(config.alertChannel);
    if (!channel.has_settings || channel.role == meshtastic_Channel_Role_DISABLED) {
        LOG_WARN("OUI-SPY mesh alert suppressed: channel %u is disabled/unconfigured", config.alertChannel);
        return false;
    }

    meshtastic_MeshPacket *packet = allocDataPacket();
    if (packet == nullptr) {
        LOG_WARN("OUI-SPY mesh alert suppressed: no packet buffer");
        return false;
    }

    packet->decoded.portnum = meshtastic_PortNum_TEXT_MESSAGE_APP;
    packet->to = NODENUM_BROADCAST;
    packet->channel = config.alertChannel;
    packet->decoded.want_response = false;
    packet->want_ack = false;
    packet->priority = meshtastic_MeshPacket_Priority_BACKGROUND;
    packet->decoded.payload.size = std::min(strlen(message), sizeof(packet->decoded.payload.bytes));
    memcpy(packet->decoded.payload.bytes, message, packet->decoded.payload.size);
    service->sendToMesh(packet, RX_SRC_LOCAL, true);
    ++meshAlerts;
    lastMeshAlertMs = millis();
    LOG_INFO("OUI-SPY mesh alert queued on channel %u (%s)", config.alertChannel, channel.settings.name);
    return true;
}

void OuiSpyModule::processBlePresence()
{
    constexpr uint32_t BLE_LOST_AFTER_MS = 15000;
    const uint32_t now = millis();
    const bool canAlert = config.meshAlertsEnabled &&
                          (lastMeshAlertMs == 0 || now - lastMeshAlertMs >= 30000);

    for (auto &entry : blePresence) {
        if (entry.address[0] == '\0')
            continue;

        if (entry.active && entry.watchlistMatched && !entry.watchlistAnnounced) {
            if (canAlert) {
                char alert[112];
                snprintf(alert, sizeof(alert), "OUI-SPY WATCHLIST: %s RSSI %d", entry.address,
                         static_cast<int>(entry.lastRssi));
                if (sendMeshAlert(alert)) {
                    entry.watchlistAnnounced = true;
                    return;
                }
            }
        }

        if (entry.active && !entry.announced) {
            if (canAlert) {
                char alert[96];
                snprintf(alert, sizeof(alert), "OUI-SPY BLE NEW: %s RSSI %d", entry.address,
                         static_cast<int>(entry.lastRssi));
                if (sendMeshAlert(alert)) {
                    entry.announced = true;
                    return;
                }
            }
        }

        if (entry.active && now - entry.lastSeenMs >= BLE_LOST_AFTER_MS) {
            entry.active = false;
            queueBleObservation("ble_lost", entry.address, entry.lastRssi);
            if (config.meshAlertsEnabled && (lastMeshAlertMs == 0 || now - lastMeshAlertMs >= 30000)) {
                char alert[80];
                snprintf(alert, sizeof(alert), "OUI-SPY BLE LOST: %s", entry.address);
                if (sendMeshAlert(alert))
                    return;
            }
        }
    }
}

void OuiSpyModule::processDronePresence()
{
    constexpr uint32_t DRONE_LOST_AFTER_MS = 15000;
    const uint32_t now = millis();
    const bool canAlert = config.meshAlertsEnabled &&
                          (lastMeshAlertMs == 0 || now - lastMeshAlertMs >= 30000);

    for (auto &entry : dronePresence) {
        if (entry.identifier[0] == '\0')
            continue;

        if (entry.active && entry.watchlistMatched && !entry.watchlistAnnounced && canAlert) {
            char alert[112];
            snprintf(alert, sizeof(alert), "OUI-SPY DRONE ID MATCH: %s RSSI %d", entry.identifier,
                     static_cast<int>(entry.lastRssi));
            if (sendMeshAlert(alert)) {
                entry.watchlistAnnounced = true;
                return;
            }
        }

        if (entry.active && !entry.announced && canAlert) {
            char alert[96];
            snprintf(alert, sizeof(alert), "OUI-SPY DRONE ID: %s RSSI %d", entry.identifier,
                     static_cast<int>(entry.lastRssi));
            if (sendMeshAlert(alert)) {
                entry.announced = true;
                return;
            }
        }

        if (entry.active && now - entry.lastSeenMs >= DRONE_LOST_AFTER_MS) {
            entry.active = false;
            queueBleObservation("drone_id_lost", entry.identifier, entry.lastRssi);
            if (config.meshAlertsEnabled && (lastMeshAlertMs == 0 || now - lastMeshAlertMs >= 30000)) {
                char alert[96];
                snprintf(alert, sizeof(alert), "OUI-SPY DRONE ID LOST: %s", entry.identifier);
                if (sendMeshAlert(alert))
                    return;
            }
        }
    }
}

int32_t OuiSpyModule::runOnce()
{
    if (!moduleInitialized) {
        setup();
        moduleInitialized = true;
    }

    // Bluetooth and Wi-Fi can finish starting after the module thread is
    // created. Retry their integration points until the radio services exist.
    configureBleScanner();
    configureFlockMonitor();

    const bool wantFlock = config.scannerEnabled && config.flockYouEnabled;
    if (wantFlock && flockCallbackReady && !flockPromiscuousEnabled && WiFi.getMode() != WIFI_OFF) {
        if (esp_wifi_set_promiscuous(true) == ESP_OK) {
            flockPromiscuousEnabled = true;
            LOG_INFO("OUI-SPY Flock-You current-channel monitor enabled");
        }
    } else if (!wantFlock && flockPromiscuousEnabled) {
        esp_wifi_set_promiscuous(false);
        flockPromiscuousEnabled = false;
        LOG_INFO("OUI-SPY Flock-You current-channel monitor disabled");
    }

    const bool phoneBleConnected = nimbleBluetooth != nullptr && nimbleBluetooth->isConnected();
    if (phoneBleConnected) {
        if (!bleScanPausedForPhone) {
            LOG_INFO("OUI-SPY BLE scan paused while Meshtastic phone connection is active");
            bleScanPausedForPhone = true;
        }
    } else if (bleScanPausedForPhone) {
        LOG_INFO("OUI-SPY BLE scan resumed after Meshtastic phone connection ended");
        bleScanPausedForPhone = false;
        nextBleScanMs = millis();
    }

    if (config.scannerEnabled && config.bleSniffEnabled && bleScanReady && !bleScanPausedForPhone &&
        !BLEDevice::getScan()->isScanning() && static_cast<int32_t>(millis() - nextBleScanMs) >= 0) {
        BLEAdvertising *advertising = BLEDevice::getAdvertising();
        if (advertising != nullptr && advertising->isAdvertising()) {
            // NimBLE on this controller cannot advertise and initiate a scan
            // procedure at the same time. Stop first, then wait for the GAP
            // completion event before starting the scan on the next pass.
            if (!bleAdvertisingPausedForScan && advertising->stop()) {
                bleAdvertisingPausedForScan = true;
                // ble_gap_adv_stop() completes asynchronously. Give the
                // controller time to dispatch BLE_GAP_EVENT_ADV_COMPLETE;
                // otherwise ble_gap_disc() returns BLE_HS_EBUSY (rc=8).
                nextBleScanMs = millis() + 750;
            } else {
                nextBleScanMs = millis() + 250;
            }
        } else if (BLEDevice::getScan()->start(1, bleAdvertisingPausedForScan ? ouiSpyBleScanComplete : nullptr, false)) {
            bleAdvertisingPausedForScan = false;
            bleScanStartWarningLogged = false;
            nextBleScanMs = millis() + 1500;
            LOG_DEBUG("OUI-SPY BLE scan started");
        } else {
            if (bleAdvertisingPausedForScan && nimbleBluetooth != nullptr && nimbleBluetooth->isActive())
                nimbleBluetooth->startAdvertising();
            bleAdvertisingPausedForScan = false;
            // Avoid a tight retry loop when the controller is temporarily busy
            // with another GAP procedure.
            nextBleScanMs = millis() + 10000;
            if (!bleScanStartWarningLogged) {
                LOG_WARN("OUI-SPY BLE scan unavailable; retrying in 10 seconds");
                bleScanStartWarningLogged = true;
            }
        }
    }

    if (bleAdvertisements != 0) {
        observations += bleAdvertisements;
        bleAdvertisements = 0;
        lastObservationMs = millis();
    }
    processBlePresence();
    processDronePresence();
    flushBleObservation();
    if (flockObservations != 0) {
        const uint32_t newMatches = flockObservations;
        observations += flockObservations;
        flockObservations = 0;
        lastObservationMs = millis();
        if (newMatches != 0 && (lastMeshAlertMs == 0 || millis() - lastMeshAlertMs >= 30000)) {
            char alert[96];
            snprintf(alert, sizeof(alert), "OUI-SPY FLOCK: %lu OUI match(es), RSSI %d dBm",
                     static_cast<unsigned long>(newMatches), static_cast<int>(lastFlockRssi));
            sendMeshAlert(alert);
        }
    }
    gpsTagged = gpsStatus != nullptr && gpsStatus->getHasLock();
    return 1000;
}

void OuiSpyModule::drawFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    (void)state;
    lastFrameDrawMs = millis();

    if (screenMode == ScreenMode::SETTINGS) {
        drawSettings(display, x, y);
        return;
    }

    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->setFont(ArialMT_Plain_16);
    display->drawString(x, y, "OUI-SPY");
    display->setFont(ArialMT_Plain_10);
    display->drawString(x, y + 20, config.scannerEnabled ? "Scanner: enabled" : "Scanner: disabled");
    display->drawString(x, y + 34, "Hold SELECT: settings");
    display->drawString(x, y + 48, "Obs: " + String(observations));
    display->drawString(x, y + 62, "BLE: " + String(lastBleAddress));
    display->drawString(x, y + 76, "BLE new: " + String(bleDevicesSeen) + " " + String(lastBleRssi) + " dBm");
    display->drawString(x, y + 90, "Flock: " + String(flockMatchesTotal) + " " + String(lastFlockRssi) + " dBm");
    display->drawString(x, y + 104, "Alerts: " + String(meshAlerts));
    display->drawString(x, y + 118, (config.includeGps && gpsTagged) ? "GPS: FIX" : "GPS: waiting");
    display->drawString(x, y + 132, sdReady ? "SD: ready" : "SD: shared layer");
}

void OuiSpyModule::drawSettings(OLEDDisplay *display, int16_t x, int16_t y)
{
    static const char *const names[] = {"Scanner", "Flock-You", "Foxhunter", "Sky-Spy", "BLE Sniff",
                                        "Watchlist", "Drone IDs", "Mesh alerts", "Include GPS", "Alert channel"};
    const bool values[] = {config.scannerEnabled, config.flockYouEnabled, config.foxhunterEnabled,
                           config.skySpyEnabled, config.bleSniffEnabled, config.axonWatchlistEnabled,
                           config.droneIdsEnabled, config.meshAlertsEnabled, config.includeGps, true};

    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->setFont(ArialMT_Plain_10);
    display->drawString(x, y, "OUI-SPY settings");
    for (uint8_t row = 0; row < 4; ++row) {
        uint8_t index = (selectedSetting / 4) * 4 + row;
        if (index >= 10)
            break;
        display->drawString(x, y + 16 + row * 14, index == selectedSetting ? ">" : " ");
        display->drawString(x + 10, y + 16 + row * 14, names[index]);
        if (index == 9)
            display->drawString(x + 180, y + 16 + row * 14, String(config.alertChannel) + " " + uiAlertChannelName());
        else
            display->drawString(x + 180, y + 16 + row * 14, values[index] ? "ON" : "OFF");
    }
    display->drawString(x, y + 76, "UP/DOWN select  LEFT/RIGHT toggle");
    display->drawString(x, y + 90, "Hold SELECT save  BACK cancel");
}

#endif
