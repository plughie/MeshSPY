# SD-card watchlist

MeshSPY reads this file at boot:

```text
/OUI-SPY/watchlist.csv
```

Create the `OUI-SPY` directory on the T-Deck Plus SD card, then create
`watchlist.csv`. The file is line-oriented and uses three comma-separated
fields:

```text
TYPE,VALUE,LABEL
```

Supported types:

```text
OUI,AA:BB:CC,My device family
MAC,AA:BB:CC:DD:EE:FF,My unique device
CID,034D,Bluetooth company identifier
SVC,FC81,Bluetooth service UUID
NAME,My Device,Advertised name fragment
DRONE,Remote-ID-123,Known drone Remote ID
```

Lines beginning with `#` and blank lines are ignored. Separators in OUI,
MAC, CID, and SVC values are ignored. NAME matching is case-insensitive.

Enable **Tools → OUI-SPY → Watchlist** and reboot after changing the file.
Watchlist matches are logged and can generate ordinary Meshtastic alerts when
**Mesh alerts** is enabled. The built-in Axon signatures remain available as
fallbacks when the Watchlist setting is enabled.

Drone Remote IDs are separate from BLE vendor/device matching. Enable both
**Sky-Spy** and **Drone IDs** in **Tools → OUI-SPY** to enable the decoder.
Sky-Spy recognizes OpenDroneID Basic ID messages in BLE service-data
advertisements and supported Wi-Fi management-frame vendor elements. `DRONE`
entries are exact matches after lowercasing and removing spaces, colons, and
hyphens. Drone events are written to the same JSONL history with
`drone_id_new`, `drone_id_returned`, `drone_id_watchlist_match`, and
`drone_id_lost` event names. Ordinary BLE advertisements are not treated as
drone IDs. Wi-Fi decoding listens on the current Wi-Fi channel so the
Meshtastic network connection is not disrupted; channel hopping is not yet
enabled.

## Detection history

BLE state transitions are appended to:

```text
/OUI-SPY/observations.jsonl
```

Each line is a JSON object. The current implementation records BLE transition
events and normalized drone-ID transition events, including uptime, identifier,
RSSI, and GPS coordinates when GPS inclusion is enabled and a fix is available.
It deliberately does not write every repeated advertisement, which limits SD
wear and shared-SPI contention.
