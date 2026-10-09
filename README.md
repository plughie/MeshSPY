# MeshSPY

MeshSPY is a T-Deck Plus firmware fork that combines Meshtastic mesh
communication with the OUI-SPY Unified Blue detector feature set. It is a
[Bug-Mag.net](https://a-bug-mag.net) Project by DuvTheDove
([GitHub](https://github.com/plughie)), with implementation assistance from
**OpenAI Codex**.

The primary target is the LILYGO T-Deck Plus (`t-deck-tft`). MeshSPY preserves
the normal Meshtastic UI and LoRa operation while adding an OUI-SPY application
for cooperative Wi-Fi/BLE detection and alert policy.

## Alpha release

Download [MeshSPY v0.1.0-alpha.1](https://github.com/plughie/MeshSPY/releases/tag/v0.1.0-alpha.1).
The release ZIP contains application and factory firmware images, the LittleFS
image, installation instructions, release notes, and checksums.

## Included capabilities

- Meshtastic SX1262 mesh participation remains active while OUI-SPY services run.
- OUI-SPY detector integration for Flock-You, Foxhunter, Sky-Spy, and BLE Sniff.
- Persistent observation history and configuration on the T-Deck Plus SD card,
  with PSRAM-backed working state where appropriate.
- GPS-enriched observations and an architecture for SD-card ATAK/Cursor-on-Target
  export and later live transport.
- Configurable, rate-limited, deduplicated human-readable alerts to a standard
  private Meshtastic channel.
- SD-card watchlists for exact BLE MAC addresses, OUIs, Bluetooth company IDs,
  service UUIDs, and advertised-name fragments.

## Build and install

See [INSTALL.md](INSTALL.md) for release installation and recovery procedures.
See [build, flash, and test procedures](docs/BUILD_FLASH_TEST.md) for source
builds and hardware validation.

```sh
pio run -e t-deck-tft
```

Additional technical documentation:

- [Integration assessment](docs/MESHSPY_INTEGRATION_ASSESSMENT.md)
- [SD-card watchlist guide](docs/OUI_SPY_WATCHLIST.md)
- [Alpha release notes](docs/RELEASE_NOTES_v0.1.0-alpha.1.md)

## Upstream projects

- [Meshtastic firmware](https://github.com/meshtastic/firmware)
- [OUI-SPY Unified Blue](https://github.com/colonelpanichacks/oui-spy-unified-blue)

## License and attribution

MeshSPY inherits the repository’s existing **GNU General Public License,
version 3** from Meshtastic; see [LICENSE](LICENSE). Meshtastic notices and
the obligations for corresponding source remain applicable to distributed
firmware.

OUI-SPY Unified Blue is credited as the behavioral/reference source above. The
referenced OUI-SPY repository does not publish a license file, so this fork does
not represent its source as separately relicensed. Any copied OUI-SPY code or
binary redistribution should be reviewed with its author before distribution.
See [THIRD_PARTY_NOTICES](THIRD_PARTY_NOTICES) for provenance and licensing
notes.

## Project status

This project is under active alpha development and hardware validation. A
successful build does not by itself verify every RF coexistence, detector, SD
persistence, or ATAK function on physical hardware.
