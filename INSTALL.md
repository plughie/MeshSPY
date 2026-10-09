# MeshSPY installation

MeshSPY alpha targets the LILYGO T-Deck Plus with the `t-deck-tft` Meshtastic
environment.

## Recommended update

The application-only image updates firmware without erasing the T-Deck Plus
SD card or its existing persistent data:

```sh
esptool --chip esp32s3 --port PORT write-flash 0x10000 \
  firmware-t-deck-tft-2.8.2.364a111.bin
```

Replace `PORT` with the device port, such as `/dev/cu.usbmodem141101` on macOS.
The device may reboot into USB programming mode during the operation.

## First installation or recovery

The `*.factory.bin` image contains the bootloader, partition table, boot app,
and application image. It is intended for a complete firmware installation:

```sh
esptool --chip esp32s3 --port PORT write-flash 0x0 \
  firmware-t-deck-tft-2.8.2.364a111.factory.bin
```

Back up important device settings first. A factory image can replace the
existing application partition layout. It does not replace the removable SD
card.

The included `littlefs-*.bin` is the web/filesystem image produced by
PlatformIO. Use the project’s PlatformIO target when updating it:

```sh
pio run -e t-deck-tft -t uploadfs --upload-port PORT
```

Do not erase or reformat the SD card as part of a normal firmware update.

## After flashing

1. Boot the T-Deck Plus into the normal Meshtastic UI.
2. Open `Tools → OUI-SPY` and enable the desired detectors.
3. Configure Mesh alerts and the standard private Meshtastic channel if mesh
   notifications are wanted.
4. For SD-card watchlists, create `/OUI-SPY/watchlist.csv`; see
   [the watchlist guide](docs/OUI_SPY_WATCHLIST.md).
5. Reboot after changing the watchlist file.

This is an alpha release. Build success does not by itself verify every RF
coexistence, detector, SD persistence, or ATAK function on physical hardware.
