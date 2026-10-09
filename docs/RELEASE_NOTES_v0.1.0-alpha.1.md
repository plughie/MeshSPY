# MeshSPY v0.1.0-alpha.1

First public alpha release of MeshSPY for the LILYGO T-Deck Plus.

This release includes the Meshtastic T-Deck TFT firmware with the OUI-SPY
application integration, including BLE observation, watchlist matching,
deduplicated alert handling, standard private-channel message support, and the
MeshSPY Tools interface.

Included artifacts:

- Application-only firmware image for normal updates.
- Factory image containing bootloader, partition table, and application.
- LittleFS web/filesystem image.

See [INSTALL.md](../INSTALL.md) for installation instructions and
[BUILD_FLASH_TEST.md](BUILD_FLASH_TEST.md) for source-build procedures.

This is an alpha release. Physical validation remains ongoing for complete
detector coverage, RF coexistence, persistent observation storage, and ATAK
output.
