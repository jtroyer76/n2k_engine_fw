# n2k_engine_fw — Specification

Firmware for the [Hat Labs HALMET](https://docs.hatlabs.fi/halmet/) that measures engine temperatures with 1-Wire probes and sends them on NMEA 2000, so chartplotters and Signal K see them without WiFi.

Status: draft, 2026-10-30. Nothing built yet. Claims not yet checked on hardware are marked **(verify)**.

## 1. Goals

- Engine temperatures on the NMEA 2000 bus, readable by chartplotters and by Signal K (canboatjs).
- **NMEA 2000 is the data path.** WiFi is for setup, firmware updates and a live debug view only. Losing WiFi never stops data on the bus.
- Additive only: the engine's own panel, senders and alarms stay as they are.
- Probes are assigned to roles (coolant, exhaust, engine room) in configuration, not in code.
- Fits alongside other devices that already send engine data (for example an alternator regulator sending RPM in PGN 127488): this firmware does not send RPM.

## 2. Non-goals (v1)

- RPM / tachometer input. Left for a later phase, for installs with no other RPM source.
- Alarm thresholds. Log a baseline first; thresholds come in a later phase.
- Analog senders (oil pressure, tank levels) and digital alarm inputs. Later phases.
- Signal K as a data path. Only if a value can't be expressed on NMEA 2000 (§6.4).

## 3. Hardware

- **Board:** Hat Labs HALMET — ESP32-WROOM-32E, isolated NMEA 2000 interface, 1-Wire header, 4 analog inputs (ADS1115), 4 digital inputs.
- **1-Wire:** data pin "DQ" is GPIO4 (HALMET docs: "the 1-Wire data pin (labeled "DQ") is physically mapped to GPIO4"). The header ships unsoldered. Pull-up on the board: **(verify)** from the schematic.
- **Probes:** DS18B20 waterproof probes, powered (3-wire) mode, not parasitic. Range −55 to +125 °C, so an exhaust probe goes on the water-cooled hose, never upstream of water injection.
- **Wiring:** one 3-wire run from the HALMET to a junction near the engine; each probe's cable lands there.

## 4. Sensors and roles

| Role | Typical mount | Purpose |
|---|---|---|
| `exhaust` | Wet exhaust hose, just downstream of the mixing elbow | Earliest warning of raw-water flow loss |
| `coolant` | Thermostat housing / coolant outlet, clamped | Heat exchanger, thermostat, coolant loss. A surface probe reads below true coolant temperature; alarm against a logged baseline. |
| `engine_room` | Air in the engine compartment, away from hot surfaces | Ventilation, general overheating |

Each role is configured with a DS18B20 ROM address (64-bit). A role with no address is not sent. Unknown probes found on the bus are listed in the debug view so they can be assigned.

## 5. NMEA 2000 output

PGN choices are based on what Raymarine Axiom (LightHouse 4) receives and displays, and on how Signal K's n2k-signalk maps them. Sources in §9.

### 5.1 Coolant → PGN 127489 Engine Parameters, Dynamic

- Field "Temperature" (K, 0.01 resolution). Axiom LH4 shows it as "Coolant temperature (PGN 127489)". Signal K maps it to `propulsion.<id>.temperature` (not `coolantTemperature`, which comes only from J1939).
- **Engine instance** configurable, default 0. Must match the instance used by whatever sends 127488 (RPM) for the same engine, so displays and Signal K treat them as one engine. Signal K names instance 0 `port` and 1 `starboard` (`propulsion.port.temperature` for a single engine).
- All other fields sent as "not available". Discrete status bits all clear in v1.
- Interval 500 ms (the PGN's standard rate).
- Built on `N2kEngineParameterDynamicSender` from the HALMET example firmware.

### 5.2 Exhaust → PGN 130316 Temperature, Extended Range

- Temperature Source 14 "Exhaust Gas Temperature". Axiom LH4 lists "Exhaust Gas Temperature (PGN 130316)" in its Engine data items. LH3 has no such item **(verify on LH3)**.
- Temperature instance configurable, default 0. Which instance the Axiom expects is undocumented **(verify)**.
- Signal K maps it to `propulsion.<instance>.exhaustTemperature` using the **numeric** instance, so a single engine ends up split: coolant under `propulsion.port`, exhaust under `propulsion.0`. Handle on the Signal K side (path mapping) or §6.4; firmware can't fix it on the bus.
- Interval 2 s.

### 5.3 Engine room → PGN 130316 Temperature, Extended Range

- Temperature Source configurable, default 3 "Engine Room Temperature". Signal K maps source 3 to `environment.inside.engineRoom.temperature`.
- Axiom LH4 has no "engine room" data item. It shows source 2 "Inside" as "Inside temperature", per the number of interior sensors set in Boat details. Whether source 3 shows anywhere is undocumented **(verify)**; if not, set this role to source 2 with its own instance.
- Instance configurable, default 0.
- Interval 2 s.

### 5.4 Not sent

- PGN 130312 Temperature: deprecated in favour of 130316 (NMEA2000 library: "Please use PGN 130316 (Temperature-Extended Range) for all new designs"). Axiom LH3 and LH4 both receive 130316.
- PGN 127488 (RPM): from another device.

### 5.5 Missing or failed probe

- A probe that stops answering is sent as "not available" (data value absent), never its last good reading. Displays then show dashes instead of a frozen number.
- A reading of exactly 85.0 °C on the first conversion is the DS18B20 power-on value and is discarded **(verify the library already does this)**.

## 6. Configuration and WiFi

### 6.1 Configuration

Through SensESP's web UI, stored on the device:

- Per role: probe address, enabled, N2K instance, temperature source (engine room), calibration offset (°C).
- Engine instance for 127489.
- Device name and NMEA 2000 device instance.

### 6.2 WiFi

- Station mode on the boat network; SensESP's setup hotspot when no network is configured.
- Used for: configuration, OTA updates, debug view (each probe's address, raw reading, role, last N2K send).
- Data never depends on it.

### 6.3 NMEA 2000

- Address claim and product information per the NMEA2000 library. Device class/function for a sensor/engine-monitoring device **(verify the right values)**.

### 6.4 Signal K client (optional, off by default)

Only for data that can't be expressed on NMEA 2000, or to fix the path split in §5.2 if Signal K mapping can't. No such data in v1. When enabled it is an extra output; NMEA 2000 output is unchanged.

## 7. Phases

1. **Desk: probes.** Start from the HALMET example firmware (SensESP, NMEA2000, NMEA2000_twai). 1-Wire on GPIO4 via SensESP OneWire. Debug view lists probe addresses and readings. Assign roles in config.
2. **Desk: NMEA 2000.** 127489 and 130316 as above, checked on a test bus with `candump` + canboat `analyzer` (field values, instance, source, interval) and in Signal K's data browser.
3. **Boat install.** Probes mounted, readings checked against a known thermometer at the mount points; engine instance set to match the RPM source; check what the chartplotter shows for exhaust and engine room (§5.2, §5.3) and set sources/instances accordingly. Log a baseline at idle and cruise.
4. **Alarms.** Thresholds from the baseline. Coolant over threshold sets the 127489 "Over Temperature" status bit (Axiom raises "Generic engine alerts" from 127489). Exhaust over threshold: status bit or Signal K notification **(decide)**.
5. **Later:** digital inputs for the engine's existing oil-pressure and temperature alarm lines (127489 status bits); analog oil pressure sender; tach input.

## 8. Build and test

- PlatformIO, environment from the HALMET example (`halmet_espidf`).
- Host tests for the pure logic: °C → K conversion and N2K scaling, role/address mapping, "not available" on stale data, power-on 85 °C discard.
- CI (GitHub Actions): firmware build + host tests.

## 9. Sources

- HALMET example firmware (Apache-2.0): https://github.com/hatlabs/HALMET-example-firmware — `src/n2k_senders.h` (`N2kEngineParameterDynamicSender`), `platformio.ini` (SensESP ^3.5.0, NMEA2000-library ^4.17.2, NMEA2000_twai).
- HALMET hardware docs: https://github.com/hatlabs/halmet (`docs/en/hardware/index.md`, 1-Wire on GPIO4).
- SensESP OneWire: https://github.com/SensESP/OneWire
- Raymarine LightHouse 4 PGN support: https://docs.raymarine.com/81406/en-US/latest/NMEA2000PGNSupport-946047B8.html
- Raymarine LH4 engine data items: https://docs.raymarine.com/81406/en-US/latest/EngineData-C78CBE53.html
- Raymarine LH4 inside environment data: https://docs.raymarine.com/81406/en-US/latest/InsideEnvironmentData-C791063A.html
- Raymarine LH4 NMEA 2000 alerts: https://docs.raymarine.com/81406/en-US/latest/NMEA2000Alerts-33B254EE.html
- canboat PGN database (127489 fields, TEMPERATURE_SOURCE): https://github.com/canboat/canboat
- NMEA2000 library (`tN2kTempSource`, 130312 deprecation note): https://github.com/ttlappalainen/NMEA2000
- n2k-signalk mappings: https://github.com/SignalK/n2k-signalk — `pgns/127489.ts`, `temperatureMappings.js`, `utils.ts`
