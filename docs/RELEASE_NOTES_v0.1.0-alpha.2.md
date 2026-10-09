# MeshSPY v0.1.0-alpha.2

This alpha updates the T-Deck Plus firmware to the 2.8.3 build line and
addresses sustained BLE advertiser traffic in busy RF environments.

Changes include:

- Generic rotating BLE advertisers remain visible in serial diagnostics but no
  longer create one SD transaction or mesh alert per address.
- Generic BLE activity is aggregated into periodic `ble_summary` records in
  `/OUI-SPY/observations.jsonl`.
- Watchlist matches, watchlist returns/losses, drone IDs, and detector events
  remain individually persisted and alertable.
- The SD observation queue remains bounded and drains in batches.

The release contains application-only, factory, and LittleFS images for the
LILYGO T-Deck Plus `t-deck-tft` environment, plus installation instructions and
checksums.

This is an alpha release. Physical validation remains ongoing for complete
detector coverage, RF coexistence, persistent storage, and ATAK output.
