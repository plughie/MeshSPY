<div align="center" markdown="1">

<img src=".github/meshtastic_logo.png" alt="Meshtastic Logo" width="80"/>
<h1>Meshtastic Firmware</h1>

![GitHub release downloads](https://img.shields.io/github/downloads/meshtastic/firmware/total)
[![CI](https://img.shields.io/github/actions/workflow/status/meshtastic/firmware/main_matrix.yml?branch=master&label=actions&logo=github&color=yellow)](https://github.com/meshtastic/firmware/actions/workflows/ci.yml)
[![CLA assistant](https://cla-assistant.io/readme/badge/meshtastic/firmware)](https://cla-assistant.io/meshtastic/firmware)
[![Fiscal Contributors](https://opencollective.com/meshtastic/tiers/badge.svg?label=Fiscal%20Contributors&color=deeppink)](https://opencollective.com/meshtastic/)
[![Vercel](https://img.shields.io/static/v1?label=Powered%20by&message=Vercel&style=flat&logo=vercel&color=000000)](https://vercel.com?utm_source=meshtastic&utm_campaign=oss)
[![MemBrowse](https://membrowse.com/badge.svg)](https://membrowse.com/public/meshtastic/firmware)

<a href="https://trendshift.io/repositories/5524" target="_blank"><img src="https://trendshift.io/api/badge/repositories/5524" alt="meshtastic%2Ffirmware | Trendshift" style="width: 250px; height: 55px;" width="250" height="55"/></a>

</div>

## MeshSPY

MeshSPY is a T-Deck Plus firmware fork that combines Meshtastic mesh
communication with the OUI-SPY Unified Blue detector feature set. It is a
[Bug-Mag.net](https://a-bug-mag.net) Project by DuvTheDove
([GitHub](https://github.com/plughie)), with implementation assistance from
**OpenAI Codex**.

The primary target is the LILYGO T-Deck Plus (`t-deck-tft`). MeshSPY preserves
the normal Meshtastic UI and LoRa operation while adding an OUI-SPY application
for cooperative Wi-Fi/BLE detection and alert policy.

### Included capabilities

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

### Build and flash

See [build, flash, and test procedures](docs/BUILD_FLASH_TEST.md), the
[integration assessment](docs/MESHSPY_INTEGRATION_ASSESSMENT.md), and the
[SD-card watchlist guide](docs/OUI_SPY_WATCHLIST.md). Release installation
instructions are in [INSTALL.md](INSTALL.md).

```sh
pio run -e t-deck-tft
```

The project is still under hardware validation. A successful build does not by
itself verify RF coexistence, detector fidelity, SD persistence, or ATAK output.

### Upstream projects

- [Meshtastic firmware](https://github.com/meshtastic/firmware)
- [OUI-SPY Unified Blue](https://github.com/colonelpanichacks/oui-spy-unified-blue)

### License and attribution

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

</div>

<div align="center">
	<a href="https://meshtastic.org">Website</a>
	-
	<a href="https://meshtastic.org/docs/">Documentation</a>
</div>

## Overview

This repository contains the official device firmware for Meshtastic, an open-source LoRa mesh networking project designed for long-range, low-power communication without relying on internet or cellular infrastructure. The firmware supports various hardware platforms, including ESP32, nRF52, RP2040/RP2350, and Linux-based devices.

Meshtastic enables text messaging, location sharing, and telemetry over a decentralized mesh network, making it ideal for outdoor adventures, emergency preparedness, and remote operations.

### Get Started

- 🔧 **[Building Instructions](https://meshtastic.org/docs/development/firmware/build)** - Learn how to compile the firmware from source.
- ⚡ **[Flashing Instructions](https://meshtastic.org/docs/getting-started/flashing-firmware/)** - Install or update the firmware on your device.

Join our community and help improve Meshtastic! 🚀

## Stats

![Alt](https://repobeats.axiom.co/api/embed/8025e56c482ec63541593cc5bd322c19d5c0bdcf.svg "Repobeats analytics image")
