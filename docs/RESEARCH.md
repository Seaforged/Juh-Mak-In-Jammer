# SENTRY-RF Test Equipment Research
## Everything Available for GPS Spoofing, GPS Jamming, Fake Drone IDs & Fake Drone RF Signals

---

## 1. FAKE DRONE REMOTE ID (WiFi & BLE Beacons)

These tools broadcast fake Open Drone ID packets (ASTM F3411) that will appear as real drones on any compliant receiver — including your SENTRY-RF Sprint 8 WiFi scanner, the OpenDroneID phone app, DroneTag, and even DJI AeroScope.

### 1A. jjshoots/RemoteIDSpoofer ⭐ EASIEST START
- **URL:** github.com/jjshoots/RemoteIDSpoofer
- **Hardware:** ESP8266 (NodeMCU) or ESP32 — ~$5
- **What it does:** Spawns 16 fake drones broadcasting Remote ID via WiFi, all flying random patterns around a configurable GPS coordinate
- **Setup:** Arduino IDE → flash → connect to WiFi AP "ESP_RIDS" (password: makkauhijau) → enter GPS coords at 192.168.4.1
- **Protocol:** WiFi beacon frames with Open Drone ID vendor-specific IEs
- **Limit:** Maxes out at 16 drones due to MAC address cycling constraints
- **License:** Educational use disclaimer, similar to ADS-B spoofing legality concerns
- **Best for:** Quick and dirty testing — flash a spare ESP8266 and go

### 1B. colonelpanichacks/Remote-ID-Spoofer ⭐ MOST FEATURE-RICH
- **URL:** Part of the OUI-SPY ecosystem (github.com/colonelpanichacks/oui-spy)
- **Hardware:** Seeed XIAO ESP32-S3 (~$8) or OUI-SPY custom board (~$25 on Tindie)
- **What it does:**
  - WiFi-based ASTM F3411 Remote ID broadcast via NAN action frames AND AP beacon vendor IEs
  - **Swarm mode: up to 20 simultaneous fake drones** with configurable formations
  - Realistic variation engine: altitude drift, speed variation, GPS jitter, heading wobble, TX timing jitter — all adjustable with live sliders
  - Flask web UI with map-based flight path planning
  - Play/Pause/Stop controls with live telemetry feeds
- **Protocol:** Both NDP/NAN and legacy vendor IE frame formats
- **License:** MIT, research/education disclaimer
- **Best for:** Realistic testing with multiple drones and controllable flight behavior
- **Bonus:** The same developer (colonelpanichacks) also makes **Sky-Spy** — a passive Remote ID *detector* on the same hardware. Great test pair.

### 1C. sycophantic/Remote-ID-Spoofer
- **URL:** github.com/sycophantic/Remote-ID-Spoofer
- **Hardware:** Seeed XIAO ESP32-S3
- **What it does:**
  - ESP32 firmware + Python Flask web UI for map-based flight path design
  - Real-time playback controls (Play/Pause/Stop)
  - Dynamic MAC spoofing (randomized on each boot, override via web UI)
  - Pilot location setting, USB port management
- **Protocol:** WiFi NDP/NAN + legacy vendor IE frames
- **License:** MIT
- **Best for:** Visual flight path design and controlled single-drone simulation

### 1D. cyber-defence-campus/droneRemoteIDSpoofer ⭐ MOST RIGOROUS
- **URL:** github.com/cyber-defence-campus/droneRemoteIDSpoofer
- **Hardware:** Linux laptop + WiFi adapter with monitor mode (e.g., EDIMAX EW-7811Un) — NOT an ESP32
- **What it does:**
  - Python script generates raw 802.11 beacon frames AND BLE advertisements with ASTM F3411 payloads
  - Supports WiFi and Bluetooth **simultaneously**
  - Multi-drone with unique serial, MAC, and flight behavior per drone
  - Scenario-based JSON configs for repeatable tests (random, static, waypoints, stress test)
  - Full architecture documented
- **Protocol:** Both WiFi beacons (scapy raw injection) and BLE advertisements (HCI socket)
- **Origin:** Zurich University of Applied Sciences / Swiss Cyber-Defence Campus (thesis project)
- **License:** Academic/research
- **Best for:** Dual-transport (WiFi + BLE) testing, academic rigor, scenario-based repeatability

### 1E. jbohack/nyanBOX
- **URL:** github.com/jbohack/nyanBOX
- **Hardware:** ESP32 Wroom32U
- **What it does:** All-in-one wireless security toolkit with a menu-driven interface. Includes a "Drone Spoofer" mode that broadcasts fake ODID packets over BLE and WiFi per ASTM F3411, generating randomized drone identities, GPS coordinates, altitudes, speeds, and operator IDs.
- **Best for:** If you want a multi-tool device that also does BLE scanning, AirTag spoofing, WiFi analysis, etc.

### 1F. opendroneid/opendroneid-core-c (Reference Library)
- **URL:** github.com/opendroneid/opendroneid-core-c
- **What it is:** The official C library for encoding/decoding Open Drone ID messages. Not a spoofer itself, but the foundation that most ESP32 spoofers are built on. Includes ESP32 transmitter example code.
- **Best for:** Building your own custom Remote ID test transmitter from scratch

### 1G. sxjack/remote_id_bt5 (BLE-only, nRF52840)
- **URL:** github.com/sxjack/remote_id_bt5
- **Hardware:** nRF52840 dongle (~$10)
- **What it does:** Transmits ASTM F3411 remote ID signals over Bluetooth 4 and 5 using Zephyr RTOS. Supports Japanese variation with authorization codes.
- **Best for:** Testing BLE-only Remote ID reception (different hardware than ESP32)

### 1H. Gurkengewuerz/micropython-remoteid
- **URL:** github.com/Gurkengewuerz/micropython-remoteid
- **Hardware:** Any MicroPython-capable ESP32
- **What it does:** MicroPython library for encoding/decoding Open Drone ID messages. BLE advertising only currently.
- **Best for:** If you prefer MicroPython over Arduino/C++

---

## 2. FAKE SUB-GHZ DRONE RF SIGNALS (Testing the LoRa Spectrum Scanner)

These generate RF energy in the 860–930 MHz band to trigger SENTRY-RF's spectrum scanner (Sprint 2).

### 2A. Your Second ESP32+SX1262 Board ⭐ CHEAPEST & BEST
- **Hardware:** Your other T3S3 or Heltec V3 (the one you're NOT using as the detector)
- **What to do:** Write a simple RadioLib sketch that transmits LoRa packets on known ELRS/Crossfire frequencies. Hop between channels to simulate FHSS.
- **Cost:** $0 — you already own it
- **Why it's best:** Generates actual SX1262 LoRa signals at exact ELRS/Crossfire frequencies and modulation parameters

### 2B. Actual ELRS 900 MHz TX Module
- **Hardware:** HappyModel ES900TX (~$20–30), BetaFPV Nano 900MHz, or any ELRS 900MHz TX
- **What to do:** Just power it on. It transmits on 868/915 MHz ISM band immediately when searching for a receiver.
- **Cost:** ~$20–30 if you don't already own one
- **Why it works:** Generates authentic ELRS FHSS signals — the exact waveform your scanner is trying to detect

### 2C. TBS Crossfire TX
- **Hardware:** TBS Crossfire Micro TX V2 (~$60) or any Crossfire transmitter
- **What to do:** Power on. It transmits FSK at 85.1 kBaud with 260 kHz channel spacing in 868/915 MHz.
- **Cost:** ~$60+ (expensive just for testing)
- **Why it works:** Generates authentic Crossfire signals — different modulation (FSK vs LoRa) tests your scanner's broadband detection

### 2D. HackRF One as Signal Generator
- **Hardware:** HackRF One (~$300)
- **What to do:** Use GNU Radio or Portapack Mayhem firmware to generate arbitrary signals at 868/915 MHz. Can simulate CW tones, noise, or custom waveforms.
- **Cost:** ~$300 but serves double duty for GPS spoofing (see Section 3)
- **Why it works:** Complete control over frequency, power, modulation — can test edge cases

---

## 3. GPS SPOOFING (Testing GNSS Integrity — Sprint 4)

These generate fake GPS satellite signals that fool your u-blox M10 into reporting a false position, testing SENTRY-RF's spoofing detection (NAV-STATUS spoofDetState, C/N0 analysis, position jump detection).

### 3A. gps-sdr-sim + HackRF One ⭐ THE STANDARD
- **URL:** github.com/osqzss/gps-sdr-sim (archived Jan 2025, 3.3k stars, 926 forks)
- **Hardware:** HackRF One ($300–350) or ADALM-Pluto ($150) or bladeRF ($400)
- **What it does:**
  - Generates GPS L1 C/A baseband IQ samples from real NASA ephemeris data
  - Supports static position, ECEF motion files, and NMEA traces
  - Output plays through any TX-capable SDR at 1575.42 MHz
  - Proven to work against u-blox receivers (M8, M10, F9)
- **How to use:**
  ```
  # Generate IQ file for a fixed location
  ./gps-sdr-sim -e brdc0250.25n -l 38.6139,37.2090,100 -b 8 -o gpssim.bin
  
  # Transmit via HackRF
  hackrf_transfer -t gpssim.bin -f 1575420000 -s 2600000 -a 1 -x 0
  ```
- **Limitation:** GPS L1 only — cannot spoof GLONASS or BeiDou. Your M10 tracks GPS+Galileo+BeiDou, so spoofDetState should flag this (Galileo shares L1 band and gets blocked, but BeiDou B1I at 1561.098 MHz doesn't)
- **Pro tip:** A TCXO-equipped HackRF gives much more reliable results. Without it, frequency drift can prevent the receiver from locking on.
- **License:** MIT
- **Best for:** The proven, well-documented path. Hundreds of tutorials available.

### 3B. galileo-sdr-sim (Galileo E1 Signals)
- **URL:** github.com/harshadms/galileo-sdr-sim
- **Hardware:** Ettus USRP (preferred) or HackRF/Pluto
- **What it does:**
  - Generates Galileo E1B/C signals with CBOC modulation
  - Tested against u-blox and Septentrio receivers with position accuracy within ~1 meter
  - Real-time transmission capability via USRP
- **Origin:** Northeastern University research (ION GNSS+ 2023 paper)
- **Why useful:** Tests whether your M10's multi-constellation cross-validation catches Galileo-only spoofing
- **License:** Research/academic

### 3C. GNSS-sdr-sim (Multi-constellation — WIP)
- **URL:** github.com/lzs920924/GNSS-sdr-sim
- **Hardware:** ADALM-Pluto or HackRF
- **What it does:**
  - Ambitious project supporting GPS, Galileo, GLONASS, BeiDou, IRNSS, QZSS, and SBAS
  - Python-based with GUI (pluto_studio.py)
  - GPS and Galileo verified to ~10m accuracy; GLONASS and IRNSS verified; BeiDou has known ~5km error
- **Status:** Unfinished, actively developed in spare time
- **Why useful:** Only open-source project attempting full multi-constellation spoofing
- **Caveat:** Quality varies per constellation. Not production-ready.

### 3D. HackRF + Portapack Mayhem "GPS Sim" App ⭐ STANDALONE / NO LAPTOP
- **Hardware:** HackRF One + Portapack H2/H4M (~$150–350 for clones with both)
- **What it does:**
  - Built-in GPS Sim app that plays pre-generated .C8 files from SD card
  - Built-in Jammer app (see Section 4)
  - No laptop needed — fully portable
- **How to use:**
  - Generate .C8 file on PC: `gps-sdr-sim -e brdc.n -l lat,lon,alt -b 8 -o gpssim.C8`
  - Create matching .TXT file with sample rate and center frequency
  - Copy to Portapack SD card → open GPS Sim app → play
- **Best for:** Field-portable GPS spoofing without a laptop

### 3E. GPSPATRON GP-Simulator 2 (Commercial)
- **URL:** gpspatron.com/gp-simulator-2
- **Hardware:** Ettus USRP (supported now), ADALM-Pluto and HackRF support coming
- **What it does:**
  - Professional GPS simulation with coherent and non-coherent spoofing modes
  - Real-time control over coordinates, time, PPS phase, and satellite parameters
  - Live sky operation — can synchronize with real signals for seamless takeover
  - Auto-downloads authentic almanac/ephemeris from NASA
- **Cost:** Commercial software license (contact for pricing — likely $1000+)
- **Best for:** Professional/serious testing. Overkill for hobby use but good to know about.

---

## 4. GPS JAMMING (Testing GNSS Integrity — Sprint 4)

These generate RF interference at GPS frequencies to trigger your M10's UBX-MON-RF jamming indicators (jamInd, agcCnt, jammingState).

### 4A. HackRF One as Noise Source ⭐ SIMPLEST APPROACH
- **Hardware:** HackRF One (same one used for spoofing)
- **What to do:** Transmit wideband noise or CW tone at 1575.42 MHz
- **Methods:**
  - **GNU Radio:** Noise source → HackRF sink at 1575.42 MHz
  - **hackrf_transfer:** Transmit a file of random bytes at GPS L1 frequency
  - **Portapack Mayhem Jammer app:** Set frequency range to 1575–1576 MHz, choose random/CW/sweep mode
- **Effect on M10:** jamInd rises toward 255, agcCnt drops, jammingState goes to 2 (warning) or 3 (critical), satellites lost
- **Range:** Very limited — a few meters with HackRF's ~10 dBm output (but that's all you need for bench testing)

### 4B. HackRF + Portapack Mayhem Jammer App ⭐ NO LAPTOP
- **URL:** github.com/portapack-mayhem/mayhem-firmware/wiki/Jammer
- **What it does:**
  - Three configurable frequency ranges, each up to 24 MHz wide
  - Jamming signal types: Random CW, SW sweep, FM tone, Random FSK
  - Configurable hop speed, sleep time, jitter
  - Typical output: 5–10 dBm
- **How to configure for GPS:**
  - Range 1: Start 1574.42 MHz, End 1576.42 MHz (2 MHz centered on L1)
  - Type: Random CW or SW sweep
  - This will disrupt GPS reception within a few meters
- **Best for:** Quick jamming tests without a laptop

### 4C. GPSPATRON GP-Jammer (Commercial)
- **URL:** gpspatron.com/gp-jammer
- **Hardware:** ADALM-Pluto SDRs (~$150 each, one per channel)
- **What it does:**
  - Multi-channel RF interference simulator covering 70 MHz–6 GHz
  - Open-source Python library for custom jamming waveforms
  - Supports wideband noise, narrowband CW, chirp, sweep, multi-tone, burst patterns
  - Multiple channels can jam different GNSS bands simultaneously (L1, L2, L5)
  - Custom firmware pre-loads IQ into Pluto memory for continuous loop (no CPU load)
- **Cost:** Software + Pluto SDRs (~$250 per channel)
- **Best for:** Professional multi-band jamming simulation. The Python library is open-source and useful even without buying their full solution.

### 4D. Simple Approach: Attenuated Connection
- **Hardware:** SMA cable + 30–40 dB attenuator + SMA tee
- **What to do:** Instead of radiating, connect the HackRF output directly to your GPS module's antenna input through heavy attenuation. Mix in real GPS signals via the tee.
- **Why:** Completely legal (no radiation), perfectly repeatable, adjustable signal levels
- **Cost:** ~$20 in SMA adapters and attenuators
- **Best for:** Lab-safe testing that won't violate any regulations

---

## 5. COMBINED / MULTI-PURPOSE TOOLS

### 5A. HackRF One + Portapack H2 (Mayhem Firmware) ⭐ BEST OVERALL VALUE
- **Cost:** ~$150 for Chinese clone combo, ~$350 for official
- **Covers:** GPS spoofing (GPS Sim app), GPS jamming (Jammer app), signal generation at any frequency 1 MHz–6 GHz, spectrum analysis, signal replay
- **URL:** github.com/portapack-mayhem/mayhem-firmware
- **Why it's the one to buy:** Single device covers GPS spoofing, GPS jamming, sub-GHz signal generation for testing your LoRa scanner, and general RF experimentation. Portable, standalone, no laptop needed.

### 5B. ADALM-Pluto SDR
- **Cost:** ~$150 (official from Analog Devices), ~$100 for clones ("Fishball Pluto+")
- **Range:** 325 MHz–3.8 GHz (hackable to 70 MHz–6 GHz)
- **TX/RX:** Full duplex
- **12-bit ADC** (vs HackRF's 8-bit) — better signal quality
- **Works with:** gps-sdr-sim, GP-Jammer, GNU Radio
- **Best for:** Higher-quality GPS spoofing signals than HackRF (12-bit vs 8-bit)

---

## 6. RECOMMENDED TEST KIT BUILD

### Budget Kit (~$160)
| Item | Purpose | Cost |
|------|---------|------|
| ESP8266 NodeMCU | Remote ID spoofer (jjshoots) | ~$5 |
| HackRF + Portapack clone | GPS spoof + jam + sub-GHz signals | ~$150 |
| SMA attenuators (30 dB) | Safe conducted GPS testing | ~$10 |
| **Total** | | **~$165** |

### Full Kit (~$350)
| Item | Purpose | Cost |
|------|---------|------|
| Seeed XIAO ESP32-S3 | Advanced Remote ID spoofer (colonelpanichacks swarm mode) | ~$8 |
| ESP8266 NodeMCU | Simple Remote ID spoofer (jjshoots, 16 drones) | ~$5 |
| HackRF + Portapack clone | GPS spoof + jam + sub-GHz signals | ~$150 |
| ADALM-Pluto SDR | Higher-quality GPS spoofing, GP-Jammer channel | ~$150 |
| SMA cables, tees, attenuators | Conducted testing setup | ~$30 |
| RF shielding bag/Faraday pouch | Legal over-the-air testing | ~$15 |
| **Total** | | **~$358** |

---

## 7. LEGAL WARNINGS

**GPS spoofing/jamming:** Transmitting fake GPS signals or jamming real GPS is a federal crime in the US (47 USC § 333) and most countries. **Always test inside a Faraday cage, shielded enclosure, or via conducted (cabled) connections with sufficient attenuation.** Even a few milliwatts at 1575 MHz can disrupt GPS receivers hundreds of meters away because real GPS signals are incredibly weak (-130 dBm at ground level).

**Remote ID spoofing:** Broadcasting fake Remote ID in public airspace is likely illegal (similar to ADS-B spoofing). Test indoors or in RF-shielded environments only.

**Sub-GHz transmission:** Transmitting on 868/915 MHz ISM band is legal at low power levels in most jurisdictions under ISM band regulations, but generating signals designed to interfere with others' communications may violate FCC Part 15 rules.

**Bottom line:** All of these tools are legitimate for bench testing, security research, and developing detection systems. Just keep your RF contained — use cables, attenuators, and shielding rather than antennas pointed at the sky.
