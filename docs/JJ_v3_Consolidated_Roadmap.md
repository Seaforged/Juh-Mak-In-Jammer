# JJ v3.0 — Consolidated Development Roadmap
## From Sub-GHz Test Instrument to Full-Spectrum Drone Signal Emulator

**Document Version:** 1.0  
**Date:** April 12, 2026  
**Authors:** ND (Seaforged), Claude (Technical Lead)  
**Supersedes:** JJ_XR1_Phased_Development_Plan.md (April 11, 2026)  
**Status:** ACTIVE — This is the master roadmap for all JJ development  

---

## Current State (Post-Audit, April 12, 2026)

**Hardware:**
- T3S3 V1.3 (ESP32-S3 + SX1262) — primary sub-GHz emitter, firmware at v2.0.0 on COM6
- RadioMaster XR1 (ESP32C3 + LR1121) — purchased, not yet wired, firmware scaffold committed (xr1-firmware/)

**Firmware status (main branch, commit 9cdcb63):**
- 7/7 critical audit findings fixed (BLE overflow, infra buffer, implicit header, hop dedup, ODID direction/vspeed/timestamp)
- Sub-GHz protocols working: ELRS (7 domains × 6 rates), Crossfire 915 FSK, SiK MAVLink, mLRS, custom LoRa
- Infrastructure modes working: LoRaWAN US915/EU868, Meshtastic, Helium, Dense Mixed
- Remote ID working: WiFi beacon + BLE advertisement (ASTM F3411 ODID), Swarm (1-16 drones)
- Combined mode working: ELRS FHSS + RID simultaneous (dual-core)
- XR1 firmware scaffold committed but not flashed (Phase 0 hardware work pending)

**What's missing:**
- 2.4 GHz RF capability (requires XR1 hardware bring-up)
- DJI DroneID emission (requires XR1 WiFi radio)
- BLE ODID spec compliance improvements (Location weighting, AD counter)
- Crossfire 868 EU band (defined but unreachable in code)
- Crossfire dual-modulation (FSK+LoRa TDM)
- WiFi NaN transport for Remote ID
- startTransmit() return value checking
- Combined mode task join safety
- Remaining WARNING-level audit findings

---

## Architecture Overview

```
┌──────────────────────────────────────────────────────────┐
│  T3S3 (ESP32-S3) — JJ Main Controller                   │
│  ┌───────────┐  ┌──────────────┐  ┌───────────────────┐ │
│  │ SX1262    │  │ UART1 to XR1 │  │ WiFi (internal)   │ │
│  │ Sub-GHz   │  │ GPIO 43/44   │  │ RID beacon        │ │
│  │ 150-960   │  │ Command bus  │  │ Swarm beacons     │ │
│  │ MHz       │  │ 115200 baud  │  │ DJI DroneID (alt) │ │
│  │ LoRa/FSK  │  │              │  │                   │ │
│  └───────────┘  └──────┬───────┘  └───────────────────┘ │
│  ┌───────────┐         │          ┌───────────────────┐ │
│  │ BLE       │         │          │ Serial Console    │ │
│  │ (internal)│         │          │ USB CDC           │ │
│  │ RID advert│         │          │ Operator commands │ │
│  └───────────┘         │          └───────────────────┘ │
└────────────────────────┼────────────────────────────────┘
                         │  4-wire (5V, GND, TX, RX)
┌────────────────────────┼────────────────────────────────┐
│  XR1 (ESP32C3) — Dual-Band + WiFi/BLE Emitter          │
│  ┌───────────┐  ┌──────┴───────┐  ┌───────────────────┐│
│  │ LR1121    │  │ UART from    │  │ WiFi (internal)   ││
│  │ Sub-GHz   │  │ T3S3 host    │  │ ODID beacon       ││
│  │ + 2.4 GHz │  │ Commands     │  │ DJI DroneID       ││
│  │ LoRa/GFSK │  │              │  │ WiFi NaN          ││
│  └───────────┘  └──────────────┘  └───────────────────┘│
│  ┌───────────────────────────────────────────────────┐  │
│  │ BLE 5.0 (internal)                                │  │
│  │ ODID Legacy ADV + BLE 5 Long Range (stretch)      │  │
│  └───────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────┘
```

**Four independent RF emitters, one serial command bus.**

---

## Phase 1: T3S3 Quick Wins (No Hardware Changes)

**Goal:** Improve signal authenticity of existing T3S3 firmware before XR1 hardware work  
**Duration:** 1 session (2-3 hours)  
**Dependencies:** None — all software changes on existing codebase  

### Sprint 1.1 — BLE ODID Spec Compliance

**What:** Two fixes to make BLE Remote ID output match real-world ODID transmitters.

**Fix A: Location message 3x weighting.** Real ODID transmitters send Location messages more frequently than identity messages because receivers need position updates more often than serial numbers. Change the current 50/50 alternation (Basic ID → Location → Basic ID → Location) to a 3:1 weighted rotation (Location → Location → Location → Basic ID → repeat).

**Fix B: AD counter byte.** The byte at position 7 in `_bleAdvData` is currently hardcoded `0x0D`. Per ASTM F3411, this byte is split: upper nibble = application code (0x0 for ODID), lower nibble = rotating message counter (0x0–0xF, incremented per advertisement of each message type, wraps at 0xF). A compliant receiver uses this counter for deduplication — without it, receivers may discard JJ's repeated advertisements as duplicates.

**Source:** cyber-defence-campus/droneRemoteIDSpoofer ARCHITECTURE.md, ASTM F3411-22a §5.4.6

### Sprint 1.2 — Crossfire Dual-Modulation + EU868 Band

**What:** Two improvements to Crossfire emulation fidelity.

**Fix A: Add Crossfire 868 EU band as a selectable option.** The band parameters are already defined in `protocol_params.h` (`CRSF_BANDS[CRSF_BAND_868]`) but hardcoded to 915 in `crossfire.cpp`. Add a serial sub-command so the operator can select 868 vs 915.

**Fix B: Add Crossfire 50 Hz LoRa mode.** Real Crossfire uses LoRa modulation (not FSK) in 50 Hz long-range mode. Currently JJ only emits Crossfire as FSK at 150 Hz. Add a mode that switches to LoRa for the 50 Hz variant, matching v2 §3.2.2. This is important because SENTRY-RF's CAD detection should trigger on Crossfire LoRa but not on Crossfire FSK — testing both validates the detector's dual-modulation handling.

### Sprint 1.3 — Remaining WARNING-Level Audit Fixes

**What:** Cleanup from the devil's advocate audit that improves reliability but doesn't change RF output.

- Check `startTransmit()` return values in all ELRS-family modes — if the radio rejects a TX, don't increment the packet counter
- Add proper task join in `combinedStop()` — wait for the Core 1 task to actually exit before reconfiguring the radio
- Gate `menuSetState(STATE_*_ACTIVE)` behind successful `*Start()` return — don't show active state if the radio failed to configure
- Fix `swarmCycleCount()` off-by-one (static idx starts at 1, should start at 0)

---

## Phase 2: XR1 Hardware Bring-Up

**Goal:** Physical wiring, pin verification, first SPI communication with LR1121  
**Duration:** 1 session (2-3 hours)  
**Dependencies:** Soldering iron, 4x jumper wires, USB-to-Serial adapter  

### Sprint 2.1 — Wiring and Factory Backup

1. Solder 4 wires to XR1 castled pads (5V, GND, TX, RX)
2. Power on XR1, hold bind button 5-7 seconds → WiFi AP starts
3. Connect to AP at http://10.0.0.1, download hardware.json → save to `docs/xr1_hardware.json`
4. Compare pin mapping to `xr1-firmware/include/xr1_pins.h` — update any differences
5. Download ELRS firmware backup → save to `docs/xr1_elrs_backup.bin`
6. If ELRS v3.6+ has written custom firmware to the LR1121 chip, flash stock Semtech transceiver firmware (lr1121_transceiver_0103.bin) via the ELRS web UI lr1121.html page

### Sprint 2.2 — Hello Hardware Flash and Verify

1. Flash `xr1-firmware/` via UART bootloader (hold GPIO9/BOOT while powering on)
2. Verify serial output shows LR1121 version and "XR1 READY"
3. Confirm LoRa TX on 915 MHz succeeds
4. Confirm band switch to 2440 MHz and LoRa TX succeeds
5. If SPI fails: debug BUSY pin, TCXO voltage, RF switch table against actual hardware.json

---

## Phase 3: XR1 UART Command Protocol

**Goal:** T3S3 sends serial commands → XR1 executes RF operations  
**Duration:** 2-3 sessions  
**Dependencies:** Phase 2 complete (XR1 responding to SPI)  

### Sprint 3.1 — XR1 Command Parser

Implement ASCII command protocol on XR1 at 115200 baud:

| Command | Function |
|---------|----------|
| `FREQ <MHz>` | Set frequency (auto band-switch) |
| `MOD LORA <SF> <BW> <CR>` | Set LoRa modulation |
| `MOD FSK <BR> <DEV>` | Set GFSK modulation |
| `PWR <dBm>` | Set output power |
| `TX <hex>` | Transmit single packet |
| `TXRPT <ms> <count>` | Repeat transmit |
| `STOP` | Stop all transmission |
| `HOP <ch1,ch2,...> <dwell_ms>` | Start FHSS hopping |
| `BAND?` | Query current band |
| `STATUS?` | Query system status |
| `RESET` | Hardware reset LR1121 |

### Sprint 3.2 — T3S3 XR1 Driver

Create `xr1_driver.h/cpp` on the T3S3 side. Functions: `xr1_init()`, `xr1_setFreq()`, `xr1_setLoRa()`, `xr1_setFSK()`, `xr1_setPower()`, `xr1_transmit()`, `xr1_startHop()`, `xr1_stop()`. Each sends the UART command and waits for OK/ERR with 500 ms timeout.

### Sprint 3.3 — JJ Menu Integration

Add `x` command to JJ serial menu → XR1 submenu with numbered protocol selections. All XR1 modes start/stop through the driver. T3S3 SX1262 modes continue to work independently.

---

## Phase 4: 2.4 GHz Protocol Profiles (XR1 LR1121)

**Goal:** Protocol-accurate FHSS on 2.4 GHz for every supported drone protocol  
**Duration:** 3-4 sessions  
**Dependencies:** Phase 3 complete (UART command link working)  

### Sprint 4.1 — ELRS 2.4 GHz Full Profile

**Channel plan:** 80 channels, 2400.4–2479.4 MHz, 1 MHz spacing  
**Packet rates:** 25/50/100/150/250/500 Hz (LoRa), F500/F1000 (FSK), K1000 (FSK+FEC)  
**LoRa config per rate:**
- 500 Hz: SF6/BW500
- 250 Hz: SF7/BW500
- 150 Hz: SF8/BW500
- 50 Hz: SF9/BW500
- 25 Hz: SF10/BW500

**FSK config (K1000):** GFSK 250 kbps  
**Hopping:** Pseudo-random sequence, sync channel 40, hop timing matches sub-GHz patterns  
**Header:** Implicit (matching the fix from the audit)

### Sprint 4.2 — ImmersionRC Ghost 2.4 GHz

**Channel plan:** ~80 channels, 2.4 GHz ISM  
**Modulation:** LoRa  
**Packet rates:** 250 Hz, 500 Hz  
**Hopping:** Proprietary pseudo-random (approximate with LCG-based shuffle)

### Sprint 4.3 — FrSky ACCST D16 RF Footprint (2.4 GHz)

**Channel plan:** ~47 channels, 2400–2483 MHz, ~1 MHz spacing  
**Modulation:** GFSK, ~250 kbps, ~50 kHz deviation  
**Hop interval:** 9 ms (8ch mode), 18 ms (16ch mode)  
**Source:** DIY-Multiprotocol-TX-Module FrSky D16 implementation  
**Note:** RF footprint emulation — correct modulation and hop pattern, not protocol-correct packets

### Sprint 4.4 — FlySky AFHDS 2A RF Footprint (2.4 GHz)

**Channel plan:** 16 channels, 2.4 GHz  
**Modulation:** GFSK  
**Hop interval:** ~1.5 ms  
**Source:** DIY-Multiprotocol-TX-Module FlySky implementation

### Sprint 4.5 — Generic 2.4 GHz FHSS

Configurable channel count (16-80), hop interval (1-50 ms), modulation (LoRa or GFSK), for testing against unknown 2.4 GHz drone protocols.

### Sprint 4.6 — DJI OcuSync Energy Approximation (2.4 GHz)

**What it IS:** GFSK bursts hopping across 2.4 GHz channels to approximate the energy pattern of OcuSync's FHSS control uplink.  
**What it is NOT:** OFDM video downlink (impossible without SDR).  
**Purpose:** Generates "something hopping at 2.4 GHz" that triggers energy-based detectors even though it's not protocol-correct OcuSync.

---

## Phase 5: Remote ID — WiFi, BLE, and DJI DroneID (XR1 ESP32C3)

**Goal:** Spec-compliant ODID emission on WiFi + BLE + DJI proprietary format  
**Duration:** 3-4 sessions  
**Dependencies:** Phase 3 complete (UART link for T3S3 → XR1 commands)  

### Sprint 5.1 — ASTM F3411 WiFi Beacon (XR1)

Use ESP32C3's built-in WiFi to broadcast ODID messages as vendor-specific IE in 802.11 beacon frames.

**Details:**
- OUI: FA:0B:BC (ODID standard)
- Message types: Basic ID, Location, System, Operator ID
- Packed message format (Message Pack type 0xF) for efficiency
- Channel rotation: 1, 6, 11 every 3 seconds
- Beacon interval: 100 ms
- Use opendroneid-core-c library (already vendored in lib/opendroneid/)

**Parameters via UART:** `RID WIFI <serial> <lat> <lon> <alt> <speed> <heading>`

### Sprint 5.2 — ASTM F3411 WiFi NaN (XR1)

**What:** WiFi Neighbor Awareness Networking is an additional transport method added in ASTM F3411-22a. Some newer drones broadcast ODID via NaN instead of or alongside beacon frames.

**Why it matters:** SENTRY-RF's WiFi scanner needs to be tested against both transport types. A scanner that only looks for beacon frames will miss NaN-based drones.

**Implementation:** ESP32C3 supports WiFi Aware (NaN) via esp_wifi_nan APIs. Publish ODID data as a NaN service with the standardized ODID service name.

### Sprint 5.3 — ASTM F3411 BLE Advertisement (XR1)

**BLE 4 Legacy:** ADV_NONCONN_IND with Service Data AD (type 0x16), UUID 0xFFFA, 25-byte ODID message. Location 3x weighting. Rotating AD counter in lower nibble of app code byte.

**BLE 5 Long Range (stretch):** Extended advertising with coded PHY (S8 or S2). 255-byte payload allows packed messages containing all ODID message types in a single advertisement. Better range than BLE 4 Legacy.

**Message rotation:** Location → Location → Location → Basic ID → System → Operator ID cycle (Location at 3x frequency per ASTM recommendation and droneRemoteIDSpoofer reference implementation).

### Sprint 5.4 — DJI DroneID WiFi Beacon (XR1)

**This is the high-value target.** DJI dominates 70%+ of consumer drones. Every DJI drone broadcasts a proprietary DroneID via WiFi vendor-specific IE that is distinct from ASTM ODID.

**Byte layout from RUB-SysSec/DroneSecurity (NDSS 2023) + Kismet Kaitai definition:**

| Field | Offset | Type | Encoding |
|-------|--------|------|----------|
| OUI | IE header | 3 bytes | 26:37:12 (DJI, NOT FA:0B:BC) |
| Subcommand | 3 | uint8 | 0x10 = telemetry, 0x11 = flight purpose |
| Sequence | 5 | int16le | Incrementing counter |
| State info | 7 | uint16le | Bitfield (see below) |
| Serial number | 9 | ASCII[16] | Null-padded |
| Longitude | 25 | int32le | degrees × 174533 (NOT × 1e7) |
| Latitude | 29 | int32le | degrees × 174533 |
| Altitude | 33 | int16le | meters MSL |
| Height AGL | 35 | int16le | meters |
| V_north/east/up | 37-42 | int16le × 3 | m/s |
| Pitch/roll/yaw | 43-48 | int16le × 3 | raw/100/57.296 = radians |
| Home lon/lat | 49-56 | int32le × 2 | degrees × 174533 |
| Product type | 57 | uint8 | 0x03=Mavic2, 0x0A=Mini2, 0x11=Air2S |
| UUID | 59 | binary[20] | Device UUID |

**State info bitfield:** serial_valid (0x01), private_disabled (0x02), homepoint_set (0x04), uuid_set (0x08), motors_on (0x10), in_air (0x20), gps_valid (0x40), alt_valid (0x80), height_valid (0x100), h_vel_valid (0x200), v_up_valid (0x400), attitude_valid (0x800)

**GPS encoding critical difference:** DJI uses `int32(degrees × 174533)`. ODID uses `int32(degrees × 1e7)`. Using the wrong formula produces positions that are off by a factor of ~57 (the degrees-to-radians conversion). The `dji_droneid.cpp` stub in xr1-firmware MUST use 174533.

**Realistic state_info for flying drone:** 0x0FFF (all valid flags set)  
**Broadcast:** WiFi channels 1, 6, 11 at ~200 ms intervals  
**Validation:** SENTRY-RF's existing `remote_id_parser.h` parses this format — end-to-end test loop.

### Sprint 5.5 — Swarm Mode (Multi-Drone, All Transports)

**WiFi:** Up to 16 virtual ODID beacons, each with unique serial/position  
**WiFi DJI:** Up to 4 virtual DJI DroneID beacons with different model codes  
**BLE:** Rotate through up to 4 virtual drone BLE advertisements  
**LR1121:** Rapidly alternate between different FHSS patterns (simulating multiple control links)

Each virtual drone gets: unique serial number (FAA format), GPS position offset from base (configurable radius), slightly different FHSS timing offset, independent heading/speed.

---

## Phase 6: Combined Scenarios (All Emitters Simultaneous)

**Goal:** Realistic multi-emitter scenarios exercising SENTRY-RF's full detection pipeline  
**Duration:** 2 sessions  
**Dependencies:** Phases 4 and 5 complete  

### Sprint 6.1 — Scenario Profiles

| Command | Scenario | SX1262 (sub-GHz) | LR1121 (2.4 GHz) | WiFi | BLE |
|---------|----------|-------------------|-------------------|------|-----|
| `c 1` | Racing drone | ELRS 900 FHSS | ELRS 2.4 FHSS | ODID beacon | ODID advert |
| `c 2` | DJI consumer | Idle | OcuSync energy approx | DJI DroneID | ODID advert |
| `c 3` | Long range FPV | Crossfire 915 | Idle | ODID beacon | ODID advert |
| `c 4` | Dual-band ELRS | ELRS 900 FHSS | ELRS 2.4 FHSS | Idle | Idle |
| `c 5` | Everything | ELRS 900 FHSS | ELRS 2.4 FHSS | DJI DroneID + ODID | ODID advert |
| `c s N` | Swarm (N drones) | Mixed FHSS | Mixed 2.4 FHSS | N × ODID + DJI | N × BLE |

### Sprint 6.2 — Protocol Info Banner

When any mode starts, print a complete parameter summary matching v2 §7.2 format:

```
[COMBINED: Racing Drone]
  SX1262: ELRS-FCC915 40ch SF6/BW500 200Hz implicit 0x12 | 10 dBm
  LR1121: ELRS-ISM2G4 80ch SF6/BW500 500Hz implicit 0x12 | 13 dBm
  WiFi:   ODID beacon ch1/6/11 100ms | Serial: JJ-TEST-001
  BLE:    ODID Legacy ADV 3:1 Location weighting | UUID 0xFFFA
  Position: 36.8529°N 75.9780°W Alt: 50m Speed: 5 m/s Hdg: 270°
```

---

## Phase 7: Polish, Validation, and Documentation

**Goal:** Complete menu system, field validation against SENTRY-RF, documentation  
**Duration:** 2-3 sessions  
**Dependencies:** Phases 1-6 complete  

### Sprint 7.1 — Complete Serial Menu

```
JJ v3.0 Signal Generator
├── [e] Sub-GHz Drone Protocols (T3S3 SX1262)
│   ├── ELRS FHSS (7 domains × 6 rates + binding)
│   ├── Crossfire (868/915, FSK 150Hz / LoRa 50Hz / Dual TDM)
│   ├── SiK MAVLink (433/868/915, 3 air speeds)
│   ├── mLRS (868/915/433)
│   └── Custom LoRa/FSK Direct
├── [x] 2.4 GHz Drone Protocols (XR1 LR1121)
│   ├── ELRS 2.4 GHz (80ch, all packet rates)
│   ├── ImmersionRC Ghost (LoRa 2.4 GHz)
│   ├── FrSky ACCST D16 footprint (GFSK FHSS)
│   ├── FlySky AFHDS 2A footprint (GFSK FHSS)
│   ├── DJI OcuSync energy approximation
│   ├── Generic 2.4 GHz FHSS (configurable)
│   └── Custom 2.4 GHz LoRa/FSK
├── [r] Remote ID (T3S3 WiFi/BLE — existing)
│   ├── ASTM F3411 WiFi Beacon (ODID)
│   ├── ASTM F3411 BLE Advertisement (ODID, 3:1 Location weighting)
│   ├── Swarm (1-16 virtual drones)
│   └── Stop All Remote ID
├── [y] Remote ID (XR1 WiFi/BLE — enhanced)
│   ├── ASTM F3411 WiFi Beacon (ODID, opendroneid-core-c encoding)
│   ├── ASTM F3411 WiFi NaN (ODID)
│   ├── ASTM F3411 BLE 4 Legacy + BLE 5 Long Range
│   ├── DJI DroneID WiFi Beacon (OUI 26:37:12, proprietary format)
│   ├── Swarm (1-16 ODID + 1-4 DJI virtual drones)
│   └── Stop All Remote ID
├── [f] Infrastructure / False Positive (T3S3 SX1262)
│   ├── LoRaWAN US915 SB2 / EU868
│   ├── Meshtastic Beacon
│   ├── Helium PoC
│   └── Dense Mixed (ELRS + LoRaWAN + Meshtastic)
├── [c] Combined Scenarios (All Emitters)
│   ├── Racing Drone (900+2400+RID)
│   ├── DJI Consumer (2400+DJI_RID)
│   ├── Long Range FPV (900+RID)
│   ├── Dual-Band ELRS (900+2400)
│   ├── Everything (all emitters active)
│   └── Swarm (N drones, all transports)
├── [w] CW / Band Sweep / Power Ramp
├── [p] Cycle TX power
├── [d] Toggle OLED
├── [h] Help
└── [q] Stop current mode
```

### Sprint 7.2 — Third-Party Validation

1. **bkerler/DroneID receiver** — Run on laptop, confirm JJ's WiFi+BLE ODID output decodes correctly
2. **OpenDroneID Android app** — Confirm JJ's BLE advertisements appear as valid drones with correct direction, altitude, speed
3. **SDR capture** — Record JJ's ELRS output with HackRF/RTL-SDR, compare spectrogram against real ELRS TX
4. **SENTRY-RF integration test** — Run JJ through every mode while SENTRY-RF monitors, verify detection/classification for each protocol

### Sprint 7.3 — Protocol Coverage Scorecard

| Protocol | Band | Emitter | Detected? | Method | Range | Notes |
|----------|------|---------|-----------|--------|-------|-------|
| ELRS 900 (7 domains × 6 rates) | 868/915 MHz | SX1262 | ☐ | RSSI+CAD | ☐ | |
| ELRS 2.4 (6 rates) | 2.4 GHz | LR1121 | ☐ | RSSI | ☐ | |
| Crossfire FSK 150Hz | 868/915 MHz | SX1262 | ☐ | FSK detect | ☐ | |
| Crossfire LoRa 50Hz | 868/915 MHz | SX1262 | ☐ | CAD | ☐ | |
| SiK MAVLink (3 speeds) | 915 MHz | SX1262 | ☐ | FSK detect | ☐ | |
| mLRS | 915 MHz | SX1262 | ☐ | RSSI+CAD | ☐ | |
| Ghost 2.4 | 2.4 GHz | LR1121 | ☐ | RSSI | ☐ | |
| FrSky D16 footprint | 2.4 GHz | LR1121 | ☐ | RSSI | ☐ | |
| FlySky footprint | 2.4 GHz | LR1121 | ☐ | RSSI | ☐ | |
| DJI OcuSync energy | 2.4 GHz | LR1121 | ☐ | RSSI | ☐ | |
| ODID WiFi beacon | 2.4 GHz | ESP32C3 WiFi | ☐ | WiFi promiscuous | ☐ | |
| ODID WiFi NaN | 2.4 GHz | ESP32C3 WiFi | ☐ | WiFi NaN subscribe | ☐ | |
| ODID BLE 4 Legacy | 2.4 GHz | ESP32C3 BLE | ☐ | BLE scan | ☐ | |
| ODID BLE 5 LR | 2.4 GHz | ESP32C3 BLE | ☐ | BLE 5 coded | ☐ | |
| DJI DroneID WiFi | 2.4 GHz | ESP32C3 WiFi | ☐ | WiFi promiscuous | ☐ | |
| LoRaWAN US915/EU868 | 868/915 MHz | SX1262 | ☐ | FP rejection | ☐ | |
| Meshtastic | 906 MHz | SX1262 | ☐ | FP rejection | ☐ | |
| Dual-band paired | 900+2400 | SX1262+LR1121 | ☐ | Correlated | ☐ | |

---

## Emulation Coverage Summary (All Phases Complete)

| Category | Protocols | Count |
|----------|-----------|-------|
| Sub-GHz LoRa drone links | ELRS 900 (7 domains × 6 rates), Crossfire LoRa 50Hz (868/915), FrSky R9, mLRS | 46+ variants |
| Sub-GHz FSK drone links | Crossfire FSK 150Hz (868/915), SiK MAVLink (3 bands × 3 speeds), Crossfire Dual TDM | 12+ variants |
| 2.4 GHz LoRa drone links | ELRS 2.4 (6 LoRa rates), Ghost 2.4 | 8 variants |
| 2.4 GHz FSK drone links | ELRS K/FK modes, FrSky D16 footprint, FlySky footprint, DJI OcuSync energy | 6+ variants |
| WiFi Remote ID | ASTM F3411 beacon, ASTM F3411 NaN, DJI DroneID proprietary | 3 formats |
| BLE Remote ID | ASTM F3411 BLE4 Legacy (3:1 weighted), BLE5 Long Range (stretch) | 2 formats |
| Infrastructure (false positive) | LoRaWAN US915/EU868, Meshtastic, Helium, Dense Mixed | 5 scenarios |
| Combined scenarios | Racing, DJI consumer, long range, dual-band, everything, swarm (1-16) | 6 scenarios |
| **Total distinct emulation profiles** | | **~90+** |

## What Remains Out of Reach

| Limitation | Reason | Alternative |
|------------|--------|-------------|
| DJI OcuSync OFDM video | Requires multi-carrier SDR hardware | AntSDR E200 future project (TESTKIT-GPS) |
| 5.8 GHz analog FPV | LR1121 max frequency is 2.5 GHz | Would require dedicated 5.8 GHz TX hardware |
| Spektrum DSSS | LR1121 has no DSSS/spread-spectrum capability | No known workaround |
| Actual protocol handshake/binding | JJ emulates RF footprint, not application layer | Acceptable — detectors classify by RF, not by binding |

---

## References

### Hardware
1. Semtech LR1121 Datasheet Rev 2.0 — https://files.waveshare.com/wiki/Core1121/LR1121_H2_DS_v2_0.pdf
2. Semtech LR1121 User Manual v1.1 — https://www.mouser.com/pdfDocs/UserManual_LR1121_v1_1.pdf
3. RadioMaster XR1 Product Page — https://radiomasterrc.com/products/xr1-nano-multi-frequency-expresslrs-receiver

### Software Libraries
4. RadioLib — https://github.com/jgromes/RadioLib
5. ExpressLRS — https://github.com/ExpressLRS/ExpressLRS
6. ExpressLRS Targets — https://github.com/ExpressLRS/targets
7. opendroneid-core-c — https://github.com/opendroneid/opendroneid-core-c
8. DIY-Multiprotocol-TX-Module — https://github.com/pascallanger/DIY-Multiprotocol-TX-Module
9. Semtech LR1121 Firmware Images — https://github.com/Lora-net/radio_firmware_images

### Protocol Research
10. DeepWiki ELRS LR1121 Targets — https://deepwiki.com/ExpressLRS/targets/4.2.3-dual-band-receivers-(lr1121)
11. DeepSig Commercial Drone Signals — https://www.deepsig.ai/introduction-to-commercial-drone-signals/
12. Oscar Liang ELRS vs Crossfire — https://oscarliang.com/expresslrs/
13. Oscar Liang FPV Protocols — https://oscarliang.com/rc-protocols/
14. TBS Crossfire Manual — https://ia600700.us.archive.org/22/items/tbs-crossfire-manual/tbs-crossfire-manual.pdf
15. IEEE/ETR DJI Signal Analysis — https://journals.ru.lv/index.php/ETR/article/download/8486/6933/10816

### DJI DroneID
16. RUB-SysSec/DroneSecurity (NDSS 2023) — https://github.com/RUB-SysSec/DroneSecurity
17. Kismet DJI DroneID Kaitai struct — https://github.com/kismetwireless/kismet/blob/master/kaitai_definitions_disabled/dot11_ie_221_dji_droneid.ksy
18. NDSS 2023 Paper — https://www.ndss-symposium.org/wp-content/uploads/2023-217-paper.pdf

### Remote ID
19. ASTM F3411-22a Specification
20. cyber-defence-campus/droneRemoteIDSpoofer — https://github.com/cyber-defence-campus/droneRemoteIDSpoofer
21. bkerler/DroneID — https://github.com/bkerler/DroneID
22. alphafox02/droneid-go — https://github.com/alphafox02/droneid-go
23. sxjack/remote_id_bt5 — https://github.com/sxjack/remote_id_bt5
24. nyanBOX ESP32 ODID broadcaster — https://github.com/jbohack/nyanBOX

### RF Datasets
25. DroneRF Dataset — https://pmc.ncbi.nlm.nih.gov/articles/PMC6727013/
26. DroneRFb-Spectra — https://ieee-dataport.org/documents/dronerfb-spectra-rf-spectrogram-dataset-drone-recognition
27. DroneDetect Dataset — https://ieee-dataport.org/open-access/dronedetect-dataset-radio-frequency-dataset-unmanned-aerial-system-uas-signals-machine

### Project Documents
28. JJ_Protocol_Emulation_Reference_v2.md — Sub-GHz protocol parameters with source citations
29. XR1_Integration_Research.md — XR1 hardware specs, pin mapping, feasibility analysis
30. SENTRY-RF_Phased_Improvement_Plan.md — Detection system development plan (companion project)
