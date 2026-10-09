# MeshSPY build, flash, and test

## Build

Use PlatformIO from the repository root:

```sh
pio run -e t-deck-tft
```

The target is the upstream Meshtastic T-Deck TFT build with the MeshSPY module enabled by default for `T_DECK`.

## OUI-SPY settings slice

After flashing this build, navigate to the `OUI-SPY` application frame. Hold
the T-Deck select button to open its settings screen.

- `UP` / `DOWN`: choose a setting
- `LEFT` / `RIGHT` or `SELECT`: change the selected value
- Hold `SELECT`: save and return
- `BACK` / `CANCEL`: return without saving

The settings currently cover the master scanner switch, Flock-You, Foxhunter,
Sky-Spy, BLE Sniff, watchlist matching, mesh alerts, GPS inclusion, and an
alert-channel slot. They are stored locally in the ESP32 NVS namespace
`meshspy`. All switches default to off except GPS inclusion. Enabled BLE
recognition records state transitions to `/OUI-SPY/observations.jsonl` on the
SD card; mesh alerts remain independently configurable.

## Flash

Follow the normal Meshtastic T-Deck DFU/esptool procedure for the generated artifacts. Do not erase the TF card as part of firmware testing. The first hardware milestone is a normal Meshtastic boot with the additional `OUI-SPY` application frame visible.

## Initial verification

1. Build succeeds for `t-deck-tft`.
2. Meshtastic UI, keyboard, trackball, touch, GPS, SD diagnostics, and SX1262 initialization remain functional.
3. The OUI-SPY frame reports scheduler health and enabled detector activity.
4. Two ordinary Meshtastic nodes exchange messages while the T-Deck is running.
5. No OUI-SPY code calls `WiFi.mode(WIFI_OFF)`, `esp_wifi_restore()`, `SD.begin()`, or direct SX1262 operations from the module scheduler.

Hardware RF coexistence, detector fidelity, SD persistence, ATAK output, and alert delivery remain unverified until the staged services are implemented and tested on a physical T-Deck Plus.
