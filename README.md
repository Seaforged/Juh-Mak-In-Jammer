![Juh-Mak-In Jammer Logo](Juh-Mak-In%20Jammer.png)
# JUH-MAK-IN JAMMER — Sub-GHz Drone Signal Emulator

**Professional test instrument for validating passive drone RF detection systems**

![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)
![Platform: ESP32-S3](https://img.shields.io/badge/Platform-ESP32--S3-blue.svg)
![Version: v2.0.0](https://img.shields.io/badge/Version-v2.0.0-orange.svg)

Part of the [Seaforged](https://github.com/Seaforged) counter-UAS test infrastructure. Designed to test [SENTRY-RF](https://github.com/Seaforged/Sentry-RF), an open-source passive drone RF detector.

---

## What It Does

JUH-MAK-IN JAMMER (JJ) simulates real-world drone RF control protocols so detection systems can be tested without real drones. It generates authentic LoRa FHSS, GFSK, and WiFi signals that exercise every detection path a passive RF sensor needs to cover.

- **16+ operating modes** across 5 drone protocol families
- **Protocol parameters sourced from open-source firmware** (ExpressLRS, SiK, mLRS) with regulatory standard citations
- **False positive testing** against LoRaWAN, Meshtastic, and Helium infrastructure patterns
- **All constants in a single header** (`protocol_params.h`) with inline v2 reference citations

**This is not a jammer.** It does not disrupt communications. It is a calibrated RF signal generator for controlled detection system testing.

---

## Supported Protocols

### Drone Protocols

| Protocol | Modulation | Bands | Channels | Hop Rate | Serial | Description |
|----------|-----------|-------|----------|----------|--------|-------------|
| **ExpressLRS** | LoRa FHSS | FCC915, AU915, EU868, IN866 | 4-40 | 6-250 hops/s | `e` | 6 air rates (SF5-SF9), 4 domains, binding state, sync channel. CR 4/7, 6-sym preamble, real LCG constants. |
| **TBS Crossfire** | GFSK FHSS | 915 MHz | 100 | 150 hops/s | `g` | 85.1 kbps FSK, 260 kHz spacing. |
| **SiK Radio** | GFSK TDM+FHSS | 915 MHz | 50 | ~50 hops/s | `k` | MAVLink telemetry link (ArduPilot/PX4). 64/125/250 kbps. Guard-band channel formula. |
| **mLRS** | LoRa/FSK FHSS | 915 MHz | 20 | 9.5-25 hops/s | `l` | Symmetric TX/RX alternation. 19/31/50 Hz modes. Slowest FHSS pattern for detection testing. |
| **Custom LoRa** | User-configurable | 860-930 MHz | 1-16 | 1-100 Hz | `u` | Any SF (5-12), BW (7.8-500 kHz), frequency, hop pattern. Tests detection of unknown drone signatures. |
| **WiFi Remote ID** | WiFi + BLE | 2.4 GHz | - | 1 Hz | `r` | ASTM F3411 Open Drone ID beacon spoofing. |
| **Drone Swarm** | WiFi | 2.4 GHz | - | 20 Hz | `w` | 1-16 virtual drones with unique IDs and circular orbits. |

### ELRS Air Rates

| Command | Rate | SF | Hop Interval | DVDA | Use Case |
|---------|------|----|-------------|------|----------|
| `e1` | 200 Hz | SF6 | 4 | No | Racing / freestyle |
| `e2` | 100 Hz | SF7 | 4 | No | Balanced |
| `e3` | 50 Hz | SF8 | 4 | No | Long range |
| `e4` | 25 Hz | SF9 | 4 | No | Ultra long range |
| `e5` | 250 Hz | SF6 | 2 | Yes | DVDA fast |
| `e6` | 500 Hz | SF5 | 2 | Yes | DVDA fastest |

Append domain letter: `e1f` (FCC915), `e1a` (AU915), `e1u` (EU868), `e1i` (IN866). Append `b` for binding state: `e1fb`.

### False Positive Testing

These modes simulate non-drone LoRa infrastructure that detection systems must correctly reject:

| Mode | Sync Word | Preamble | Pattern | Serial | Why It Matters |
|------|-----------|----------|---------|--------|----------------|
| **LoRaWAN US915** | 0x34 (public) | 8 sym | 8 SB2 channels, 30-60s interval | `i` | Standard IoT — must not trigger alerts |
| **LoRaWAN EU868** | 0x34 (public) | 8 sym | 868.1/868.3/868.5 MHz, ~60s | `f3` | EU band coverage |
| **Meshtastic** | 0x2B | 16 sym | Fixed frequency, periodic beacon + relay cascade | `f1` | Different sync word and long preamble |
| **Helium PoC** | 0x34 (public) | 8 sym | 5 hotspots, rotating SB2 channels | `f2` | Hardest FP pattern: slow multi-channel diversity |
| **Mixed** | Both | Both | ELRS FHSS + LoRaWAN interleaved | `m` | Combined drone + infrastructure |

---

## Hardware

| Component | Details |
|-----------|---------|
| Board | LilyGo T3S3 V1.3 (ESP32-S3 + SX1262) |
| Radio | Semtech SX1262: 150-960 MHz, LoRa SF5-SF12, FSK 0.6-300 kbps, +22 dBm max |
| Display | SSD1306 128x64 OLED via I2C |
| Antenna | SMA connector, 868/915 MHz whip antenna |
| Power | USB-C, 5V |
| Build system | PlatformIO (Arduino framework) |

Same board as SENTRY-RF. Either firmware can be flashed to the same hardware.

---

## Serial Commands

All modes controllable at 115200 baud. Type `h` or `?` for the built-in help menu.

```
DRONE PROTOCOLS:
  e  ELRS FHSS      e1-e6=rate  f/a/u/i=domain  b=binding
  g  Crossfire FSK   150Hz GFSK 85.1kbps FHSS
  k  SiK Radio       k1=64k  k2=125k  k3=250k
  l  mLRS            l1=19Hz  l2=31Hz  l3=50Hz(FSK)
  u  Custom LoRa     u?=settings  uf/us/ub/ur/uh/up/uw=config

INFRASTRUCTURE (False Positive Testing):
  i  LoRaWAN US915   Single node, 8 SB2 channels, 30-60s
  m  Mixed FP        LoRaWAN + ELRS interleaved
  f1 Meshtastic      16-sym preamble, sync 0x2B
  f2 Helium PoC      5 hotspots, rotating SB2 channels
  f3 LoRaWAN EU868   868.1/868.3/868.5 MHz

SPECIAL MODES:
  c  CW Tone         b=sweep  t=power ramp
  r  Remote ID       WiFi+BLE ASTM F3411 broadcast
  x  Combined        RID(Core0) + ELRS(Core1)
  w  Drone Swarm     n=cycle count (1/4/8/16)

CONTROLS:
  q  Stop TX         p=cycle power  h/?=this menu
```

---

## Quick Start

```bash
# Clone
git clone https://github.com/Seaforged/Juh-Mak-In-Jammer.git
cd Juh-Mak-In-Jammer

# Build
pio run -e t3s3

# Flash
pio run -e t3s3 --target upload

# Monitor
pio device monitor -b 115200
```

On boot, JJ prints the help menu. Type `e` to start ELRS 200 Hz FCC915 simulation, or `h` to see all commands.

---

## Protocol Reference

All protocol parameters are documented in [`docs/JJ_Protocol_Emulation_Reference_v2.md`](docs/JJ_Protocol_Emulation_Reference_v2.md), with citations to:

- **ExpressLRS firmware** (`FHSS.cpp`, `rx_main.cpp`) for channel tables, hop intervals, LCG constants
- **ArduPilot SiK documentation** for channel formulas and TDM behavior
- **LoRa Alliance RP002-1.0.2** for LoRaWAN channel plans
- **ETSI EN 300 220 V3.1.1** for EU868 duty cycle regulations
- **FCC 47 CFR 15.247** for US ISM band requirements
- **NCC Group** and **GNU Radio research** confirming ELRS FHSS sequence generation

Constants marked `[VERIFY]` in the reference document have not been confirmed against primary sources and are noted with TODO comments in the code.

---

## Automated Testing

JJ is designed for dual-device automated testing against SENTRY-RF. Python test scripts in the [SENTRY-RF repository](https://github.com/Seaforged/Sentry-RF):

```bash
python run_test.py e   # Test ELRS FHSS detection
python run_test.py c   # Test CW tone detection
python full_validation.py  # Run all modes, generate PASS/FAIL summary
```

---

## Legal Disclaimer

This tool transmits RF energy on ISM bands (868/915 MHz) and WiFi (2.4 GHz). **The user is responsible for compliance with all applicable local, national, and international RF regulations.**

- Intended for testing detection systems in controlled environments
- Use shielded enclosures or inline attenuators for conducted testing
- Do not use to interfere with actual drone operations or communications
- Do not transmit on frequencies you are not authorized to use
- Remote ID spoofing generates test beacons only — do not use to impersonate real aircraft

The authors assume no liability for misuse. See [LICENSE](LICENSE) for full terms.

## License

MIT License. See [LICENSE](LICENSE).
