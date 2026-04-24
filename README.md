![Juh-Mak-In Jammer Logo](Juh-Mak-In%20Jammer.png)

# JUH-MAK-IN JAMMER — Counter-UAS RF Test Signal Emulator

**Professional test instrument for validating passive drone RF detection
systems across sub-GHz, 2.4 GHz, WiFi, and BLE.**

![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)
![Platform: ESP32-S3 + ESP32-C3](https://img.shields.io/badge/Platform-ESP32--S3%20%2B%20ESP32--C3-blue.svg)
![Version: v3.0.0](https://img.shields.io/badge/Version-v3.0.0-brightgreen.svg)

Built by [Seaforged](https://seaforged.io) — veteran-owned counter-UAS
infrastructure. Designed to test
[SENTRY-RF](https://github.com/Seaforged/Sentry-RF), an open-source passive
drone RF detector.

---

## What It Does

JUH-MAK-IN JAMMER (JJ) v3.0 is a **dual-board** calibrated RF signal
generator that emulates real drone control-link and Remote-ID emissions so
passive detection systems can be tested without real aircraft.

- Sub-GHz drone protocols: ELRS 900, TBS Crossfire, SiK/MAVLink, mLRS
- 2.4 GHz drone protocols: ELRS 2.4, Ghost, FrSky D16, FlySky AFHDS-2A, DJI energy
- Remote ID: ASTM F3411 ODID (WiFi beacon + BLE 4), DJI DroneID
- False-positive traffic: LoRaWAN (US915 / EU868), Meshtastic, Helium PoC
- 5 combined multi-emitter threat scenarios (`c1`–`c5`)
- Upstream-grounded packet content (CRC-14, X.25 CRC + CRC_EXTRA, CRSF
  frame, opendroneid-core-c) — not just an energy footprint

**JJ is NOT a jammer.** It does not disrupt communications. It is a
calibrated signal generator for controlled detection testing.

---

## Hardware Architecture

v3.0 is a **three-emitter** system driven from a single USB-C connection
to the T3S3 host:

| Emitter | Hardware | Role |
|---|---|---|
| **T3S3** | LilyGo T3-S3 V1.3 (ESP32-S3 + SX1262) | Primary sub-GHz LoRa/FSK |
| **XR1** (LR1121) | RadioMaster XR1 Nano (ESP32-C3 + LR1121) | 2.4 GHz LoRa/FSK |
| **XR1** (WiFi/BLE) | ESP32-C3 built-in WiFi + BLE | Remote ID transports |

The T3S3 controls the XR1 over a 115200 baud crossed UART link. See
[BUILD_GUIDE.md](BUILD_GUIDE.md) for BoM, wiring, and flashing.

---

## Supported Protocols

### Sub-GHz (T3S3 / SX1262)

| Cmd | Protocol | Bands | Rates | Fidelity |
|---|---|---|---|---|
| `e` | ExpressLRS FHSS | FCC915 / AU915 / EU868 / IN866 | 6 air rates (25–500 Hz) | **PASS** |
| `g` | TBS Crossfire FSK | 915 / 868 MHz | 150 Hz, 42.48 kHz deviation | **PARTIAL** |
| `gl` | Crossfire LoRa | 915 / 868 MHz | 50 Hz | FOOTPRINT |
| `k` | SiK Radio (MAVLink) | 915 MHz | 64 / 125 / 250 kbps | **PARTIAL** |
| `l` | mLRS | 915 MHz, 43 ch | 19 / 31 / 50 Hz | FOOTPRINT |
| `u` | Custom LoRa | 860–930 MHz | user-configurable | — |

### 2.4 GHz (XR1 / LR1121)

| Cmd | Protocol | Modulation | Fidelity |
|---|---|---|---|
| `x1`–`x4` | ELRS 2.4 (500 / 250 / 150 / 50 Hz) | LoRa FHSS, per-nonce CRC-14 | **PASS** |
| `x5` | ImmersionRC Ghost | GFSK | APPROXIMATE |
| `x6` | FrSky D16 | GFSK | FOOTPRINT |
| `x7` | FlySky AFHDS-2A | GFSK | FOOTPRINT |
| `x8` | DJI energy | LoRa-shaped | FOOTPRINT |
| `x9` | Generic 2.4 | user-configurable LoRa or FSK | — |

### Remote ID (XR1 WiFi / BLE)

| Cmd | Transport | Spec | Fidelity |
|---|---|---|---|
| `y1` | WiFi ODID beacon | ASTM F3411, Message Pack | **PASS** |
| `y2` | BLE 4 ODID | UUID 0xFFFA, legacy advertisement | **PARTIAL** |
| `y3` | DJI DroneID | OUI 26:37:12, Flight Telemetry | **PARTIAL** |
| `y4` | WiFi NaN | — | STUB (ESP32-C3 limitation) |
| `y` / `ya` | All three | — | — |

### Combined Scenarios

| Cmd | Scenario | Emitters |
|---|---|---|
| `c1` | Racing Drone | ELRS 200Hz + ELRS 500Hz 2.4 + WiFi/BLE ODID |
| `c2` | DJI Consumer | DJI energy + DJI DroneID + BLE ODID |
| `c3` | Long Range FPV | Crossfire 915 + WiFi/BLE ODID |
| `c4` | Dual-Band ELRS | ELRS sub-GHz + ELRS 2.4 |
| `c5` | Everything | Dual-band ELRS + all 3 RID transports |

### Infrastructure / False Positive

| Cmd | Source | Pattern |
|---|---|---|
| `i` | LoRaWAN US915 | 8 SB2 channels, 30–60 s |
| `f1` | Meshtastic | sync 0x2B, 16-sym preamble |
| `f2` | Helium PoC | 5 hotspots, rotating SB2 |
| `f3` | LoRaWAN EU868 | 868.1 / .3 / .5 MHz |
| `m` | Mixed FP | LoRaWAN + ELRS interleaved |

See [`docs/USER_GUIDE.md`](docs/USER_GUIDE.md) for the full command
reference (special modes, sweep, power ramp, drone swarm, controls).

---

## Protocol Fidelity Scorecard

| Rating | Meaning |
|---|---|
| **PASS** | Authentic at all 5 layers (energy, modulation, protocol-ID, content, behavior). |
| **PARTIAL** | RF envelope correct + some authentic content; gaps at content or behavior. |
| **FOOTPRINT** | Correct energy + modulation class, no authentic framing. |
| **APPROXIMATE** | Plausible modulation/energy, protocol proprietary and not reverse-engineered. |
| **STUB** | Unavailable due to hardware / API limitation. |

v3.0 achieves PASS on ELRS 900, ELRS 2.4, and ODID WiFi; PARTIAL on
Crossfire FSK, SiK/MAVLink, DJI DroneID, and ODID BLE.

---

## Quick Start

```bash
git clone https://github.com/seaforged-dev/Juh-Mak-In-Jammer.git
cd Juh-Mak-In-Jammer

# Build both firmwares
pio run -e t3s3
(cd xr1-firmware && pio run -e xr1)

# Flash T3S3 directly, flash XR1 via T3S3 passthrough — see BUILD_GUIDE.md
pio run -e t3s3 --target upload --upload-port COMx

# Open serial monitor
pio device monitor -b 115200
```

On boot, JJ prints the help menu. Type `h` or `?` to see it again.

---

## Project Structure

```
Juh-Mak-In-Jammer/
├─ src/                     # T3S3 host firmware (ESP32-S3 + SX1262)
├─ include/                 # T3S3 headers (board_config, protocol_params, …)
├─ xr1-firmware/            # XR1 secondary firmware (ESP32-C3 + LR1121)
│  ├─ src/
│  ├─ include/
│  └─ lib/opendroneid/      # Vendored opendroneid-core-c
├─ tools/xr1_passthrough/   # T3S3 sketch that bridges USB↔XR1 UART for flashing
├─ docs/
│  ├─ USER_GUIDE.md                        # Operator's manual
│  ├─ JJ_Protocol_Emulation_Reference_v2.md # Authoritative protocol parameters
│  ├─ JJ_v3_Consolidated_Roadmap.md         # Phase history
│  └─ …
├─ README.md
├─ BUILD_GUIDE.md                          # Hardware bring-up + flashing
└─ platformio.ini
```

---

## Documentation

- [USER_GUIDE.md](docs/USER_GUIDE.md) — Complete operator's manual
- [BUILD_GUIDE.md](BUILD_GUIDE.md) — Hardware setup, wiring, flashing
- [`docs/JJ_Protocol_Emulation_Reference_v2.md`](docs/JJ_Protocol_Emulation_Reference_v2.md)
  — Authoritative protocol parameters with upstream citations:
  ExpressLRS firmware, TBS Crossfire, ArduPilot SiK, mLRS (olliw42),
  opendroneid-core-c, ASTM F3411, LoRa Alliance RP002, ETSI EN 300 220,
  FCC 47 CFR 15.247.

---

## Legal Disclaimer

This tool transmits RF energy on ISM bands (868/915 MHz, 2.4 GHz) and
emits WiFi beacons and BLE advertisements. **The user is responsible for
compliance with all applicable local, national, and international RF
regulations** (e.g. FCC Part 15, ETSI EN 300 220, ETSI EN 300 328).

- Intended for testing passive detection systems in controlled environments.
- Use shielded enclosures or inline attenuators for conducted testing.
- Do **not** transmit on frequencies you are not authorized to use.
- Do **not** impersonate real aircraft. Remote ID emissions are **test
  beacons only**.
- Do **not** use to interfere with actual drone operations or safety-of-life
  communications.

The authors assume no liability for misuse. See [LICENSE](LICENSE) for
full terms.

---

## License

MIT License. See [LICENSE](LICENSE).

Built with care by [Seaforged](https://seaforged.io).
