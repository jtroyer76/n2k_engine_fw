# n2k_engine_fw

Engine temperature monitoring on NMEA 2000 for the [Hat Labs HALMET](https://docs.hatlabs.fi/halmet/). DS18B20 1-Wire probes on the coolant outlet, the wet exhaust hose and the engine room air; readings go out on the NMEA 2000 bus for chartplotters and Signal K. WiFi is used only for setup, updates and a status page.

Status: early. Builds; not yet tested with probes or on a bus. Design and details: [docs/SPEC.md](docs/SPEC.md).

## What it sends

| Reading | PGN | Notes |
|---|---|---|
| Coolant | 127489 Engine Parameters, Dynamic | "Temperature" field; other fields not available. Engine instance configurable. |
| Exhaust | 130316 Temperature, Extended Range | Source 14, Exhaust Gas Temperature |
| Engine room | 130316 Temperature, Extended Range | Source 3, Engine Room (configurable) |
| Alarms | 127489 status bits | Over Temperature, Water Flow, Low Oil Pressure, Check Engine. All off until configured. |

## Wiring

- Probes: DS18B20, powered 3-wire. Data to the HALMET 1-Wire header "DQ" (GPIO4), power to 3.3 V, ground to GND. All probes share the same three wires.
- Optional engine panel warning lines: D2 (oil pressure), D3 (temperature). Off by default.

## Build and flash

```
pio run -e halmet_espidf                 # build
pio run -e halmet_espidf -t upload       # flash over USB
pio test -e native                       # host tests
```

To enable updates over WiFi, create `src/secrets.h` (gitignored):

```cpp
#define OTA_PASSWORD "choose-a-password"
```

## Setup

1. On first boot the HALMET starts a WiFi hotspot named `engine-temps` (password `thisisfine`, SensESP's default). Join it, open http://192.168.4.1 and add one or more WiFi networks; it joins whichever is in range.
2. Open `http://engine-temps.local`. Under Configuration, check which probe address each role (coolant, exhaust, engine room) picked up and correct it. Unused probes are listed on the status page. Changing an address needs a restart.
3. Set the engine instance to match whatever sends RPM for this engine.
4. After logging a baseline, set alarm thresholds.

Based on the [HALMET example firmware](https://github.com/hatlabs/HALMET-example-firmware) by Hat Labs (Apache-2.0).

## License

Apache-2.0. See [LICENSE](LICENSE).
