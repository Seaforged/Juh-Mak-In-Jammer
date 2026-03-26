# JUH-MAK-IN JAMMER
## Complete Test Suite for SENTRY-RF — Design, Build & Operating Plan
### Version 2.0 — Incorporating Professional RF/EW Engineering Review

---

## 1. PROJECT OVERVIEW

**JUH-MAK-IN JAMMER** is an open-source test suite purpose-built to validate every detection capability of the SENTRY-RF passive drone detector and GNSS integrity monitor. It consists of two tools:

| Tool | Platform | What It Tests |
|------|----------|---------------|
| **JAMMER-RF** | LilyGo T3S3 (ESP32-S3 + SX1262) | Remote ID spoofing, sub-GHz drone signal simulation, ISM band false-positive generation |
| **JAMMER-GPS** | Python GUI + AntSDR E200 (AD9361) | GPS spoofing, GPS jamming, combined attack scenarios, GNSS vulnerability characterization |

**Design Philosophy:**
- Every test mode produces *measurable* results — detection probability, time-to-detect, false alarm rate
- Attack scenarios include both easy-to-detect and hard-to-detect cases — passing easy tests doesn't validate the system
- Signal levels are calibrated to realistic power levels, not overwhelming brute force
- False positive testing is treated as equally important as true positive testing
- Test parameters can be randomized to prevent unconscious bias from the developer who also builds the detector

---

## 2. HARDWARE REQUIREMENTS

### 2.1 JAMMER-RF Hardware

| Component | Specification | Purpose | Status |
|-----------|--------------|---------|--------|
| LilyGo T3S3 | ESP32-S3 + SX1262 868/915 MHz | Test tool main board | ✅ Already owned |
| Onboard SSD1306 OLED | 128×64, I2C, addr 0x3C | Menu system and status display | ✅ Built-in |
| Onboard SX1262 | 150–960 MHz LoRa/FSK TX/RX | Sub-GHz signal generation | ✅ Built-in |
| ESP32-S3 WiFi | 2.4 GHz 802.11 b/g/n | Remote ID beacon frame TX | ✅ Built-in |
| ESP32-S3 BLE | Bluetooth 5.0 | Remote ID BLE advertising TX | ✅ Built-in |

**Pin Configuration (from SENTRY-RF board_config.h):**
- SX1262 SPI: SCK=5, MISO=3, MOSI=6, CS=7, RST=8, DIO1=33, BUSY=34
- OLED I2C: SDA=18, SCL=17
- LED: GPIO 37
- Boot Button: GPIO 0 (menu navigation)

**No additional hardware required. Total cost: $0.**

### 2.2 JAMMER-GPS Hardware

| Component | Specification | Purpose | Status |
|-----------|--------------|---------|--------|
| AntSDR E200 (Geekom clone) | AD9361, 70 MHz–6 GHz, 12-bit, 2x2 MIMO, GigE | GPS signal generation and jamming | ✅ Already owned |
| PC/Laptop | Windows or Linux, Python 3.10+, GigE port | Runs JAMMER-GPS GUI application | ✅ Already owned |

**Conducted Test RF Path (must purchase):**

| Component | Specification | Source | Cost |
|-----------|--------------|--------|------|
| SMA male-to-male cable | 50Ω, 30cm–1m | Amazon/DigiKey | $5 |
| SMA 20 dB fixed attenuator | DC-6 GHz, 2W | Mini-Circuits/Amazon | $8 |
| SMA 20 dB fixed attenuator | DC-6 GHz, 2W (second one) | Mini-Circuits/Amazon | $8 |
| SMA 10 dB fixed attenuator | DC-6 GHz, 2W | Mini-Circuits/Amazon | $6 |
| SMA Tee adapter | M-F-F | Amazon/DigiKey | $5 |
| U.FL to SMA pigtail | 10–15cm | SparkFun/Amazon | $3 |
| RF shielding pouch | Faraday bag, >60 dB isolation | Amazon | $15 |
| **Total** | | | **~$50** |

**Why multiple attenuators instead of one 30 dB:** Stackable attenuators let you test at different power levels (20, 30, 40, 50 dB) to characterize your M10's capture threshold. This is essential — see Section 5.3.

### 2.3 Technical Validation Summary

Every component has been validated against shipping products and published research. Full references in Appendix A.

| Component | Proven By | API/Method |
|-----------|-----------|------------|
| ESP32 WiFi beacon TX | Espressif official API, 5+ shipping projects | `esp_wifi_80211_tx()` — sends beacon/probe/action frames |
| ESP32 BLE advertising | ESP-IDF BLE API, nyanBOX project | `esp_ble_gap_config_adv_data_raw()` |
| SX1262 LoRa/FSK TX | RadioLib v7, SENTRY-RF already uses this | `radio.transmit()`, `radio.beginFSK()` |
| WiFi + SX1262 dual-core | Different HW buses (internal WiFi vs SPI), Sky-Spy precedent | FreeRTOS task pinning, Core 0 / Core 1 |
| AntSDR GPS spoofing via UHD | gps-sdr-sim + USRP B200 peer-reviewed (UND 2018) | `tx_samples_from_file`, `gps-sdr-sim-uhd.py` |
| AntSDR GPS jamming | Standard SDR practice, GP-Jammer reference | UHD Python TX streaming + NumPy IQ generation |
| ASTM F3411 encoding | opendroneid-core-c official library | C encoding functions, MIT licensed |

---

## 3. JAMMER-RF — ESP32 FIRMWARE DESIGN

### 3.1 Menu System

```
JUH-MAK-IN JAMMER v1.0
> [1] Remote ID Spoofer
  [2] RF Signal Generator
  [3] False Positive Gen
  [4] Combined Attack
  [5] Settings
  
BOOT = select | LONG = back
```

Navigation: Short press BOOT button cycles options, long press (>1s) selects. Long press from submenu returns to main menu.

### 3.2 Mode 1: Remote ID Spoofer

**Purpose:** Generate fake Open Drone ID broadcasts that SENTRY-RF's Sprint 8 WiFi scanner must detect, parse, and track.

**Features:**
- **Single drone mode:** One configurable fake drone (serial, position, altitude, speed, heading)
- **Swarm mode:** Up to 16 simultaneous fake drones with independent MAC addresses
- **Dual transport:** WiFi beacon frames (via `esp_wifi_80211_tx`) AND BLE 4 Legacy Advertising simultaneously
- **Flight patterns:**
  - Static hover (fixed position)
  - Circular orbit (configurable radius and center)
  - Random wander (bounded random walk)
  - Linear transit (point A to point B)
- **Realism engine:** GPS jitter (±0.0001°), altitude drift (±5m), speed variation (±2 m/s), heading wobble (±5°), TX timing jitter (±50ms)
- **Configurable via serial JSON:** Change any parameter without reflashing

**ASTM F3411 Messages per drone per cycle:**
1. Basic ID (serial number, UA type)
2. Location/Vector (lat, lon, alt, speed, heading, timestamp)
3. System (operator location, area count, area radius)
4. Operator ID (operator registration string)

**Implementation References:**
- WiFi beacon assembly: Jeija/esp32-80211-tx pattern
- ODID message encoding: opendroneid-core-c library
- Swarm MAC cycling: jjshoots/RemoteIDSpoofer approach
- Flight simulation: colonelpanichacks realism engine concept

**OLED Display:**
```
RID SPOOF [SWARM x8]
TX: WiFi+BLE  Pkt: 1247
36.8529N  75.978W
Pattern: ORBIT R:200m
```

**Key Test Metrics to Record:**
- Total packets transmitted (WiFi beacons + BLE adverts)
- Number of unique drone IDs broadcast
- Transmission rate (packets/sec per drone)

### 3.3 Mode 2: RF Signal Generator

**Purpose:** Generate sub-GHz signals that SENTRY-RF's Sprint 2 spectrum scanner and Sprint 6 detection engine must detect, classify, and track.

**Sub-modes:**

**2A. ELRS 915 Simulation:**
- LoRa modulation: SF6, BW500 kHz (matches ELRS 200Hz mode)
- 80 channels: 902.0–928.0 MHz
- Pseudo-random FHSS with configurable seed
- 4-byte payload mimicking ELRS frame structure
- Hop rate: 200 Hz (5ms dwell per channel)
- TX power: configurable -9 to +22 dBm

**2B. Crossfire 915 Simulation:**
- FSK modulation: 85.1 kBaud, deviation 25 kHz (matches Crossfire 150Hz mode)
- 100 channels at 260 kHz spacing: 902.165–927.905 MHz
- Also includes LoRa mode (SF12 BW500) for Crossfire 50Hz mode
- TX power: configurable

**2C. CW Tone:**
- Single-frequency continuous carrier at configurable freq (default: 915.0 MHz)
- Use case: calibrate scanner noise floor and detection threshold

**2D. Band Sweep:**
- Linear frequency sweep 860–930 MHz in configurable step size
- Dwell time per step: configurable (default 10ms)
- Use case: test full scanner bandwidth response

**Key Test Metrics to Record:**
- Packets transmitted count (for hop modes)
- Current frequency and power level
- Elapsed time (for calculating detection latency on SENTRY-RF side)

### 3.4 Mode 3: False Positive Generator ⭐ NEW — Critical Addition

**Purpose:** Generate ISM-band signals that look like legitimate IoT devices — NOT drones. SENTRY-RF's detection engine must correctly IGNORE these while still detecting real drone signals. This tests false alarm rate (Pfa).

**Sub-modes:**

**3A. LoRaWAN Simulation:**
- LoRa modulation: SF7-SF12, BW125 kHz (standard LoRaWAN parameters)
- Single channel (e.g., 903.9 MHz) — LoRaWAN devices don't frequency hop like ELRS
- Low duty cycle: 1 transmission every 30-60 seconds (typical IoT behavior)
- 20-50 byte payloads (typical sensor data size)

**3B. ISM Burst Noise:**
- Random short RF bursts (10-50ms) at random frequencies in 902-928 MHz
- Simulates garage door openers, tire pressure monitors, weather stations
- Varied modulation: OOK, FSK, random noise bursts

**3C. Mixed Scenario (Most Realistic):**
- Runs LoRaWAN + ISM bursts as background noise
- Simultaneously transmits ELRS or Crossfire drone signals
- SENTRY-RF must detect the drone while ignoring the IoT devices
- This is the test that professional counter-UAS systems must pass

**OLED Display:**
```
FALSE POS GEN [MIXED]
IoT: LoRaWAN @903.9 SF7
Burst: Random ISM 902-928
Drone: ELRS 915 FHSS
Total TX: 3,842
```

### 3.5 Mode 4: Combined Attack

**Purpose:** Simulate a full drone threat scenario with multiple signal sources active simultaneously. Tests SENTRY-RF's threat correlation engine (Sprint 6) and multi-sensor fusion.

**Scenario Sequencer** (configurable delays):
```
T+0s:    LoRa ELRS signal begins (drone control link detected)
T+Xs:    Remote ID appears (drone is now broadcasting identification)
T+Ys:    [If JAMMER-GPS also running] GPS anomaly begins
```

Default timing: X=15s, Y=30s. Configurable via serial JSON.

**FreeRTOS Architecture:**
- Core 0: WiFi beacon TX + BLE advertising (Remote ID)
- Core 1: SX1262 LoRa/FSK TX (drone signal simulation)
- Shared state via mutex-protected struct (same pattern as SENTRY-RF itself)

**Expected SENTRY-RF Response:**
- T+0s: ADVISORY (RF peak on known ELRS channel)
- T+Xs: WARNING (RF + Remote ID correlation)
- T+Ys: CRITICAL (RF + Remote ID + GNSS anomaly)

### 3.6 Mode 5: Settings

Configurable via OLED menu or serial JSON:
- GPS center coordinates (lat/lon for Remote ID)
- TX power level (-9 to +22 dBm)
- Frequency band (868 EU / 915 US)
- Number of swarm drones (1-16)
- Flight pattern selection
- Signal mode (ELRS / Crossfire / CW / Sweep / LoRaWAN / Mixed)
- Combined mode timing offsets
- **Randomize mode:** Randomizes all parameters within configured bounds for unbiased testing

### 3.7 Project Structure

```
juh-mak-in-jammer-rf/
├── platformio.ini
├── include/
│   ├── board_config.h            # Pin definitions (reuse from SENTRY-RF)
│   ├── version.h                 # "JUH-MAK-IN JAMMER v1.0"
│   ├── menu_system.h             # OLED menu + button navigation
│   ├── rid_spoofer.h             # Remote ID orchestrator
│   ├── odid_messages.h           # ASTM F3411 message encoding
│   ├── flight_patterns.h         # Drone movement simulation
│   ├── lora_siggen.h             # LoRa/FSK signal generator
│   ├── drone_freq_tables.h       # ELRS/Crossfire channel plans
│   ├── false_positive_gen.h      # IoT/ISM noise generator
│   ├── combined_mode.h           # Multi-signal scenario sequencer
│   ├── test_metrics.h            # Packet counters, timestamps
│   └── settings.h                # Persistent configuration (NVS)
├── src/
│   ├── main.cpp                  # Menu + FreeRTOS task creation
│   ├── menu_system.cpp
│   ├── rid_spoofer.cpp           # Orchestrates WiFi + BLE TX
│   ├── rid_wifi_tx.cpp           # Raw beacon frame assembly
│   ├── rid_ble_tx.cpp            # BLE advertising with ODID payloads
│   ├── odid_messages.cpp         # ASTM F3411 packing (from opendroneid-core-c)
│   ├── flight_patterns.cpp       # Static, orbit, wander, transit
│   ├── lora_siggen.cpp           # RadioLib TX orchestration
│   ├── lora_elrs_sim.cpp         # ELRS FHSS pattern
│   ├── lora_crossfire_sim.cpp    # Crossfire FSK/LoRa modes
│   ├── drone_freq_tables.cpp     # Channel lookup tables
│   ├── false_positive_gen.cpp    # LoRaWAN + ISM burst generation
│   ├── combined_mode.cpp         # Scenario sequencer with timing
│   ├── test_metrics.cpp          # Counters and serial JSON reporting
│   └── settings.cpp              # NVS read/write, serial config
├── lib/
│   └── opendroneid/              # opendroneid-core-c (relevant files)
├── LICENSE                       # MIT
└── README.md
```

### 3.8 PlatformIO Configuration

```ini
[platformio]
default_envs = jammer_rf

[env:jammer_rf]
platform = espressif32
board = lilygo-t3-s3
framework = arduino
monitor_speed = 115200
build_flags =
    -DBOARD_T3S3
    -DARDUINO_USB_CDC_ON_BOOT=1
    -DJAMMER_MODE=1
lib_deps =
    jgromes/RadioLib@^7.0.0
    adafruit/Adafruit SSD1306@^2.5.7
    adafruit/Adafruit GFX Library@^1.11.9
board_build.partitions = huge_app.csv
```

Note: `huge_app.csv` partition scheme needed because WiFi + BLE + RadioLib + OLED libraries together are large.

---

## 4. JAMMER-GPS — PYTHON PC APPLICATION DESIGN

### 4.1 AntSDR E200 Configuration

**Firmware:** UHD mode (boot from SD card)
- UHD firmware from: https://github.com/MicroPhase/antsdr_uhd
- Device appears as "ANTSDR-E200" at 192.168.1.10 via Gigabit Ethernet

**Verification commands (run before any testing):**
```bash
# Discover device
uhd_find_devices
# Expected: "name: ANTSDR-E200, addr: 192.168.1.10, type: ant"

# Probe capabilities
uhd_usrp_probe
# Expected: TX Frontend A+B, Freq range 70-6000 MHz, Gain range 0-89.75 dB
```

### 4.2 GPS Spoofing Module

**Engine:** gps-sdr-sim (github.com/osqzss/gps-sdr-sim, MIT license)

**Spoofing Modes:**

**4.2A. Static Position Spoof:**
- Teleports receiver to a fixed lat/lon/alt
- Simplest test — verifies spoofing pipeline works end-to-end
- Command: `./gps-sdr-sim -e brdc.n -l lat,lon,alt -b 16 -o gpssim.bin`

**4.2B. Trajectory Spoof:**
- Receiver follows a predefined path (NMEA or CSV waypoints)
- Tests SENTRY-RF's position jump detection at various drift rates
- Command: `./gps-sdr-sim -e brdc.n -g trajectory.nmea -b 16 -o gpssim.bin`

**4.2C. Slow Drift Spoof** ⭐ NEW — Critical Test:
- Starts at the receiver's true position, then drifts 0.5 m/s in a direction
- Over 30 minutes, the position moves ~900m — within plausible walking speed
- Tests whether SENTRY-RF's position consistency check catches gradual drift
- Generated by creating a custom NMEA trajectory with very slow movement

**4.2D. Cold Start Spoof** ⭐ NEW — Critical Test:
- Spoofer transmits continuously while the M10 is power-cycled
- Tests whether SENTRY-RF detects spoofing when the receiver boots directly into fake signals
- This bypasses spoofDetState (which only detects transitions from real to fake)
- SENTRY-RF's C/N0 uniformity analysis should catch this even when spoofDetState doesn't

**Transmission via UHD:**
```bash
# 16-bit signed IQ for UHD devices
tx_samples_from_file --file gpssim.bin --type short --rate 2500000 \
    --freq 1575420000 --gain 0 --args "addr=192.168.1.10"
```

### 4.3 GPS Jamming Module

**Engine:** Custom NumPy IQ generation → UHD Python TX streaming

**Jamming Waveforms:**

| Waveform | Description | What It Tests on M10 |
|----------|-------------|---------------------|
| Wideband noise | Gaussian noise, 2 MHz BW centered on L1 | jamInd rise, AGC drop, satellite loss |
| CW tone | Single carrier at 1575.42 MHz | CW jamming indicator specifically |
| Chirp sweep | Linear FM sweep across L1 band | Swept interference resilience |
| Pulsed noise | 50ms on / 200ms off duty cycle | Recovery time after intermittent jamming |
| Matched spectrum | GPS-like PRN code without valid nav data | Hardest to detect — tests advanced algorithms |

**Power Control:**
- AntSDR AD9361 TX gain range: -89.75 to 0 dB
- At 0 dB gain, output is approximately +10 dBm
- Through 50 dB attenuation: signal arrives at M10 at approximately -40 dBm
- Real GPS at ground level: approximately -130 dBm
- So spoofed/jammed signal is ~90 dB above real GPS — still very strong but more realistic than the 100+ dB margin at 30 dB attenuation

### 4.4 Combined Attack Mode (MIMO)

**Uses AntSDR's dual TX channels simultaneously:**
- TX1 (SMA): GPS spoofing signal
- TX2 (U.FL or second SMA): GPS jamming signal

**Attack Sequence:**
```
Phase 1 — JAM (T=0 to T=30s):
  TX2 transmits wideband noise at L1
  Purpose: Break M10's lock on real satellites
  
Phase 2 — SPOOF + REDUCED JAM (T=30s to T=90s):
  TX1 begins transmitting fake GPS via gps-sdr-sim
  TX2 reduces jam power gradually (ramp down over 30s)
  Purpose: M10 acquires spoofed signals as jam clears
  
Phase 3 — SPOOF ONLY (T=90s+):
  TX1 continues spoofing
  TX2 off
  Purpose: M10 now tracking spoofed position
```

**If MIMO dual-TX doesn't work on AntSDR** (needs verification):
Run phases sequentially — stop jamming, then start spoofing. Less elegant but still validates SENTRY-RF's detection.

### 4.5 Scenario Presets (JSON)

```json
{
  "name": "Quick Static Spoof",
  "description": "Teleport 1km north — basic functionality test",
  "mode": "spoof",
  "target_lat": 36.8619,
  "target_lon": -75.978,
  "target_alt": 100,
  "duration_sec": 120,
  "tx_gain_db": -30,
  "attenuation_note": "Use 50 dB total attenuation"
}
```

**Preset Library:**
1. `quick_static_spoof.json` — Move 1 km north (basic test)
2. `jamming_only_30sec.json` — Wideband noise for 30 seconds (test MON-RF)
3. `slow_drift_30min.json` — 0.5 m/s drift for 30 minutes (test position consistency)
4. `cold_start_spoof.json` — Spoofer on before M10 boots (test C/N0 analysis)
5. `full_attack_mimo.json` — Jam then spoof sequence (most realistic attack)
6. `stress_test_jumps.json` — Rapid 1km position jumps every 10 seconds
7. `cw_jamming_sweep.json` — CW tone power ramp from -60 to 0 dBm (find jam threshold)
8. `randomized_scenario.json` — All parameters randomized within bounds (unbiased testing)

### 4.6 Project Structure

```
juh-mak-in-jammer-gps/
├── requirements.txt
├── setup.py
├── README.md
├── src/
│   ├── main.py                   # GUI entry point
│   ├── gui/
│   │   ├── main_window.py        # PyQt6 tabbed interface
│   │   ├── map_widget.py         # Folium coordinate picker
│   │   ├── spoof_panel.py        # GPS spoofing controls + scenario selector
│   │   ├── jam_panel.py          # Jamming waveform controls
│   │   ├── combined_panel.py     # MIMO attack sequence controls
│   │   ├── metrics_panel.py      # Live test metrics display ⭐ NEW
│   │   └── status_bar.py         # SDR connection, TX status
│   ├── sdr/
│   │   ├── antsdr_interface.py   # UHD device discovery + connection
│   │   ├── tx_streamer.py        # IQ streaming (single and MIMO)
│   │   └── sdr_config.py         # Frequency, gain, sample rate presets
│   ├── gps/
│   │   ├── spoof_engine.py       # Wraps gps-sdr-sim subprocess
│   │   ├── jam_engine.py         # NumPy waveform generation
│   │   ├── ephemeris.py          # Auto-download NASA BRDC files
│   │   ├── scenarios.py          # JSON scenario loader + randomizer
│   │   └── trajectory_gen.py     # Slow drift + custom path generation ⭐ NEW
│   ├── metrics/
│   │   ├── test_logger.py        # CSV logging with timestamps ⭐ NEW
│   │   ├── detection_timer.py    # T(attack) → T(detect) measurement ⭐ NEW
│   │   └── power_sweep.py        # Automated attenuation characterization ⭐ NEW
│   └── utils/
│       ├── config.py             # User settings (QSettings)
│       └── logging_setup.py
├── gps-sdr-sim/                  # Git submodule
├── scenarios/                    # JSON preset files
├── ephemeris/                    # Cached NASA BRDC files
├── logs/                         # Test result CSV files ⭐ NEW
└── reports/                      # Generated test reports ⭐ NEW
```

---

## 5. TEST METHODOLOGY & OPERATING PROCEDURES

### 5.1 Conducted Test Setup

```
AntSDR E200 (AD9361)
├── TX1/SMA ─── [20dB] ─── [20dB] ─── [10dB] ─── SMA Tee ─── U.FL ─── M10 ANT
│              (stackable: use 20, 30, 40, or 50 dB total)    │
│                                                          (optional)
│                                                         GPS Antenna
│                                                      (for mixed real+fake)
│
└── TX2/SMA ─── [20dB] ─── [20dB] ─── (same tee or direct to second U.FL)
                (for jamming channel)
```

**Power Level Reference Table:**

| AntSDR TX Gain | Output Power | Through 50 dB Atten | At M10 Input | vs. Real GPS (-130 dBm) |
|---------------|-------------|---------------------|-------------|------------------------|
| 0 dB (max) | +10 dBm | -40 dBm | -40 dBm | +90 dB above real |
| -10 dB | 0 dBm | -50 dBm | -50 dBm | +80 dB above real |
| -20 dB | -10 dBm | -60 dBm | -60 dBm | +70 dB above real |
| -40 dB | -30 dBm | -80 dBm | -80 dBm | +50 dB above real |
| -60 dB | -50 dBm | -100 dBm | -100 dBm | +30 dB above real |
| -80 dB | -70 dBm | -120 dBm | -120 dBm | +10 dB above real |
| -89 dB | -79 dBm | -129 dBm | -129 dBm | ~Equal to real GPS |

**Start testing at -40 dB TX gain (arriving at ~-80 dBm through 50 dB attenuation) and sweep upward to find the capture threshold.**

### 5.2 Test Execution Protocol

**For every test scenario, record:**

| Metric | How to Measure | Why It Matters |
|--------|---------------|----------------|
| T_attack | Timestamp when JAMMER starts transmitting | Reference point |
| T_indicator | Timestamp when first SENTRY-RF indicator changes | Raw detection speed |
| T_alarm | Timestamp when SENTRY-RF raises threat level | End-to-end latency |
| Detection latency | T_alarm - T_attack | Time to respond |
| Pd (detection probability) | Detections / total test runs at each power level | Receiver sensitivity |
| Pfa (false alarm rate) | False alarms / total observation time with no attack | Specificity |
| Packets TX'd | From JAMMER-RF serial output | For calculating detection ratio |
| Packets detected | From SENTRY-RF serial output | Detection ratio = detected/TX'd |

### 5.3 Required Test Sequences

Run these in order. Each builds on the previous.

**Phase 1: Baseline Characterization (Before Any Attack Testing)**

| Test | Procedure | Expected Result |
|------|-----------|----------------|
| 1.1 RF noise floor | Run SENTRY-RF spectrum scanner for 10 min with no JAMMER active. Log min/max/mean RSSI per frequency bin. | Establishes detection threshold baseline |
| 1.2 GNSS baseline | Run SENTRY-RF GNSS monitor for 1 hour under open sky. Log C/N0 mean and σ per satellite, jamInd, agcCnt, spoofDetState. | Establishes normal C/N0 σ (~4-8 dBHz) and normal jamInd (~5-15) |
| 1.3 ISM environment survey | Log all RF activity in 860-930 MHz for 30 min. Note any persistent signals from local IoT devices. | Identifies potential false positive sources |

**Phase 2: Individual Component Tests**

| Test | JAMMER Mode | SENTRY-RF Feature Tested | Pass Criteria |
|------|-------------|-------------------------|---------------|
| 2.1 CW detection | RF Signal Gen → CW at 915 MHz, +10 dBm | Spectrum scanner peak detection | Peak visible on OLED spectrum within 3 sweeps |
| 2.2 ELRS detection | RF Signal Gen → ELRS 915, +10 dBm | Detection engine ELRS signature match | "ELRS" identified in serial output within 10s |
| 2.3 Crossfire detection | RF Signal Gen → Crossfire FSK, +10 dBm | Detection engine Crossfire signature match | "Crossfire" identified within 10s |
| 2.4 False positive rejection | False Positive Gen → LoRaWAN only | Detection engine should NOT trigger | No drone alert for 5 minutes |
| 2.5 Mixed signal discrimination | False Positive Gen → Mixed (LoRaWAN + ELRS) | Detect ELRS, ignore LoRaWAN | ELRS detected, no false alarm from LoRaWAN |
| 2.6 Remote ID detection | RID Spoofer → 1 drone, WiFi+BLE | WiFi scanner drone detection | Drone appears in SENTRY-RF output within 30s |
| 2.7 Swarm capacity | RID Spoofer → 16 drones, WiFi+BLE | WiFi scanner multi-drone tracking | All 16 drones tracked simultaneously |
| 2.8 GPS jamming detection | JAMMER-GPS → Wideband noise, -20 dB gain | MON-RF jamInd rise, fix degradation | jamInd > 100 within 5s, jammingState ≥ 2 |
| 2.9 GPS spoof detection (non-coherent) | JAMMER-GPS → Static spoof, -30 dB gain | NAV-STATUS spoofDetState or C/N0 anomaly | Any spoofing indicator triggers within 60s |
| 2.10 GPS cold-start spoof | JAMMER-GPS → Spoof running, then power cycle M10 | Host-side C/N0 analysis (spoofDetState may fail) | C/N0 σ below threshold triggers alarm |

**Phase 3: Integration Tests**

| Test | Scenario | Expected SENTRY-RF Response |
|------|----------|----------------------------|
| 3.1 RF only → ADVISORY | ELRS signal, no Remote ID, no GPS anomaly | Threat: ADVISORY |
| 3.2 RF + RID → WARNING | ELRS + Remote ID, no GPS anomaly | Threat escalates to WARNING |
| 3.3 RF + RID + GPS → CRITICAL | ELRS + Remote ID + GPS jamming | Threat escalates to CRITICAL |
| 3.4 GPS only → separate alert | GPS jamming, no RF, no Remote ID | GNSS integrity alert (not drone alert) |
| 3.5 Timed escalation | Combined mode with 15s/30s delays | ADVISORY→WARNING→CRITICAL at expected times |
| 3.6 Cooldown test | Run test 3.3, then stop all signals | Threat decays to CLEAR within 30s |

**Phase 4: Edge Case and Vulnerability Tests**

| Test | Scenario | What It Reveals |
|------|----------|-----------------|
| 4.1 Capture threshold | GPS spoof at decreasing power (-20 to -89 dB TX gain) | Minimum spoofing power to capture M10 |
| 4.2 Jam threshold | GPS jam at decreasing power | Minimum jamming power to degrade fix |
| 4.3 Slow drift detection limit | 0.1, 0.3, 0.5, 1.0 m/s drift rates | Minimum drift rate SENTRY-RF catches |
| 4.4 Scanner detection probability | 1000 ELRS packets at various power levels | Pd vs. signal power curve |
| 4.5 Randomized stress test | All parameters randomized, 100 runs | Overall system reliability |

### 5.4 Test Reporting

JAMMER-GPS generates a CSV log for every test session:

```csv
timestamp_utc, test_name, attack_type, tx_gain_db, attenuation_db, signal_at_receiver_dbm,
t_attack_ms, t_first_indicator_ms, t_alarm_ms, detection_latency_ms,
sentry_threat_level, sentry_jam_ind, sentry_spoof_det, sentry_cno_sigma,
packets_transmitted, packets_detected, detection_ratio, notes
```

---

## 6. BUILD SEQUENCE (8 Weeks, Parallel)

### Weeks 1-2: Foundations + Baseline

**JAMMER-RF:**
- [ ] Create PlatformIO project with board_config.h
- [ ] Build menu system: OLED display + boot button navigation
- [ ] Implement Mode 2: CW tone and frequency sweep
- [ ] Test against SENTRY-RF → Run Test 2.1 (CW detection)
- [ ] Record RF noise floor baseline (Test 1.1)

**JAMMER-GPS:**
- [ ] Install MicroPhase UHD drivers
- [ ] Boot AntSDR E200 with UHD firmware
- [ ] Run `uhd_find_devices` and `uhd_usrp_probe` → verify AD9361 detected
- [ ] Compile gps-sdr-sim, generate first IQ file
- [ ] Assemble conducted test RF path (attenuators + cables)
- [ ] Transmit static spoof → verify M10 shows spoofed position
- [ ] Record GNSS baseline (Test 1.2) — 1 hour under open sky
- [ ] **Milestone: Basic GPS spoofing works through cable**

### Weeks 3-4: Core Signal Generation

**JAMMER-RF:**
- [ ] Implement ELRS frequency hopping simulation
- [ ] Implement Crossfire FSK simulation
- [ ] Run Tests 2.2, 2.3 (ELRS/Crossfire detection)
- [ ] Begin Remote ID spoofer: single drone WiFi beacon
- [ ] Integrate opendroneid-core-c library

**JAMMER-GPS:**
- [ ] Build jamming module: NumPy noise + UHD TX streaming
- [ ] Run Test 2.8 (GPS jamming detection)
- [ ] Build basic PyQt6 GUI with spoof and jam tabs
- [ ] Add ephemeris auto-download
- [ ] Run Test 2.9 (non-coherent spoofing detection)
- [ ] **Test MIMO dual-TX** → attempt simultaneous spoof + jam

### Weeks 5-6: Advanced Features + False Positive Testing

**JAMMER-RF:**
- [ ] Add BLE advertising (dual-transport Remote ID)
- [ ] Implement swarm mode (16 drones)
- [ ] Implement flight patterns (orbit, wander, transit)
- [ ] **Build False Positive Generator** (Mode 3) — LoRaWAN + ISM bursts
- [ ] Run Tests 2.4, 2.5 (false positive rejection, mixed discrimination)
- [ ] Implement Combined Mode with scenario sequencer
- [ ] Monitor memory: verify heap stays above 50 KB free

**JAMMER-GPS:**
- [ ] Implement slow drift trajectory generation
- [ ] Implement cold-start spoof scenario
- [ ] Run Tests 2.10, 4.3 (cold-start, slow drift)
- [ ] Build metrics panel: live display of detection latency
- [ ] Add test logging (CSV output)
- [ ] Implement power sweep automation (Test 4.1, 4.2)

### Weeks 7-8: Integration, Edge Cases, Documentation

**Both tools together:**
- [ ] Run full Phase 3 integration test sequence (Tests 3.1–3.6)
- [ ] Run Phase 4 edge case tests (4.1–4.5)
- [ ] Run 100-iteration randomized stress test
- [ ] Generate test report from CSV logs
- [ ] Write comprehensive README for both repos
- [ ] Document all known limitations (see Section 7)
- [ ] Create wiring diagrams and photos for conducted test setup

---

## 7. KNOWN LIMITATIONS (Document Honestly)

| Limitation | Why | Impact |
|-----------|-----|--------|
| GPS spoofing is non-coherent only | gps-sdr-sim cannot synchronize with real sky signals | Sophisticated coherent spoofing attacks are not tested |
| GPS L1 only (no GLONASS/BeiDou spoofing) | No open-source multi-constellation spoofer is production-ready | M10's multi-constellation cross-validation will detect GPS-only spoofing |
| SX1262 sweep speed (1-3s) limits hop detection | Physics of PLL settling and ADC reads | Fast FHSS signals (5ms dwell) detected statistically, not per-hop |
| Remote ID has no authentication | ASTM F3411 protocol limitation, not a test tool issue | Cannot test fake vs real RID discrimination (impossible without crypto) |
| RF-silent autonomous drones invisible | No RF emissions to detect | Fundamental limitation of passive RF detection approach |
| Same developer builds detector and tester | Potential bias in test case design | Mitigated by randomized scenario mode |
| MIMO dual-TX unverified on AntSDR E200 | No published reference for this specific use case | Fallback to sequential operation if needed |

---

## 8. GITHUB REPOSITORY STRUCTURE

```
Seaforged/
├── SENTRY-RF/                    # The detector (existing repo)
├── juh-mak-in-jammer-rf/         # ESP32 test tool (new repo)
│   ├── platformio.ini
│   ├── include/
│   ├── src/
│   ├── lib/opendroneid/
│   ├── LICENSE
│   └── README.md
└── juh-mak-in-jammer-gps/        # PC/SDR test tool (new repo)
    ├── requirements.txt
    ├── setup.py
    ├── src/
    ├── gps-sdr-sim/              # git submodule
    ├── scenarios/
    ├── ephemeris/
    ├── logs/
    ├── LICENSE
    └── README.md
```

---

## APPENDIX A: REFERENCE LINKS

### ESP32 Raw WiFi TX
- Espressif ESP-IDF WiFi API (esp_wifi_80211_tx): https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/network/esp_wifi.html
- Jeija/esp32-80211-tx: https://github.com/Jeija/esp32-80211-tx
- ESP32 beacon + action frame gist: https://gist.github.com/georgcampana/0d4098e87f26ff478480727dc2737c20

### Remote ID Spoofer Reference Projects
- jjshoots/RemoteIDSpoofer: https://github.com/jjshoots/RemoteIDSpoofer
- colonelpanichacks/oui-spy: https://github.com/colonelpanichacks/oui-spy
- sycophantic/Remote-ID-Spoofer: https://github.com/sycophantic/Remote-ID-Spoofer
- cyber-defence-campus/droneRemoteIDSpoofer: https://github.com/cyber-defence-campus/droneRemoteIDSpoofer
- jbohack/nyanBOX: https://github.com/jbohack/nyanBOX

### ASTM F3411 / Open Drone ID
- opendroneid-core-c: https://github.com/opendroneid/opendroneid-core-c

### GPS Spoofing
- gps-sdr-sim: https://github.com/osqzss/gps-sdr-sim
- galileo-sdr-sim: https://github.com/harshadms/galileo-sdr-sim
- UND GPS Spoofing Paper (peer-reviewed): https://jgps.springeropen.com/articles/10.1186/s41445-018-0018-3
- GPSPATRON u-blox M8T spoofing test: https://gpspatron.com/ublox-m8t-gps-spoofing-test/
- GPSPATRON spoofing scenarios: https://gpspatron.com/gnss-spoofing-scenarios-with-sdrs/
- GPSPATRON u-blox F9T spoofing: https://gpspatron.com/spoofing-a-multi-band-rtk-gnss-receiver-with-hackrf-one-and-gnss-jammer/

### u-blox GNSS Security
- u-blox OSNMA: https://www.u-blox.com/en/technologies/osnma-galileo-spoofing
- u-blox Jammertest 2025 results: https://www.u-blox.com/en/blogs/tech/gnss-jamming-spoofing-detection-jammertest-2025-andoya
- u-blox spoofing threat overview: https://www.u-blox.com/en/blogs/insights/looming-threat-gps-jamming-and-spoofing

### AntSDR E200
- Crowd Supply page: https://www.crowdsupply.com/microphase-technology/antsdr-e200
- Getting started: https://antsdr-doc-en.readthedocs.io/
- UHD firmware: https://github.com/MicroPhase/antsdr_uhd
- PlutoSDR firmware: https://github.com/MicroPhase/antsdr-fw-patch

### Counter-UAS Professional References
- DHS C-UAS Technology Guide: https://www.dhs.gov/sites/default/files/publications/c-uas-tech-guide_final_28feb2020.pdf
- CRFS non-library C-UAS detection: https://www.crfs.com/blog/why-modern-automated-non-library-based-counter-unmanned-aerial-systems-are-vital
- Safran GNSS vulnerability testing: https://safran-navigation-timing.com/the-importance-of-testing-to-detect-gnss-vulnerabilities-in-intelligent-transportation-systems/
- Stanford GPS Lab monitoring approach (GPS World): https://www.gpsworld.com/innovation-monitoring-gnss-interference-and-spoofing-a-low-cost-approach/
- EUROCONTROL C-UAS workshop: https://www.eurocontrol.int/sites/default/files/2024-11/eurocontrol-2024-cuas-workshop-s2-berz.pdf

### RadioLib
- RadioLib: https://github.com/jgromes/RadioLib

### NASA Ephemeris
- CDDIS daily BRDC archive: https://cddis.nasa.gov/archive/gnss/data/daily/
