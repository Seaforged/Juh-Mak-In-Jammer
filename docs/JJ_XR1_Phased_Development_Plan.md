# JJ + XR1: Maximum Drone Signal Emulation — Phased Development Plan
## From Single-Band LoRa Emulator to Full-Spectrum Drone Signal Generator

**Document Version:** 1.0  
**Date:** April 11, 2026  
**Authors:** ND (Seaforged), Claude (Technical Lead)  
**Project:** Juh-Mak-In Jammer (JJ) v3.0  
**Hardware:** T3S3 SX1262 + RadioMaster XR1 (ESP32C3 + LR1121)  
**Companion Doc:** XR1_Integration_Research.md  

---

## How to Use This Plan

This document defines **6 phases** of development, each building on the previous. Every phase contains numbered sprints with explicit CC prompts, acceptance criteria, and test procedures. Do not skip phases.

**Three emitters, one command bus:**

```
┌─────────────────────────────────────────────────────┐
│  T3S3 (ESP32-S3) — JJ Main Controller              │
│  ┌──────────┐  ┌────────────┐  ┌─────────────────┐ │
│  │ SX1262   │  │ UART1      │  │ Serial Console  │ │
│  │ Sub-GHz  │  │ TX→XR1 RX  │  │ User Commands   │ │
│  │ 150-960  │  │ RX←XR1 TX  │  │ Status Output   │ │
│  │ MHz      │  │ GPIO 43/44 │  │ USB             │ │
│  └──────────┘  └─────┬──────┘  └─────────────────┘ │
└───────────────────────┼─────────────────────────────┘
                        │ 4-wire (5V, GND, TX, RX)
┌───────────────────────┼─────────────────────────────┐
│  XR1 (ESP32C3) — Dual-Band RF + WiFi/BLE Emitter   │
│  ┌──────────┐  ┌─────┴──────┐  ┌─────────────────┐ │
│  │ LR1121   │  │ UART       │  │ ESP32C3 WiFi    │ │
│  │ Sub-GHz  │  │ Commands   │  │ 802.11 b/g/n    │ │
│  │ + 2.4GHz │  │ from T3S3  │  │ BLE 5.0         │ │
│  │ LoRa/FSK │  │            │  │ (internal)      │ │
│  └──────────┘  └────────────┘  └─────────────────┘ │
└─────────────────────────────────────────────────────┘
```

---

## Phase 0: Foundation (Prerequisites)

**Goal:** Physical wiring and verify communication  
**Duration:** 1 session (2–3 hours)  
**Dependencies:** Soldering iron, 4x jumper wires, USB-to-Serial adapter (CP2102/FTDI)

### Sprint 0.1 — Wiring and Power Test

Solder 4 wires to XR1 castled pads. Connect to T3S3 per wiring table in XR1_Integration_Research.md §3.3. Power on, confirm XR1 LED blinks (ELRS boot sequence). Confirm no magic smoke.

**Acceptance Criteria:**
- [ ] XR1 powers on from T3S3 5V rail
- [ ] XR1 LED shows normal ELRS disconnected pattern (slow blink)
- [ ] T3S3 continues to boot normally with XR1 attached

### Sprint 0.2 — Read XR1 Hardware JSON

Before flashing anything custom, extract the factory hardware configuration:
1. Power on XR1
2. Hold bind button 5–7 seconds → WiFi AP starts
3. Connect to AP, navigate to http://10.0.0.1
4. Download hardware.json
5. Save to `C:\Projects\juh-mak-in-jammer\docs\xr1_hardware.json`

This file contains the EXACT pin mapping RadioMaster used, resolving any ambiguity from the generic ELRS target.

**Acceptance Criteria:**
- [ ] hardware.json downloaded and saved to JJ docs
- [ ] Pin mapping compared to Generic C3 LR1121 target — differences documented

### Sprint 0.3 — ELRS Firmware Backup

Use ELRS web UI to download current firmware binary for recovery. Save to `docs/xr1_elrs_backup.bin`.

---

## Phase 1: Hello Hardware + Basic RF (XR1 Custom Firmware)

**Goal:** Flash custom RadioLib firmware, verify LR1121 control, transmit first signals  
**Duration:** 2–3 sessions  
**Dependencies:** Phase 0 complete, pin mapping confirmed

### Sprint 1.1 — Hello LR1121

**CC Prompt:**
> Create a PlatformIO project at `C:\Projects\juh-mak-in-jammer\xr1-firmware\` for the RadioMaster XR1 (ESP32C3 + LR1121). Use RadioLib to initialize the LR1121 over SPI and print the chip version to serial. Pin mapping from XR1_Integration_Research.md §3.2 (or updated values from hardware.json if different). Include RF switch table configuration. Print "XR1 READY" on success.
>
> platformio.ini: board=esp32-c3-devkitm-1, framework=arduino, lib_deps=jgromes/RadioLib@^7.0.0, build_flags=-DARDUINO_USB_MODE=1 -DARDUINO_USB_CDC_ON_BOOT=1
>
> CRITICAL: Do NOT use radio.begin() with default frequency — set TCXO voltage to 3.0V first (LR1121 V1.3 TCXO is 3.0V). Use SPI Mode 0 (CPOL=0, CPHA=0). Set SPI clock to 500 kHz initially for debugging stability.

**Flashing:** Connect USB-to-Serial adapter to XR1's TX/RX/GND pads. Hold boot button while powering on to enter bootloader. Flash via `pio run --target upload`.

**Acceptance Criteria:**
- [ ] Serial output shows LR1121 version (expected: 0x07, 0x22, 0x03, 0x01 or similar)
- [ ] No SPI timeout errors
- [ ] BUSY pin reads LOW after initialization
- [ ] "XR1 READY" printed

### Sprint 1.2 — Basic LoRa Transmit

**CC Prompt:**
> Add a loop that transmits a LoRa packet every 2 seconds on 915.0 MHz, SF7, BW125, CR 4/5, at 10 dBm. Print "TX OK" after each successful transmission. Alternate between sub-GHz and 2.4 GHz every 10 transmissions to verify band switching works. For 2.4 GHz use 2440.0 MHz, SF7, BW812.5.

**Test:** Use SENTRY-RF (T3S3 with SX1262) to verify 915 MHz transmissions are detected. Use LR1121 board (if available) or a second XR1 to verify 2.4 GHz.

**Acceptance Criteria:**
- [ ] LoRa packets transmitted on 915 MHz, confirmed by SENTRY-RF detection
- [ ] Band switch to 2.4 GHz succeeds without SPI errors
- [ ] LoRa packets transmitted on 2440 MHz
- [ ] Band switch back to 915 MHz succeeds

### Sprint 1.3 — Basic GFSK Transmit

**CC Prompt:**
> Add GFSK transmission mode. Configure LR1121 for GFSK at 2440 MHz, 250 kbps bitrate, 150 kHz frequency deviation, BW 400 kHz. Transmit a 16-byte test payload. Also test GFSK at 915 MHz with 4.8 kbps bitrate, 5 kHz deviation.

**Acceptance Criteria:**
- [ ] GFSK transmission at 2.4 GHz succeeds
- [ ] GFSK transmission at 915 MHz succeeds
- [ ] No errors switching between LoRa and GFSK modes

---

## Phase 2: UART Command Protocol (T3S3 ↔ XR1 Link)

**Goal:** T3S3 sends serial commands to XR1, XR1 executes RF operations  
**Duration:** 2–3 sessions  
**Dependencies:** Phase 1 complete

### Sprint 2.1 — XR1 Command Parser

**CC Prompt:**
> Implement a serial command protocol on the XR1 firmware. The T3S3 sends ASCII commands over UART at 115200 baud. Commands:
>
> `FREQ <MHz>` — Set frequency (e.g., `FREQ 915.0` or `FREQ 2440.0`)  
> `MOD LORA <SF> <BW> <CR>` — Set LoRa modulation (e.g., `MOD LORA 7 125.0 5`)  
> `MOD FSK <BR> <DEV>` — Set GFSK modulation (e.g., `MOD FSK 250.0 150.0`)  
> `PWR <dBm>` — Set output power (e.g., `PWR 10`)  
> `TX <hex_payload>` — Transmit single packet  
> `TXRPT <interval_ms> <count>` — Repeat transmit (0 count = infinite)  
> `STOP` — Stop repeating transmission  
> `HOP <ch1,ch2,...> <dwell_ms>` — Start FHSS hopping  
> `BAND?` — Query current band (responds "900" or "2400")  
> `STATUS?` — Query system status  
> `RESET` — Hardware reset LR1121  
>
> Respond with `OK` or `ERR <reason>` after each command.

**Acceptance Criteria:**
- [ ] All commands parse correctly from serial input
- [ ] FREQ command switches bands automatically based on frequency value
- [ ] TX command transmits and responds OK
- [ ] HOP command starts autonomous frequency hopping
- [ ] STOP command halts all transmission

### Sprint 2.2 — T3S3 XR1 Driver

**CC Prompt:**
> In the JJ main firmware (T3S3), create `xr1_driver.h` and `xr1_driver.cpp`. Implement functions to send commands to the XR1 over UART1 (GPIO 43/44 at 115200 baud):
>
> `xr1_init()` — Initialize UART, send STATUS?, verify "XR1 READY"  
> `xr1_setFreq(float mhz)` — Send FREQ command  
> `xr1_setLoRa(int sf, float bw, int cr)` — Send MOD LORA command  
> `xr1_setFSK(float bitrate, float deviation)` — Send MOD FSK command  
> `xr1_setPower(int dbm)` — Send PWR command  
> `xr1_transmit(uint8_t* data, int len)` — Send TX command with hex payload  
> `xr1_startHop(float* channels, int numCh, int dwellMs)` — Send HOP command  
> `xr1_stop()` — Send STOP command  
>
> Each function waits for OK/ERR response with 500 ms timeout.

### Sprint 2.3 — JJ Menu Integration

**CC Prompt:**
> Add XR1 entries to the JJ serial command menu. New top-level command `x` enters XR1 submenu:
>
> `x` → XR1 Signal Generator submenu:
>   `1` — ELRS 2.4 GHz (LoRa, 50 channels, configurable packet rate)
>   `2` — ELRS 900 MHz (LoRa, 40 channels, configurable packet rate)
>   `3` — Crossfire 915 MHz (LoRa 50Hz / FSK 150Hz selectable)
>   `4` — Crossfire 868 MHz
>   `5` — Generic 2.4 GHz FHSS (GFSK, configurable channels/timing)
>   `6` — Custom frequency/modulation (manual parameters)
>   `0` — Stop all XR1 transmissions
>   `q` — Return to main menu
>
> Each option configures the XR1 via the driver functions from Sprint 2.2.

**Acceptance Criteria:**
- [ ] Menu navigation works from JJ serial console
- [ ] Selecting option 1 starts ELRS 2.4 GHz hopping on XR1
- [ ] Selecting option 3 starts Crossfire-like hopping on XR1
- [ ] Option 0 stops XR1 transmission
- [ ] T3S3 SX1262 modes continue to work independently

---

## Phase 3: Protocol-Accurate Emulation Profiles

**Goal:** Implement accurate RF footprints for each major drone protocol  
**Duration:** 3–4 sessions  
**Dependencies:** Phase 2 complete

### Sprint 3.1 — ELRS Full Profile Engine

**CC Prompt:**
> Implement complete ELRS frequency plan emulation on the XR1. Reference: JJ_Protocol_Emulation_Reference_v2.md and XR1_Integration_Research.md.
>
> **ELRS 2.4 GHz ISM (ISM2G4):**
> - 80 channels, 2400.4–2479.4 MHz, 1 MHz spacing
> - Packet rates: 25 Hz, 50 Hz, 100 Hz, 150 Hz, 250 Hz, 500 Hz (LoRa), F500/F1000 (FSK), K1000 (FSK+FEC)
> - For LoRa modes: SF6/BW500 (500Hz), SF7/BW500 (250Hz), SF8/BW500 (150Hz), SF9/BW500 (50Hz), SF10/BW500 (25Hz)
> - For FSK modes (K1000): 250 kbps GFSK
> - Hopping: pseudo-random sequence, hop every packet (configurable)
> - Sync channel: channel 40 (midpoint), visited every N hops for binding beacon
>
> **ELRS 900 MHz FCC915:**
> - 40 channels, 903.5–926.9 MHz, variable spacing
> - Packet rates: 25 Hz, 50 Hz, 100 Hz, 200 Hz (LoRa), D50/D250 (diversity), K1000 Full (FSK)
> - For LoRa modes: SF6/BW500 (200Hz), SF7/BW500 (100Hz), SF8/BW500 (50Hz), SF10/BW500 (25Hz)
> - Hopping: pseudo-random, hop every 4 packets at 200Hz
>
> **ELRS 868 MHz EU868:**
> - 13 channels (restricted by EU duty cycle), 863–870 MHz range
> - Same SF/BW/packet rate mapping as FCC915
>
> Store all channel plans as const arrays. Each profile is selectable from the JJ menu.

**Acceptance Criteria:**
- [ ] ELRS ISM2G4 profile hops across 80 channels at correct timing
- [ ] ELRS FCC915 profile hops across 40 channels
- [ ] ELRS EU868 profile uses only 13 channels
- [ ] All packet rates produce correct hop interval
- [ ] SENTRY-RF detects all profiles as drone-like FHSS activity

### Sprint 3.2 — Crossfire Profile Engine

**CC Prompt:**
> Implement TBS Crossfire emulation profiles:
>
> **Crossfire 915 MHz (FCC):**
> - 100 channels, 902.165–927.905 MHz, 260 kHz spacing
> - 50 Hz mode: LoRa SF9 or SF10, BW125-500 (proprietary, approximate with SF9/BW250)
> - 150 Hz mode: FSK 85.1 kbps, 50 kHz deviation
> - Hopping: pseudo-random, ~150 channels/second at 150 Hz mode
>
> **Crossfire 868 MHz (EU):**
> - 100 channels, 860.165–885.905 MHz, 260 kHz spacing
> - Same modulation as 915 MHz variant
>
> Both modes should alternate between LoRa and FSK packets to emulate Crossfire's dynamic rate switching.

### Sprint 3.3 — FrSky R9 and Generic Sub-GHz Profiles

**CC Prompt:**
> Implement additional sub-GHz drone protocol profiles:
>
> **FrSky R9 ACCESS (900 MHz):**
> - LoRa modulation, 868/915 MHz ISM band
> - ~50 channels with FHSS
> - 25 Hz packet rate (long range), slower hop rate
>
> **mLRS (MAVLink Long Range System):**
> - 868/915/433 MHz selectable
> - LoRa SF5-SF12, BW500
> - 19/31/50 Hz packet rates
> - Hop pattern: 24 channels
>
> **SiK Radio (MAVLink Telemetry):**
> - 433/868/915 MHz
> - GFSK 64/125/250 kbps
> - 20 or 50 channel FHSS
> - Continuous bidirectional (emulate uplink only)
>
> **ImmersionRC Ghost (2.4 GHz):**
> - LoRa-based, 2.4 GHz ISM
> - Proprietary hopping, ~80 channels
> - Packet rates: 250 Hz, 500 Hz

### Sprint 3.4 — 2.4 GHz GFSK Profiles (FrSky/FlySky/Spektrum-like)

**CC Prompt:**
> Implement 2.4 GHz GFSK hopping profiles:
>
> **FrSky ACCST D16 footprint:**
> - GFSK modulation at 2.4 GHz
> - ~47 channels, 2400–2483 MHz, ~1 MHz spacing
> - Hop interval: 9 ms (8ch mode) or 18 ms (16ch mode)
> - Bitrate: ~250 kbps, deviation: ~50 kHz
> - NOTE: This emulates the RF FOOTPRINT, not the actual FrSky packet format
>
> **FlySky AFHDS 2A footprint:**
> - GFSK, 2.4 GHz, 16 channels
> - 1.5 ms hop interval
> - ~250 kbps
>
> **Generic fast-hop 2.4 GHz:**
> - Configurable channel count (16–80), hop interval (1–50 ms)
> - Useful for testing SENTRY-RF against unknown 2.4 GHz drone protocols

---

## Phase 4: WiFi Remote ID + BLE Broadcast

**Goal:** Emit ASTM F3411 compliant Remote ID via WiFi beacon AND BLE advertisement  
**Duration:** 2–3 sessions  
**Dependencies:** Phase 2 complete (Phase 3 can run in parallel)

### Sprint 4.1 — WiFi Remote ID Beacon (ASTM F3411 WiFi NaN)

**CC Prompt:**
> On the XR1 ESP32C3 firmware, implement WiFi Remote ID beacon transmission per ASTM F3411-22a.
>
> Use the ESP32C3's built-in WiFi radio (NOT the LR1121) to broadcast WiFi beacon frames containing Open Drone ID (ODID) messages as vendor-specific Information Elements.
>
> Reference implementation: github.com/opendroneid/opendroneid-core-c
>
> **Message types to implement:**
> 1. Basic ID (Message Type 0) — UA serial number, ID type (serial/CAA)
> 2. Location/Vector (Message Type 1) — lat, lon, altitude, speed, direction, timestamp
> 3. System (Message Type 4) — operator lat/lon, area count, area radius
> 4. Operator ID (Message Type 5) — operator registration ID
>
> **Packed message format:** Use Message Pack (Message Type 0xF) to combine multiple ODID messages into a single beacon frame (more efficient, required by some implementations).
>
> **Parameters configurable via UART command from T3S3:**
> - `RID WIFI <serial> <lat> <lon> <alt> <speed> <heading>` — Start WiFi Remote ID
> - `RID WIFI STOP` — Stop beacon
> - `RID WIFI SWARM <count>` — Emit multiple virtual drones (1–16), each with randomized but plausible serial numbers, positions spread within 500 m radius
>
> **WiFi beacon details:**
> - Channel: hop between 1, 6, 11 every 3 seconds
> - Beacon interval: 100 ms (per ASTM recommendation)
> - SSID: hidden or empty
> - Vendor-specific IE OUI: 0xFA, 0x0B, 0xBC (ODID OUI per F3411)
>
> **Critical reference:** The nyanBOX project (github.com/jbohack/nyanBOX) has a working ESP32 implementation of ODID WiFi beacon broadcasting that can serve as implementation reference.

**Acceptance Criteria:**
- [ ] WiFi beacon frames transmitted with ODID vendor-specific IE
- [ ] OpenDroneID Android app detects the virtual drone
- [ ] SENTRY-RF WiFi scanner (Sprint 8) detects and parses the Remote ID
- [ ] Swarm mode creates multiple distinguishable virtual drone IDs
- [ ] WiFi beacon and LR1121 RF operate simultaneously without conflict

### Sprint 4.2 — BLE Remote ID Advertisement (ASTM F3411 BLE)

**CC Prompt:**
> On the XR1 ESP32C3 firmware, implement BLE 4 Legacy Advertising for Open Drone ID per ASTM F3411.
>
> Use the ESP32C3's built-in BLE 5.0 radio to broadcast BLE advertisement frames containing ODID messages.
>
> **BLE advertisement format (ASTM F3411-19 §5.4.6):**
> - Advertisement type: ADV_NONCONN_IND (non-connectable, undirected)
> - AD Type: 0x16 (Service Data - 16-bit UUID)
> - Service UUID: 0xFFFA (ASTM Remote ID)
> - Payload: 25-byte ODID message (same encoding as WiFi)
> - AD Counter: increment after each frame of same message type, wrap at 0xFF
> - One message type per advertisement (Basic ID, Location, System rotate)
>
> **BLE 5 Long Range (optional stretch):**
> - Extended advertising with coded PHY (S8 or S2)
> - 255-byte payload allows packed messages
> - Requires ESP32C3 BLE 5 extended advertising API
>
> **Parameters configurable via UART command:**
> - `RID BLE <serial> <lat> <lon> <alt>` — Start BLE Remote ID
> - `RID BLE STOP` — Stop advertising
> - `RID BLE SWARM <count>` — Multiple virtual drones via rotating advertisement data
>
> **Message rotation:** Cycle through Basic ID → Location → System → Operator ID, one per advertisement interval (100–200 ms per rotation through all types).

**Acceptance Criteria:**
- [ ] BLE advertisements transmitted with ODID Service Data
- [ ] OpenDroneID Android app detects the virtual drone via BLE
- [ ] BLE and WiFi Remote ID can operate simultaneously
- [ ] BLE, WiFi, AND LR1121 RF can all operate simultaneously (triple emitter)

### Sprint 4.3 — DJI DroneID Emulation (WiFi Vendor-Specific)

**CC Prompt:**
> Implement DJI-style DroneID WiFi beacon emission. DJI uses a proprietary vendor-specific IE format (not standard ODID) in their WiFi beacon frames.
>
> **DJI DroneID beacon format:**
> - WiFi beacon frame with vendor-specific IE
> - OUI: 0x26, 0x37, 0x12 (DJI vendor OUI)
> - Payload contains: drone serial number (16 bytes), GPS lat/lon/alt, pilot GPS lat/lon, drone model, firmware version
> - Broadcast on channels 1, 6, 11 at ~200 ms intervals
>
> **Parameters:**
> - `RID DJI <serial> <lat> <lon> <alt> <pilot_lat> <pilot_lon>` — Start DJI DroneID
> - `RID DJI MODEL <model_code>` — Set drone model (Mavic 3, Mini 4 Pro, etc.)
> - `RID DJI STOP` — Stop
>
> **Reference:** SENTRY-RF's existing `remote_id_parser.h` has the DJI vendor IE parsing code — use the same field layout in reverse for emission.
>
> **NOTE:** This is for TESTING SENTRY-RF's DJI detection capability. The emitted beacon should be parseable by SENTRY-RF's existing WiFi scanner.

---

## Phase 5: Combined Modes + Dual-Band Scenarios

**Goal:** Simultaneous multi-emitter operation for realistic drone scenarios  
**Duration:** 2 sessions  
**Dependencies:** Phases 2–4 complete

### Sprint 5.1 — Simultaneous Dual-Band Emission

**CC Prompt:**
> Implement a combined emission mode where the T3S3 SX1262 and XR1 LR1121 transmit simultaneously:
>
> **Scenario: "Racing drone approaching"**
> - T3S3 SX1262: ELRS 900 MHz FHSS (40 channels, 200 Hz)
> - XR1 LR1121: ELRS 2.4 GHz FHSS (80 channels, 500 Hz)
> - XR1 WiFi: Remote ID beacon
> - XR1 BLE: Remote ID BLE advertisement
>
> New JJ command: `c 1` (combined mode 1 — racing drone scenario)
>
> **Scenario: "DJI consumer drone"**
> - T3S3 SX1262: Idle (DJI doesn't use sub-GHz)
> - XR1 LR1121: Generic 2.4 GHz GFSK FHSS (approximate OcuSync energy)
> - XR1 WiFi: DJI DroneID beacon
> - XR1 BLE: ODID BLE advertisement
>
> New JJ command: `c 2` (combined mode 2 — DJI drone scenario)
>
> **Scenario: "Long range fixed wing"**
> - T3S3 SX1262: Crossfire 915 MHz FHSS
> - XR1 LR1121: Idle or MAVLink 915 MHz telemetry (alternating with SX1262)
> - XR1 WiFi: Remote ID WiFi beacon
> - XR1 BLE: Remote ID BLE
>
> New JJ command: `c 3` (combined mode 3 — long range scenario)

### Sprint 5.2 — Drone Swarm Simulation

**CC Prompt:**
> Implement a swarm simulation mode that creates the RF appearance of multiple drones:
>
> - XR1 WiFi: Up to 16 virtual Remote ID beacons, each with unique serial/position
> - XR1 BLE: Rotate through up to 4 virtual drone BLE advertisements
> - XR1 LR1121: Rapidly alternate between different FHSS patterns (simulating multiple control links sharing the spectrum)
> - T3S3 SX1262: Additional sub-GHz patterns
>
> New JJ command: `c s <count>` (swarm mode with N drones)
>
> Each virtual drone gets: unique serial number (FAA format), GPS position offset from base position (configurable radius), slightly different FHSS timing (offset by random ms to avoid perfect synchronization).

---

## Phase 6: Polish, Documentation, and Validation

**Goal:** Complete menu system, documentation, and field validation  
**Duration:** 2–3 sessions  
**Dependencies:** Phases 1–5 complete

### Sprint 6.1 — Complete Protocol Selection Menu

Implement the full menu structure from JJ_Protocol_Emulation_Reference_v2.md §7.3, adding all XR1 2.4 GHz and WiFi/BLE options:

```
JJ v3.0 Signal Generator
├── [e] Sub-GHz Drone Protocols (SX1262)
│   ├── ELRS FHSS (FCC915/EU868/AU915/IN866/AU433/EU433/US433)
│   ├── Crossfire (868/915, LoRa 50Hz / FSK 150Hz)
│   ├── FrSky R9 ACCESS (868/915)
│   ├── mLRS (868/915/433)
│   ├── SiK MAVLink Telemetry (433/868/915)
│   └── Custom LoRa/FSK Direct
├── [x] 2.4 GHz Drone Protocols (XR1 LR1121)
│   ├── ELRS 2.4 GHz (80ch, all packet rates)
│   ├── ImmersionRC Ghost (LoRa 2.4 GHz)
│   ├── FrSky ACCST D16 footprint (GFSK FHSS)
│   ├── FlySky AFHDS 2A footprint (GFSK FHSS)
│   ├── Generic 2.4 GHz FHSS (configurable)
│   └── Custom 2.4 GHz LoRa/FSK
├── [r] Remote ID Emulation (XR1 WiFi + BLE)
│   ├── ASTM F3411 WiFi Beacon (ODID)
│   ├── ASTM F3411 BLE Advertisement (ODID)
│   ├── DJI DroneID WiFi Beacon (proprietary)
│   ├── Swarm (1–16 virtual drones, WiFi + BLE)
│   └── Stop All Remote ID
├── [b] Infrastructure / False Positive (SX1262)
│   ├── LoRaWAN US915/EU868
│   ├── Meshtastic Beacon
│   ├── Helium PoC
│   └── Dense Mixed (ELRS + LoRaWAN + Meshtastic)
├── [c] Combined Scenarios (All Emitters)
│   ├── Racing Drone (900+2400+RID)
│   ├── DJI Consumer Drone (2400+DJI_RID)
│   ├── Long Range Fixed Wing (900+RID)
│   ├── Drone Swarm (N drones)
│   └── Custom Combination
├── [w] CW / Power / Band Sweep (SX1262 + XR1)
└── [q] Quit
```

### Sprint 6.2 — Startup Info and Protocol Summary

When any mode starts, print a complete parameter summary:

```
[ELRS-ISM2G4] 80ch 2400.4-2479.4MHz SF6/BW500 500Hz
  Emitter: XR1 LR1121 | Power: 13 dBm (~20 mW)
  Hop: every packet | ToA: 2.1ms | Dwell: 2ms/freq
  Est. SENTRY-RF detect range: ~300m (2.4 GHz, -90 dBm threshold)

[RID-WiFi] ODID Beacon | Serial: JJ-TEST-001
  Position: 36.8529°N 75.9780°W Alt: 50m
  Channels: 1,6,11 rotating | Interval: 100ms

[RID-BLE] ODID BLE4 Legacy | Same serial
  Rotation: BasicID→Location→System→OperatorID (400ms cycle)
```

### Sprint 6.3 — Protocol Coverage Scorecard

Create a validation matrix documenting every protocol tested against SENTRY-RF:

| Protocol | Band | Emitter | SENTRY-RF Detected? | Detection Method | Range Tested | Notes |
|----------|------|---------|--------------------|-----------------|--------------| ------|
| ELRS 900 | 915 MHz | SX1262 | ☐ | RSSI+CAD | ☐ | |
| ELRS 2.4 | 2.4 GHz | LR1121 | ☐ | RSSI (LR1121 board) | ☐ | |
| Crossfire | 915 MHz | SX1262 | ☐ | RSSI+CAD | ☐ | |
| FrSky R9 | 915 MHz | SX1262 | ☐ | RSSI+CAD | ☐ | |
| WiFi RID | 2.4 GHz | ESP32C3 WiFi | ☐ | WiFi promiscuous | ☐ | |
| BLE RID | 2.4 GHz | ESP32C3 BLE | ☐ | BLE scan (future) | ☐ | |
| DJI DroneID | 2.4 GHz | ESP32C3 WiFi | ☐ | WiFi promiscuous | ☐ | |
| Ghost | 2.4 GHz | LR1121 | ☐ | RSSI (LR1121 board) | ☐ | |

---

## Final Emulation Coverage Summary

When all 6 phases are complete, JJ v3.0 can emulate:

| Category | Protocols | Count |
|----------|-----------|-------|
| Sub-GHz LoRa drone links | ELRS 900 (7 domains), Crossfire (868/915), FrSky R9, mLRS, Ghost 900 | 12 variants |
| Sub-GHz FSK drone links | Crossfire 150Hz, SiK MAVLink, mLRS K1000 | 3+ variants |
| 2.4 GHz LoRa drone links | ELRS 2.4 (6 packet rates), Ghost 2.4 | 7 variants |
| 2.4 GHz FSK drone links | ELRS K/FK modes, FrSky D16 footprint, FlySky AFHDS2A footprint | 5+ variants |
| WiFi Remote ID | ASTM F3411 ODID beacon, DJI DroneID proprietary | 2 formats |
| BLE Remote ID | ASTM F3411 BLE4 Legacy, BLE5 Long Range (stretch) | 2 formats |
| Infrastructure (false positive) | LoRaWAN US915/EU868, Meshtastic, Helium, mixed | 5+ scenarios |
| Combined scenarios | Racing drone, DJI consumer, long range, swarm (1–16) | 4 scenarios |

**Total distinct emulation profiles: ~40+**

**What remains out of reach:**
- DJI OcuSync OFDM video downlink (requires SDR, AntSDR E200 future project)
- 5.8 GHz analog FPV (LR1121 max 2.5 GHz)
- Spektrum DSSS (requires DSSS-capable hardware)
- Actual protocol handshake/binding (we emulate RF footprint, not application layer)

---

## References

All references from XR1_Integration_Research.md apply, plus:

19. **opendroneid-core-c** — Official ASTM F3411 C library for encoding/decoding ODID messages. GitHub: https://github.com/opendroneid/opendroneid-core-c
20. **nyanBOX** — ESP32 toolkit with working ODID WiFi+BLE Remote ID broadcaster. GitHub: https://github.com/jbohack/nyanBOX
21. **droneid-go** — High-performance ODID receiver supporting WiFi, BLE, and DJI DroneID. GitHub: https://github.com/alphafox02/droneid-go
22. **ASTM F3411-22a** — Standard Specification for Remote ID and Tracking (defines BLE and WiFi broadcast formats)
23. **remote_id_bt5** — nRF52840 BLE 4+5 Remote ID transmitter reference. GitHub: https://github.com/sxjack/remote_id_bt5
24. **JJ_Protocol_Emulation_Reference_v2.md** — Existing JJ protocol reference (sub-GHz protocols, channel plans, menu structure)
