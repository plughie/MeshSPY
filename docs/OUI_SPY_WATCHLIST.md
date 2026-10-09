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
```

Lines beginning with `#` and blank lines are ignored. Separators in OUI,
MAC, CID, and SVC values are ignored. NAME matching is case-insensitive.

Enable **Tools → OUI-SPY → Watchlist** and reboot after changing the file.
Watchlist matches are logged and can generate ordinary Meshtastic alerts when
**Mesh alerts** is enabled. The built-in Axon signatures remain available as
fallbacks when the Watchlist setting is enabled.
