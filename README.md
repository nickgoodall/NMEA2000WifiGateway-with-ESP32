# OpenMarine Gateway Platform

An open source marine data platform built on ESP32. Ingests NMEA2000, NMEA0183,
BLE sensors (Ruuvi, Victron), and local sensors into a unified Signal K-compatible
data store. Serves a real-time web dashboard over WiFi and publishes telemetry to
cloud MQTT for remote monitoring.

**Status**: Active development — proof-of-concept running on hardware, clean
architecture implementation in progress. See [docs/PRD.md](docs/PRD.md) and
[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the full design.

---

## Origins and Attribution

This project is a fork and significant extension of the **NMEA2000 WiFi Gateway**
by **Andreas Kirmse (AK-Homberger)**, originally published at:

- GitHub: https://github.com/AK-Homberger/NMEA2000WifiGateway-with-ESP32
- Open Boat Projects: https://open-boat-projects.org/en/nmea2000-with-wi-fi-and-nmea-0183-and-can-bus/

The original work provided the NMEA2000/0183 gateway foundation and is licensed
under the GNU Lesser General Public License v2.1 or later (LGPL-2.1+). The original
source files in `Wifi-Serial-AP-AIS-CAN/` retain their original license headers.

The NMEA2000 and NMEA0183 libraries are the work of
**Timo Lappalainen** (https://github.com/ttlappalainen).

See [NOTICE](NOTICE) for full third-party attribution.

---

## What's working (proof of concept)

Running on NodeMCU-32S + SN65HVD230 CAN transceiver:

- NMEA2000 + NMEA0183 parsing (GPS, COG/SOG, heading, depth, wind, water temp)
- WiFi AP+STA dual mode, mDNS (`nmea2000.local`)
- Real-time web dashboard with navigation, environment, and map tabs
- Local sensors: DS18B20 fridge temperature, battery voltage (ADC)
- Ruuvi BLE passive scanning (temperature, humidity, pressure, acceleration, heel/pitch)
- Cloud MQTT over TLS to HiveMQ (1-minute telemetry publish interval)
- Web settings UI: WiFi, MQTT, BLE sensor labels
- Factory reset via 5-second button hold

---

## Planned architecture

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the full design. Key principles:

- **Unified data store** — all sensor data (NMEA2000, BLE, I2C, ADC) flows through
  a single store using Signal K path strings and SI units
- **Pluggable BLE drivers** — new sensor types added as a single `.cpp` file
- **Signal K compatible** — mDNS discovery, WebSocket delta stream, REST API
- **Hardware capability detection** — one binary adapts to NodeMCU, WROVER, or
  future commercial PCB (ESP32-S3 with PSRAM + microSD)
- **SD card features** — store-and-forward telemetry, passage recording,
  dashboard served from SD without firmware reflash

---

## Hardware (prototype)

| Component | Part |
|---|---|
| Microcontroller | NodeMCU-32S (ESP32) |
| CAN transceiver | SN65HVD230 breakout |
| Fridge temp | DS18B20 (1-Wire, GPIO13) |
| Battery voltage | Resistor divider → GPIO34 (ADC) |

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) §2 for full pin mapping and
the commercial target hardware specification.

---

## Build

Requires [PlatformIO](https://platformio.org/).

```bash
git clone https://github.com/YOUR_USERNAME/NMEA2000WifiGateway-with-ESP32
cd NMEA2000WifiGateway-with-ESP32
# Open PlatformIO project at:
# c:/Users/goodall/Documents/PlatformIO/Projects/260514-235225-nodemcu-32s
pio run --target upload
```

All library dependencies are declared in `platformio.ini` and installed automatically.

---

## License

New source files are released under the **Apache License 2.0**.
See [LICENSE](LICENSE) for full terms.

The proof-of-concept files in `Wifi-Serial-AP-AIS-CAN/` retain their original
**LGPL-2.1+** headers from the upstream project by AK-Homberger.
See [NOTICE](NOTICE) for full third-party attribution.

---

## Original project history (AK-Homberger)

- 06.11.20: v1.3 — Changed PCB layout to v1.1
- 04.08.20: v1.3 — Changed CAN_TX to GPIO5, added WLAN client mode
- 23.07.20: v1.1 — Moved to JSON library v6
- 03.08.19: v0.4 — Added rudder, log, TWS/TWD, web gauge
- 23.07.19: v0.2 — Fixed AIS full message forwarding
