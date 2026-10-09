# MeshSPY integration assessment

This fork starts from Meshtastic firmware commit `364a111f4a5601708f3b9d60527aca02b6ac73ef` and targets the LILYGO T-Deck Plus. The OUI-SPY Unified Blue reference was reviewed at commit `f63f79f9616da5f81ec6a5f76a169a6995a92759`.

## Findings

### Hardware and pin compatibility

The current Meshtastic `t-deck` target already matches the T-Deck Plus wiring documented by LILYGO:

| Function | GPIO |
| --- | ---: |
| Shared SPI SCK/MISO/MOSI | 40 / 38 / 41 |
| SX1262 CS / DIO1 / BUSY / RESET | 9 / 45 / 13 / 17 |
| TF card CS | 39 |
| TFT CS / DC / backlight | 12 / 11 / 42 |
| GPS RX / TX | 44 / 43 |
| Peripheral power enable | 10 |

The SPI bus is shared by the TFT, TF card, and SX1262. Every OUI-SPY service must leave the LoRa CS line deselected and use the existing filesystem/radio ownership boundaries. Calling `SD.begin()` or directly driving the SX1262 from a scanner task would be unsafe.

The Plus has ESP32-S3 Wi-Fi/BLE, 16 MB flash, 8 MB PSRAM, SX1262, GPS, TFT/touch, keyboard, trackball, and TF storage. The separate ESP32-C3 keyboard controller is not a second application CPU.

### Source-level conflicts

| Area | OUI-SPY reference | Meshtastic | Decision |
| --- | --- | --- | --- |
| Boot/application model | Arduino `setup()` plus rebooting six-mode selector | Persistent Meshtastic service/module model | Do not merge boot loops; port detector services behind a cooperative module boundary |
| Wi-Fi ownership | AP setup, `WIFI_OFF`, `esp_wifi_restore()`, and per-mode reconfiguration | Meshtastic Bluetooth/Wi-Fi/phone services | Scanner modes must be non-destructive and scheduler-owned; AP dashboards are deferred/optional |
| BLE | NimBLE scanner and raw capture paths | Existing ESP32 Bluetooth stack and phone API | Reuse compatible low-level scanner pieces only after coexistence testing; no global radio reset |
| Wi-Fi promiscuous mode | Flock-You and PCAP callbacks/channel hopping | No equivalent always-on detector service | Time-slice Wi-Fi work independently from LoRa; preserve mesh RX/TX continuously |
| SD/persistence | LittleFS/SPIFFS session files and custom partitions | Shared TF card filesystem and 16 MB Meshtastic partition table | Store OUI-SPY data under a namespaced directory on the Meshtastic-managed TF card |
| GPS | OUI-SPY reference expects external/host-side GPS in some modes | T-Deck Plus GPS is already exposed through `gpsStatus` | Use Meshtastic GPS status as the single source of geotags |
| UI | Web selector/AP | Existing T-Deck Meshtastic UI | Add an OUI-SPY application/frame; keep normal messaging/navigation intact |
| ATAK | Not implemented as a Meshtastic module | Existing `AtakPluginModule` is a payload passthrough | Add SD CoT export first; later use the existing ATAK plugin port/live transport where appropriate |

### Resource risks

- Wi-Fi promiscuous capture, BLE scanning, and LoRa cannot all assume uninterrupted radio time. “Continuous” means cooperative background operation with bounded scan windows; it does not mean simultaneous RF activity.
- OUI-SPY’s 2 MB in-PSRAM PCAP buffers are plausible on an 8 MB PSRAM board, but they compete with display buffers, Meshtastic queues, maps, and TLS/phone services. Use bounded ring buffers and configurable capture limits.
- The OUI-SPY source currently contains global Arduino/WebServer/NVS assumptions and mode-specific `setup()`/`loop()` symbols. Those must be refactored into service objects before importing them.
- The shared SPI bus requires serialized SD access. Detection logging must be buffered and flushed in short transactions so LoRa latency remains bounded.

## Proposed architecture

```text
OUI-SPY detector services
  Flock-You | Foxhunter | Sky-Spy | BLE Sniff | OUI detector
                    |
          normalized Observation queue
                    |
       GPS enrichment from gpsStatus
          /         |          \
   SD archive   CoT exporter   Alert manager
                              |
                   ordinary TEXT_MESSAGE_APP
                   on configured private channel
```

The alert manager is policy-only: per-detector enablement, watchlists, deduplication, cooldowns, rate limits, optional GPS, and periodic summaries. Local capture remains enabled independently. A future command handler should accept only authenticated/private requests and never expose raw capture data by default.

The current implementation registers an OUI-SPY application frame and
cooperative scheduler boundary without altering normal radio or UI
initialization. BLE state transitions are archived as JSON Lines on the shared
SD card under `/OUI-SPY/observations.jsonl`; watchlist matching and mesh
notification remain independent policies. The archive writes only state
transitions rather than every repeated advertisement.

## Implementation sequence

1. Keep the Meshtastic T-Deck target buildable and add the OUI-SPY application/module boundary. **Complete.**
2. Create a normalized observation ABI and import detector logic one service at a time, beginning with BLE detector/Foxhunter, then Sky-Spy, then Flock-You Wi-Fi, then BLE/packet capture.
3. Add a PSRAM-backed bounded event/PCAP queue and a shared-SPI SD archive writer. **Initial JSONL archive complete; bounded PCAP/event queue remains.**
4. Add GPS enrichment and versioned JSON-lines/CSV observation records.
5. Add SD-card CoT export, including track/update identity rules for moving Remote ID targets.
6. Add configurable ordinary Meshtastic text alerts on a selected standard private channel, with dedupe/rate limits and location privacy controls.
7. Add the OUI-SPY screen/application controls and optional authenticated status/control commands.
8. Test mesh continuity, scan duty cycles, SD contention, GPS loss/reacquisition, power behavior, and recovery from full/corrupt storage.

## Licensing and provenance

Meshtastic is GPL-3.0 in the checked-out upstream repository. This fork keeps
that license in `LICENSE`; a combined distributed firmware based on Meshtastic
must preserve GPL notices and corresponding-source obligations.

The checked-out OUI-SPY Unified Blue repository does not contain a `LICENSE`
file. No permissive reuse license is established by that repository snapshot.
This project therefore credits OUI-SPY as the reference/source project but does
not claim to relicense OUI-SPY material. Redistribution of copied OUI-SPY code
or binaries should be considered blocked pending explicit permission from its
author.

Third-party libraries retain their own licenses and notices. See the repository
`THIRD_PARTY_NOTICES` file before distributing binaries.
