# JUH-MAK-IN JAMMER — RF Test Tool

![JUH-MAK-IN JAMMER](docs/logo.png)

**ESP32-based drone signal simulator and Remote ID spoofer for testing [SENTRY-RF](https://github.com/Seaforged/SENTRY-RF)**

## What it does

JAMMER-RF is the companion test tool for the SENTRY-RF passive drone detector. It generates the exact signals SENTRY-RF is designed to detect — fake drone control links, spoofed Remote ID broadcasts, and ISM band noise — so you can validate every detection algorithm under controlled, repeatable conditions.

### Modes

| Mode | What It Generates | What It Tests on SENTRY-RF |
|------|-------------------|---------------------------|
| **Remote ID Spoofer** | Fake Open Drone ID via WiFi beacons + BLE advertising (1–16 drone swarm) | Sprint 8 WiFi scanner, drone tracking |
| **RF Signal Generator** | ELRS/Crossfire LoRa/FSK signals on 868/915 MHz | Sprint 2 spectrum scanner, Sprint 6 detection engine |
| **False Positive Generator** | LoRaWAN IoT packets + random ISM bursts (NOT drones) | False alarm rejection, signal discrimination |
| **Combined Attack** | All signals simultaneously with configurable timing | Sprint 6 threat correlation, escalation ladder |

### Key design principles

- Every test produces **measurable results** — detection probability, time-to-detect, false alarm rate
- Includes both easy-to-detect AND hard-to-detect scenarios
- **False positive testing** is treated as equally important as true positive testing
- Parameters can be **randomized** to prevent bias from the developer who also builds the detector

## Hardware

**LilyGo T3S3** — ESP32-S3 with SX1262 LoRa radio, SSD1306 OLED, and onboard WiFi/BLE. No additional hardware required.

## Quick start

### Prerequisites
- [PlatformIO](https://platformio.org/) (CLI or VS Code extension)
- LilyGo T3S3 board

### Build and flash
```bash
git clone https://github.com/Seaforged/juh-mak-in-jammer-rf.git
cd juh-mak-in-jammer-rf
pio run
pio run --target upload
pio device monitor
```

## Current status

**Mode 2: RF Signal Generator** — three sub-modes working and tested:

| Sub-mode | Status | Description |
|----------|--------|-------------|
| CW Tone | Working | Continuous carrier on configurable frequency presets (868–925 MHz), cycle with button |
| Band Sweep | Working | Linear sweep 860–930 MHz, configurable step size (100 kHz–5 MHz) and dwell time (500 µs–50 ms) |
| ELRS 915 FHSS | Working | LoRa SF6 BW500, 80-channel pseudo-random frequency hopping at 200 Hz, matches real ELRS 200Hz mode |

**Menu system** — OLED display with BOOT button navigation (short press = cycle, long press = select).

**Serial commands** — runtime parameter control during TX:
- `d` — cycle dwell time (sweep mode)
- `s` — cycle step size (sweep mode)
- `p` — cycle TX power (-9 to +22 dBm, all modes)
- `q` — emergency stop TX

See the [full build plan](docs/BUILD_PLAN_v2.md) for remaining work.

## Part of the JUH-MAK-IN JAMMER test suite

| Tool | Platform | Purpose |
|------|----------|---------|
| **juh-mak-in-jammer-rf** (this repo) | LilyGo T3S3 | Remote ID spoofing, sub-GHz drone signals, false positive generation |
| [juh-mak-in-jammer-gps](https://github.com/Seaforged/juh-mak-in-jammer-gps) | Python + AntSDR E200 | GPS spoofing, GPS jamming, combined GNSS attack scenarios |

## Legal

All testing must be conducted in RF-shielded environments or via conducted (cabled) connections. Broadcasting fake Remote ID in public airspace is likely illegal. Transmitting on ISM bands is subject to power limits and local regulations. This tool is for authorized security research and detector validation only.

## License

[MIT](LICENSE)

## Contributing

Issues and PRs welcome. This is a learning project built in public.
