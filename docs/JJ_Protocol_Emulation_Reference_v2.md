# JUH-MAK-IN JAMMER: Sub-GHz Drone Protocol Emulation Reference v2

## Comprehensive RF Protocol Specifications for Professional Detection System Testing

**Document:** JJ-PROTOCOL-REF-v2.0  
**Date:** April 5, 2026  
**Author:** ND (Seaforged) with Claude (Anthropic) — Research & Architecture  
**Supersedes:** JJ-PROTOCOL-REF-v1.0  
**Purpose:** Authoritative reference for implementing accurate drone RF protocol simulations in JJ, sourced from firmware codebases, regulatory standards, and peer-reviewed research  
**Status:** DRAFT — Items marked [VERIFY] require primary source confirmation before implementation

---

## 1. Document Purpose and Scope

The JUH-MAK-IN JAMMER (JJ) is a professional test instrument for validating passive drone RF detection systems. JJ must accurately simulate the RF signatures of real-world drone control protocols — including their temporal behavior, modulation characteristics, and state transitions — to provide meaningful, operationally relevant detection testing.

**This document is not a toy specification.** SENTRY-RF provides situational awareness to operators whose safety depends on reliable detection. Every parameter in this document must be traceable to a primary source: open-source firmware, regulatory filings, manufacturer documentation, or peer-reviewed research. Parameters that cannot be traced are marked [VERIFY] and must not be implemented until confirmed.

### 1.1 SX1262 Hardware Constraints

All JJ simulations are constrained by the Semtech SX1262 transceiver on the LilyGo T3S3 V1.3 [Ref S1]:

| Constraint | Value | Impact |
|---|---|---|
| Frequency range | 150–960 MHz | Covers all sub-GHz ISM bands; cannot simulate 2.4/5.8 GHz |
| LoRa bandwidths | 7.8–500 kHz | Full coverage of all LoRa drone protocols |
| LoRa spreading factors | SF5–SF12 | Full coverage of all LoRa drone modes |
| FSK bitrates | 0.6–300 kbps | Covers Crossfire (85.1 kbps) and SiK (up to 250 kbps) |
| Maximum TX power | +22 dBm (160 mW) | Below ELRS/Crossfire max (1–2 W) but sufficient for bench/close-range testing |
| Frequency resolution | 1 Hz (61 Hz via FREQ register) | Sufficient for all channel plans |
| Single radio | 1 TX at a time | Cannot simulate simultaneous uplink + downlink |

---

## 2. Regulatory Framework

### 2.1 FCC Part 15.247 — United States (902–928 MHz)

**Source:** 47 CFR § 15.247 [Ref R1]

FCC Part 15.247 defines a **tiered power structure** for spread spectrum and digitally modulated systems in 902–928 MHz:

**For FHSS systems:**

| Channels | Max Output Power | Max EIRP (with 6 dBi antenna) | Dwell Time Limit |
|---|---|---|---|
| ≥ 50 | 1 W (+30 dBm) | +36 dBm EIRP | ≤ 0.4s per channel per hop period |
| 25–49 | Reduced: 1W × (25/N_channels) | Proportional | ≤ 0.4s per channel per hop period |
| < 25 | Does not qualify as FHSS | — | — |

**Dwell time rule (exact):** The average time of occupancy on any channel shall not be greater than 0.4 seconds within a period of 0.4 seconds multiplied by the number of hopping channels employed [§15.247(a)(1)(i)].

**For digitally modulated systems (non-FHSS, ≥ 500 kHz 6-dB BW):**

| Requirement | Value |
|---|---|
| Max output power | 1 W (+30 dBm) |
| Min 6-dB bandwidth | 500 kHz |
| Max PSD | 8 dBm in any 3 kHz band |

**ELRS FCC915 (40 channels, BW500 LoRa):** Qualifies as digitally modulated (500 kHz BW) under §15.247(a)(2), NOT as FHSS under §15.247(a)(1). This allows 40 channels (below the 50-channel FHSS minimum) with 1 W power. [Ref R1, P1]

**PSD compliance:** At 1W conducted power into BW500 LoRa, the PSD is approximately 10 × log10(1000 mW / 500 kHz × 3 kHz) = 10 × log10(6) ≈ 7.8 dBm per 3 kHz — below the 8 dBm limit. JJ at +22 dBm (160 mW) is well within compliance.

### 2.2 ETSI EN 300 220 — Europe (863–870 MHz)

**Source:** ETSI EN 300 220-1 **V3.1.1** (2017-02) [Ref R2] — *Note: v1.0 of this document incorrectly referenced the outdated V2.4.1*

| Sub-band | Frequency (MHz) | Max ERP | Duty Cycle | Notes |
|---|---|---|---|---|
| h1.4 | 863.0–865.0 | 25 mW | 0.1% | |
| h1.5 | 865.0–868.0 | 25 mW | 1% | Main LoRaWAN/ELRS band |
| h1.6 | 868.0–868.6 | 25 mW | 1% | LoRaWAN default channels |
| h1.7 | 868.7–869.2 | 25 mW | 0.1% | |
| h1.9 | 869.4–869.65 | 500 mW | 10% | LoRaWAN RX2 downlink |

**FHSS exemption from duty cycle:** Systems using FHSS with ≥ 47 channels at ≥ 100 kHz BW and LBT (Listen Before Talk) have no duty cycle limit [Ref R2, §4.2.6]. ELRS EU868 with only 13 channels does **NOT** qualify for this exemption — it must comply with duty cycle limits.

**EU duty cycle impact on ELRS:** At 1% duty cycle, a 200 Hz ELRS system (ToA ~4 ms per packet, 5 ms interval) has a 80% duty cycle per channel during dwell — this exceeds the 1% sub-band limit. ELRS EU868 in practice must reduce its effective transmission duty by hopping across channels and pausing. This produces a fundamentally different temporal pattern than FCC915 ELRS. **JJ must enforce duty cycle compliance when simulating EU868 modes.**

### 2.3 Other Regional Regulations

| Region | Band (MHz) | Regulation | Max Power | Key Constraint | Source |
|---|---|---|---|---|---|
| **Australia** | **915–928** (not 902–928) | ACMA AS/NZS 4268 | 1 W EIRP | Narrower than FCC: 13 MHz vs 26 MHz | [Ref R3] |
| India | 865–867 | WPC NFAP | 1 mW–1 W (license dependent) | Only 2 MHz BW, 4 ELRS channels | [Ref R4] |
| Japan | 920–928 | ARIB STD-T108 | 20 mW | LBT (carrier sense) mandatory | [Ref R5] |
| EU 433 | 433.05–434.79 | ETSI EN 300 220 | 10 mW ERP | 10% duty cycle | [Ref R2] |

---

## 3. Protocol Specifications

### 3.1 ExpressLRS (ELRS) — Open Source

**Primary source:** `ExpressLRS/ExpressLRS` GitHub repository, GPL-3.0 [Ref P1]  
**Key source files:** `src/lib/FHSS/FHSS.cpp` (channel tables), `src/src/rx_main.cpp` (hop intervals), `src/lib/OPTIONS/options.cpp` (air rate config)

#### 3.1.1 Channel Plans

**Source:** `FHSS.cpp` lines 17–25 — verbatim from firmware [Ref P1]:

| Domain | Start (MHz) | End (MHz) | Channels | Sync Ch | Regulation |
|---|---|---|---|---|---|
| **FCC915** | 903.500 | 926.900 | 40 | 20 | FCC §15.247 |
| **AU915** | 915.500 | 926.900 | 20 | 10 | ACMA |
| **EU868** | 863.275 | 869.575 | 13 | 6 | ETSI EN 300 220 |
| **IN866** | 865.375 | 866.950 | 4 | 2 | WPC India |
| **AU433** | 433.420 | 434.420 | 3 | 1 | ACMA |
| **EU433** | 433.100 | 434.450 | 3 | 1 | ETSI EN 300 220 |
| **US433** | 433.250 | 438.000 | 8 | 4 | FCC Part 15 |
| **US433W** | 423.500 | 438.000 | 20 | 10 | FCC Part 15 |
| **ISM2G4** | 2400.400 | 2479.400 | 80 | 40 | FCC/ETSI (2.4 GHz) |

**Channel frequency formula (from FHSS.cpp):**
```
freq_spread = (freq_stop - freq_start) * FREQ_SPREAD_SCALE / (freq_count - 1)
channel_freq = freq_start + (channel_index * freq_spread / FREQ_SPREAD_SCALE)
```
Note: The `(freq_count - 1)` divisor means the first and last channels are exactly at freq_start and freq_stop. This differs from the v1 document which incorrectly used `freq_count` as the divisor.

#### 3.1.2 Air Rate Modes (900 MHz)

**Source:** ELRS Lua documentation [Ref P3], Betaflight `expresslrs_common.c` [Ref P2]

| Mode | Packet Rate (Hz) | SF | BW (kHz) | CR | Preamble (symbols) | Payload (bytes) | Sensitivity (dBm) |
|---|---|---|---|---|---|---|---|
| 200 Hz | 200 | SF6 | 500 | 4/7 | 6 | 8 | -112 |
| 100 Hz | 100 | SF7 | 500 | 4/7 | 6 | 10 | -115 |
| 50 Hz | 50 | SF8 | 500 | 4/7 | 6 | 10 | -118 |
| 25 Hz | 25 | SF9 | 500 | 4/7 | 6 | 10 | -120 |
| D250 | 250 | SF6 | 500 | 4/7 | 6 | 8 | -112 |
| D500 | 500 | SF5 | 500 | 4/7 | 6 | 8 | -105 |

**DVDA (Déjà Vu Diversity Aid) modes** use `FHSShopInterval = 2` instead of 4 [Ref P19]. D250 sends 4 packets over 3 frequencies; D500 sends 2 packets over 2 frequencies.

**Preamble length:** ELRS uses **6 symbols** (configurable but 6 is standard) [Ref P1]. This is shorter than LoRaWAN's mandatory 8 symbols [Ref R6] and Meshtastic's default 16 symbols. This preamble length difference is a potential discriminator for SENTRY-RF — see Section 6.2.

**LoRa sync word:** ELRS uses sync word **0x12** (private LoRa network). LoRaWAN uses **0x34** (public LoRa network). The SX1262 can be configured to filter CAD by sync word. [Ref S1, §6.1.6]

#### 3.1.3 Hop Intervals by Domain

**Source:** `rx_main.cpp` comment: "With a interval of 4 this works out to: 2.4=4, FCC915=4, AU915=8, EU868=8, EU/AU433=36" [Ref P1]

**Clarification from Review #2:** The hop interval is defined per **air rate mode** in `ExpressLRS_AirRateConfig`, not per domain. The values below are the defaults for standard LoRa modes. DVDA modes use hop interval 2.

| Domain | Standard Hop Interval | DVDA Hop Interval | Notes |
|---|---|---|---|
| FCC915 | 4 packets | 2 packets | |
| AU915 | 8 packets | 2 packets | |
| EU868 | 8 packets | 2 packets | Duty cycle applies |
| IN866 | 8 packets | 2 packets | [VERIFY — assumed same as EU868] |
| AU/EU/US 433 | 36 packets | 2 packets | Long dwell due to few channels |

#### 3.1.4 Time-on-Air Calculations

**Source:** Semtech AN1200.13 "LoRa Modem Designer's Guide" [Ref S2], verified against `lorawan_toa` calculator [Ref T1]

**LoRa ToA formula:**
```
T_symbol = 2^SF / BW
T_preamble = (N_preamble + 4.25) × T_symbol
N_payload_symbols = 8 + max(ceil((8×PL - 4×SF + 28 + 16) / (4×(SF - 2×DE))) × (CR+4), 0)
T_payload = N_payload_symbols × T_symbol
T_packet = T_preamble + T_payload
```
Where DE = 1 for SF11/SF12 at BW125 (low data rate optimization), 0 otherwise.

**Reference note:** the historical table below is stale for current ELRS 2.4 GHz upstream. Current ExpressLRS 2.4 GHz LoRa rates use BW800/812.5 with:
- 500 Hz: SF5, CR 4/6, preamble 12, OTA4 payload length 8
- 250 Hz: SF6, CR 4/8, preamble 14, OTA4 payload length 8
- 150 Hz: SF7, CR 4/8, preamble 12, OTA4 payload length 8
- 50 Hz: SF8, CR 4/8, preamble 12, OTA4 payload length 8

Use upstream `ExpressLRS/src/src/common.cpp` as the air-rate ground truth. The table below should not be used for new ELRS 2.4 work until it is recomputed from the upstream parameters.

**Historical ToA table (retain for context only):**

| Mode | SF | Payload | T_symbol (µs) | T_preamble (ms) | T_packet (ms) | Packet Interval (ms) | Dead Time (ms) |
|---|---|---|---|---|---|---|---|
| 200 Hz | SF6 | 8 bytes | 128 | 1.31 | ~4.1 | 5.0 | 0.9 |
| 100 Hz | SF7 | 10 bytes | 256 | 2.62 | ~6.7 | 10.0 | 3.3 |
| 50 Hz | SF8 | 10 bytes | 512 | 5.25 | ~11.3 | 20.0 | 8.7 |
| 25 Hz | SF9 | 10 bytes | 1024 | 10.50 | ~19.5 | 40.0 | 20.5 |

**Why ToA matters for detection:** CAD requires observing 2–3 complete chirp symbols to detect a LoRa preamble [Ref S3]. At SF6/BW500, each symbol is only 128 µs — CAD completes in ~260–390 µs. At SF9/BW500, each symbol is 1.024 ms — CAD takes ~2–3 ms. Longer ToA = longer dwell on each frequency = higher probability that SENTRY-RF's scanner catches the signal.

#### 3.1.5 Dwell Time Per Frequency

The dwell time — how long the transmitter stays on one frequency before hopping — is the critical parameter for scan probability:

| Domain | Mode | Hop Interval | Packet Interval | Dwell Time | Hops/Second |
|---|---|---|---|---|---|
| FCC915 | 200 Hz | 4 | 5.0 ms | **20 ms** | 50 |
| FCC915 | 100 Hz | 4 | 10.0 ms | **40 ms** | 25 |
| FCC915 | 50 Hz | 4 | 20.0 ms | **80 ms** | 12.5 |
| FCC915 | 25 Hz | 4 | 40.0 ms | **160 ms** | 6.25 |
| AU915 | 200 Hz | 8 | 5.0 ms | **40 ms** | 25 |
| EU868 | 200 Hz | 8 | 5.0 ms | **40 ms** | 25 |
| EU868 | 25 Hz | 8 | 40.0 ms | **320 ms** | 3.13 |

#### 3.1.6 FHSS Sequence Generation

**Source:** `FHSS.cpp` function `FHSSrandomiseFHSSsequenceBuild()` [Ref P1], confirmed by NCC Group [Ref P4] and GNU Radio implementation [Ref P5]

1. Binding phrase → MD5 hash → first 6 bytes = UID
2. Last 4 bytes of UID → seed for 16-bit LCG: multiplier **0x343FD**, increment **0x269EC3** [Ref P4, P5]
3. LCG seeds a Fisher-Yates shuffle on channel index array
4. Sync channel inserted at position 0: `sequence[i] = syncChannel when (i % freq_count == 0)`
5. Sequence length = `(256 / freq_count) * freq_count` (rounds down to multiple of freq_count, prevents overflow — see PR #1706 [Ref P20])

#### 3.1.7 Connection States (Detection-Relevant Behavior)

**Source:** ELRS firmware `tx_main.cpp`, `rx_main.cpp` [Ref P1]

| State | RF Behavior | Detection Signature |
|---|---|---|
| **Binding/Beacon** | Fixed-frequency transmission on sync channel, repeating | Single-frequency LoRa, looks like infrastructure |
| **Scanning (TX no RX)** | FHSS hopping, beacon packets interspersed | Full FHSS pattern, detectable |
| **Connected, disarmed** | Normal FHSS, telemetry ratio active | Full FHSS + periodic telemetry slots |
| **Connected, armed** | Normal FHSS, full rate | Maximum FHSS activity |
| **Failsafe (RX lost)** | TX continues FHSS, may slow rate for re-acquisition | FHSS continues, potentially at reduced rate |
| **WiFi update mode** | 2.4 GHz WiFi emissions, no sub-GHz | Not detectable on sub-GHz |

**JJ should simulate the Binding/Beacon state** — a pilot powering on their transmitter before takeoff produces a distinctive fixed-frequency LoRa pattern that transitions to FHSS once the receiver connects. This is a realistic detection scenario.

---

### 3.2 TBS Crossfire — Closed Source, Dual-Modulation

**Source:** TBS product documentation [Ref P6], TBS support articles [Ref P7], Oscar Liang guides [Ref P8]

#### 3.2.1 Channel Plans

| Domain | Start (MHz) | End (MHz) | Channels (est.) | Spacing (kHz) | Source |
|---|---|---|---|---|---|
| **868** (EU) | **863** (not 850) | 870 | ~27 | ~260 | [Ref P7], corrected per ETSI limits |
| **915** (US) | 902 | 928 | ~100 | ~260 | [Ref P7] |

**Correction from v1:** The TBS FAQ states "850–870 MHz" for the 868 band, but operating below 863 MHz would violate ETSI EN 300 220. The actual regulatory-compliant range is 863–870 MHz. JJ must NOT simulate transmission below 863 MHz in EU Crossfire mode.

#### 3.2.2 Dual-Modulation Architecture

**Critical finding from Review #2:** Crossfire uses a **dual-modulation architecture** where the uplink and downlink use different modulations:

| Direction | Modulation | Rate | Detection by SENTRY-RF |
|---|---|---|---|
| **Uplink** (TX → drone) | GFSK, 85.1 kbps | 150 Hz | FSK Phase 3 (preamble detection) |
| **Downlink** (drone → TX) | LoRa | Lower rate (TDM slotted) | CAD (LoRa chirp detection) |
| **50 Hz mode** | LoRa (both directions) | 50 Hz | CAD |

This means a real Crossfire link in the air produces **both** FSK and LoRa energy on the same frequencies, alternating in time. SENTRY-RF needs both CAD and FSK Phase 3 to fully detect Crossfire. JJ should simulate both the FSK uplink and (optionally) the LoRa downlink to test this dual-detection path.

#### 3.2.3 Hopping Behavior

Crossfire uses **"self-healing" adaptive frequency hopping** [Ref P6]. Unlike ELRS's deterministic pseudo-random sequence, Crossfire's hopping is non-deterministic and environment-dependent — it avoids channels with detected interference. JJ's simulation of Crossfire hopping as a simple pseudo-random sequence is an approximation, but sufficient for detection testing since SENTRY-RF doesn't decode the hopping sequence.

| Parameter | 150 Hz Mode | 50 Hz Mode | Source |
|---|---|---|---|
| Packet rate | 150 Hz | 50 Hz | [Ref P6] |
| Hop rate | ~150 hops/s (every packet) | ~50 hops/s | [Ref P6] |
| Packet interval | 6.667 ms | 20 ms | Calculated |
| FSK bitrate | 85.1 kbps | N/A (LoRa) | [Ref P6] |
| FSK deviation | ~50 kHz | N/A | [VERIFY — estimated from BT product] |
| TX power range | 25 mW – 2 W | 25 mW – 2 W | [Ref P6] |

---

### 3.3 SiK Radio (RFD900/3DR Radio) — Open Source

**Primary source:** ArduPilot SiK firmware documentation [Ref P9], ThunderFly-aerospace SiK fork [Ref P10]  
**License:** BSD

SiK radios are the standard MAVLink telemetry link for ArduPilot and PX4 drones. They are extremely common in military, commercial, and hobbyist drone operations. **JJ must simulate SiK because many real-world drone detection scenarios involve SiK telemetry links.**

#### 3.3.1 Default Parameters (US 915 MHz)

**Source:** ArduPilot documentation, MissionPlanner `Sikradio.cs` [Ref P9, P11]

| Parameter (AT register) | Default Value | Description |
|---|---|---|
| S1: SERIAL_SPEED | 57 (57600 baud) | Serial port baud rate |
| S2: AIR_SPEED | 64 (64 kbps) | Over-the-air data rate |
| S3: NETID | 25 | Network ID (seeds hop sequence) |
| S4: TXPOWER | 20 (20 dBm = 100 mW) | Transmit power |
| S8: MIN_FREQ | 915000 (915.000 MHz) | Minimum frequency (kHz) |
| S9: MAX_FREQ | 928000 (928.000 MHz) | Maximum frequency (kHz) |
| S10: NUM_CHANNELS | **50** | Number of hopping channels |
| S11: DUTY_CYCLE | 100 (100%) | Duty cycle percentage |
| S12: LBT_RSSI | 0 (disabled) | Listen-before-talk threshold |

#### 3.3.2 FHSS Implementation

**Source:** ArduPilot SiK documentation [Ref P9], ThunderFly SiK repository [Ref P10]

**Channel calculation:**
```
channel_width = (MAX_FREQ - MIN_FREQ) / (NUM_CHANNELS + 2)
guard_delta = channel_width / 2
channel[n] = MIN_FREQ + guard_delta + (n * channel_width) + NETID_skew
```

The `+2` in the divisor reserves guard channels at band edges. NETID-based frequency skew provides up to one channel width of offset between networks using different NETIDs.

| Parameter | US 915 MHz | EU 868 MHz | Source |
|---|---|---|---|
| Band | 915.000–928.000 MHz | 868.000–869.000 MHz | [Ref P9] |
| Default channels | 50 | [VERIFY] | [Ref P9] |
| Channel width | (928000-915000)/(50+2) = 250 kHz | ~19 kHz | Calculated |
| Modulation | GFSK | GFSK | [Ref P9, P10] |
| Air data rates | 4, 64, 125, 250, 500, 750 kbps | Same | [Ref P9] |
| TDM | Synchronous adaptive TDM | Same | [Ref P9] |
| Max dwell time | Per FCC: 0.4s | Per ETSI duty cycle | [Ref R1] |

**TDM hopping behavior:** Unlike ELRS (pure FHSS), SiK uses **synchronous TDM + FHSS**. Both radios hop in lock-step, alternating transmit and receive windows on each frequency. The TX window is sized to fit ~3 MAVLink packets. After the TX window, both radios hop to the next frequency. Clock sync is maintained via 13-bit timestamps (16 µs resolution) embedded in each packet. [Ref P10]

**Detection characteristics:** SiK GFSK at 64–250 kbps produces a wider spectral signature than ELRS LoRa. SENTRY-RF's RSSI sweep can detect SiK energy, but CAD will NOT trigger (SiK uses GFSK, not LoRa). FSK Phase 3 should detect SiK if the preamble parameters are close to what SENTRY-RF is configured to detect.

---

### 3.4 mLRS — Open Source

**Primary source:** `olliw42/mLRS` GitHub repository, GPL-3.0 [Ref P12]  
**Documentation:** ArduPilot mLRS page [Ref P13], DeepWiki analysis [Ref P14]

#### 3.4.1 Operating Modes

| Mode | Rate (Hz) | Modulation | Chipset | Bands | Source |
|---|---|---|---|---|---|
| 19 Hz | 19 | LoRa | SX1276/SX1262 | 868/915/433 MHz | [Ref P12, P14] |
| 19 Hz 7x | 19 | LoRa SF7 | SX1276 only | 868/915 MHz | [Ref P12] |
| 31 Hz | 31 | LoRa | SX1262 only | 868/915/433 MHz | [Ref P12, P14] |
| 50 Hz | 50 | FSK | SX1262 only | 868/915 MHz | [Ref P12, P14] |
| 111 Hz | 111 | FLRC | SX1280 only | 2.4 GHz | Not sub-GHz |

**Chipset incompatibility:** SX1262 and SX1276 hardware cannot communicate with each other on 868/915 MHz, even though both operate in the same band. This is due to different LoRa implementations in the silicon. [Ref P12]

**mLRS FHSS:** Uses LoRa FHSS with "configurable shaping." Binding uses a text-based bind phrase (similar to ELRS). Each frame slot hops to a new frequency. The system is symmetric — TX and RX alternate frame slots. [Ref P12]

**JJ simulation priority:** mLRS 19 Hz LoRa mode represents the **slowest FHSS pattern** SENTRY-RF needs to detect. At 19 Hz with single-frame hopping, the hop rate is approximately 9.5 hops/second (since TX and RX alternate). This tests the lower bound of the sustained-diversity persistence gate.

**[VERIFY] items for mLRS (require cloning repo and reading `fhss.h`):**
- Exact channel frequencies for each band
- Number of channels per band
- SF/BW combinations for each mode
- Hop interval (per frame or per N frames)

---

### 3.5 FrSky R9 ACCESS — Partially Proprietary

**Source:** FrSky product documentation [Ref P15], community observations

| Parameter | Value | Confidence | Source |
|---|---|---|---|
| Frequency band (EU) | 868 MHz | Confirmed | FrSky documentation |
| Frequency band (US) | 915 MHz | Confirmed | FrSky documentation |
| Modulation | LoRa FHSS | Confirmed | FrSky ACCESS protocol description |
| Channels | ~20 | Estimated | [VERIFY — observed FHSS behavior] |
| Packet rate | ~50 Hz | Estimated | FrSky documentation mentions "50 Hz" |
| TX power | Up to 1 W (FCC) | Confirmed | FrSky product specs |

**Note:** FrSky R9 hardware (R9M, R9MX, R9MM) can also run mLRS firmware, making these devices relevant to both protocols.

---

### 3.6 Custom LoRa Direct (Non-Protocol Drones)

**Source:** Counter-UAS industry analysis [Ref P16]

Many custom-built drones use point-to-point LoRa links with RadioLib or Arduino-LoRa libraries for command and control, without any standardized FHSS protocol. As noted in the Drone Warfare C-UAS analysis: "A $30 LoRa radio module paired with an open-source autopilot creates a drone that may be invisible to a $500,000 detection system because the signature simply does not exist in any library." [Ref P16]

These drones may use:
- Fixed single-frequency LoRa (no hopping)
- Simple two-channel alternating hopping
- Custom FHSS with arbitrary channel plans
- Any SF/BW combination supported by SX1262

**JJ simulation:** A "Custom LoRa" mode with user-configurable SF, BW, frequency, hop pattern, and packet rate. This tests SENTRY-RF's ability to detect non-standard drone links that don't match any known protocol signature.

---

## 4. Non-Drone LoRa Infrastructure (False Positive Sources)

### 4.1 LoRaWAN US915

**Source:** LoRa Alliance RP002-1.0.2 [Ref R6], TTN frequency plans [Ref R7]

| Channel Set | Frequencies | Count | Spacing | Usage |
|---|---|---|---|---|
| Uplink (125 kHz) | 902.3–914.9 MHz | 64 | 200 kHz | Node → Gateway |
| Uplink (500 kHz) | 903.0–914.2 MHz | 8 | 1.6 MHz | DR4 mode |
| Downlink (500 kHz) | 923.3–927.5 MHz | 8 | 600 kHz | Gateway → Node |
| Sub-Band 2 (TTN/Helium default) | 903.9, 904.1, 904.3, 904.5, 904.7, 904.9, 905.1, 905.3 MHz | 8 | 200 kHz | Most common |

**Dwell time constraint:** FCC does not impose a formal duty cycle, but the 400 ms maximum dwell time per channel within a 20-second period creates an effective ~2% duty cycle per channel. [Ref R1, R6]

**Transmission behavior by class:**

| Class | Uplink Pattern | Downlink Pattern | False Positive Risk |
|---|---|---|---|
| **A** (most common) | ALOHA, 1 TX per minutes–hours | RX1 (TX+1s, same freq), RX2 (TX+2s, 923.3 MHz) | Low — sporadic |
| **B** | Same as A + ping slots | Beacon every 128s (GPS-synchronized, rotating frequency) | Medium — periodic beacons |
| **C** (always listening) | Same as A | Continuous RX on downlink frequency; gateway sends anytime | Medium — frequent downlinks from gateway |

**Class B beacon hopping (US915):** The beacon frequency follows an algorithm defined in RP002-1.0.2 §14.2, using the Time field of the preceding beacon and DevAddr to calculate the channel. This produces a slow, predictable frequency rotation that JJ should simulate.

### 4.2 LoRaWAN EU868

**Source:** LoRa Alliance RP002-1.0.2 [Ref R6], ETSI EN 300 220 [Ref R2]

| Parameter | Value |
|---|---|
| Mandatory channels | 868.1, 868.3, 868.5 MHz |
| Additional channels | Up to 5 operator-defined |
| Spreading factors | SF7–SF12 at BW125, SF7 at BW250 |
| Duty cycle | 0.1–1% per sub-band |
| Downlink RX2 | 869.525 MHz, SF9/BW125 |

**ADR (Adaptive Data Rate):** Nodes using ADR change SF based on network commands. The same node may appear as SF7 in one transmission and SF10 in the next. This SF variation is slow (changes per downlink command) — different from ELRS which uses a fixed SF per air rate mode. This could serve as a discriminator.

### 4.3 Meshtastic

**Source:** Meshtastic radio settings documentation [Ref R8]

| Parameter | US (915 MHz) | EU (868 MHz) |
|---|---|---|
| Default channels | ~104 | [VERIFY] |
| Frequency range | 902.0–928.0 MHz | 869.4–869.65 MHz |
| Default preamble | **16 symbols** | 16 symbols |
| Spreading factor | Preset-dependent (SF7–SF12) | SF7–SF12 |
| Sync word | 0x2B (Meshtastic-specific) | 0x2B |
| TX pattern | Periodic beacons (~15 min) + message-triggered | Same |

**Mesh forwarding cascade:** When a Meshtastic node receives a message, it rebroadcasts on the same channel. In a 5+ node mesh, a single message can cause 5+ transmissions within seconds — all on the same channel. This burst pattern is different from both FHSS and traditional LoRaWAN.

### 4.4 Helium Proof-of-Coverage

Helium hotspots are LoRaWAN gateways that also transmit periodic LoRa beacons for the Proof-of-Coverage (PoC) protocol. In dense Helium deployments (common in US cities), 5–10 PoC beacons per minute from multiple hotspots on different channels can appear. **This pattern looks more like slow FHSS than traditional LoRaWAN** and is the most likely non-drone pattern to fool a sustained-diversity gate. JJ should simulate Helium PoC beacon patterns.

---

## 5. Scan Probability Model

### 5.1 The Fundamental Detection Constraint

SENTRY-RF uses a single SX1262 scanning one frequency at a time. The probability of detecting a drone depends on **timing coincidence** — whether SENTRY-RF happens to be listening on the drone's current frequency during the drone's dwell time on that frequency.

**Source:** SENTRY-RF field test results (April 1, 2026) confirmed that scan probability, not sensitivity, is the detection bottleneck — 42+ dB of sensitivity margin existed at maximum detection range. [Ref P17]

### 5.2 Per-Cycle Detection Probability

```
P_detect_per_cycle ≈ 1 - (1 - (T_dwell × N_scanner_channels_per_cycle) / (T_cycle × N_total_channels))^N_hops_per_cycle
```

Where:
- `T_dwell` = transmitter dwell time per frequency
- `N_scanner_channels_per_cycle` = channels SENTRY-RF scans per cycle (~60 for CAD sweep)
- `T_cycle` = SENTRY-RF scan cycle time (~2.5 s)
- `N_total_channels` = total scanner frequency bins
- `N_hops_per_cycle` = drone hops during one scan cycle

### 5.3 Estimated Detection Probability by Protocol/Mode

| Protocol | Mode | Hops/Cycle (2.5s) | Dwell (ms) | Est. P_detect/cycle |
|---|---|---|---|---|
| ELRS FCC915 | 200 Hz | 125 | 20 | >99% |
| ELRS FCC915 | 50 Hz | 31 | 80 | >95% |
| ELRS FCC915 | 25 Hz | 16 | 160 | >90% |
| ELRS EU868 | 200 Hz | 63 | 40 | >99% |
| ELRS EU868 | 25 Hz | 8 | 320 | >85% |
| Crossfire 915 | 150 Hz | 375 | 6.7 | >99% |
| mLRS 915 | 19 Hz | ~24 | [VERIFY] | >80% (est.) |
| SiK 915 | 50 ch FHSS | ~50 | [VERIFY] | >90% (est.) |

**Key insight:** Even the slowest protocol (ELRS EU868 25 Hz) produces sufficient hops per scan cycle for reliable detection. The scan probability problem is primarily relevant at extreme range where signal strength approaches sensitivity limits, not for discriminating protocol type.

---

## 6. Detection Discriminators

### 6.1 Temporal Pattern (Primary — implemented in SENTRY-RF AAD)

| Signal Type | Hits/Scan Cycle | Sustained Across Cycles | Diversity Velocity |
|---|---|---|---|
| Drone FHSS | 3–15 | Yes (continuous) | High (3+ new/cycle) |
| LoRaWAN | 0–2 | No (sporadic) | Low (0–1 new/cycle) |
| Meshtastic beacon | 0–1 | No (periodic, fixed channel) | Zero |
| Helium PoC | 1–3 | Partially (periodic bursts) | Low-medium |

### 6.2 Preamble Length (Potential — not yet implemented)

| Protocol | Preamble (symbols) | Source |
|---|---|---|
| ELRS | 6 | [Ref P1] |
| LoRaWAN | 8 | [Ref R6, §4.1.1] |
| Meshtastic | 16 | [Ref R8] |
| mLRS | [VERIFY] | |

The SX1262 `cadSymbolNum` parameter could be adjusted to favor detection of short-preamble signals (drone FHSS) while being less sensitive to long-preamble signals (infrastructure). However, this would reduce overall sensitivity and requires careful tradeoff analysis.

### 6.3 Sync Word (Potential — not yet implemented)

| Protocol | Sync Word | Network Type |
|---|---|---|
| ELRS | 0x12 | Private LoRa |
| LoRaWAN | 0x34 | Public LoRa |
| Meshtastic | 0x2B | Private LoRa |
| mLRS | [VERIFY] | Private LoRa |

The SX1262 can be configured to only trigger CAD on specific sync words [Ref S1, §6.1.6]. Filtering for sync word 0x12 would instantly eliminate all LoRaWAN false positives. However, this would also miss mLRS (if it uses a different sync word) and custom LoRa drones that may use 0x34 or other values. **Not recommended as a primary discriminator**, but valuable as a supplementary filter.

---

## 7. JJ Emulation Architecture

### 7.1 Protocol Parameter Header

All protocol constants must be defined in a single header file (`protocol_params.h`) with inline source references. See Section 3 for exact values. The current duplication of ELRS constants across `rf_modes.cpp`, `combined_mode.cpp`, `power_ramp.cpp`, and `false_positive.cpp` must be eliminated.

### 7.2 JJ Protocol Info Output

When JJ starts any mode, it should print a complete parameter summary:
```
[ELRS-FCC915] 40ch 903.5-926.9MHz SF6/BW500 200Hz hop_every_4 sync_ch=20
  ToA: 4.1ms/pkt  Dwell: 20ms/freq  Hops: 50/s  Preamble: 6sym  SyncWord: 0x12
  Est. SENTRY-RF P_detect/cycle: >99%  Power: 10 dBm
```

This turns JJ into a teaching tool as well as a test instrument.

### 7.3 Protocol Selection Menu

```
Signal Generator
├── Drone Protocols
│   ├── ELRS FHSS
│   │   ├── Domain: [FCC915 / AU915 / EU868 / IN866 / AU433 / EU433 / US433]
│   │   ├── Rate: [25 Hz / 50 Hz / 100 Hz / 200 Hz / D250 / D500]
│   │   ├── State: [Connected / Binding-Beacon]
│   │   └── Power: [adjustable]
│   ├── Crossfire
│   │   ├── Band: [868 / 915]
│   │   ├── Mode: [150Hz FSK / 50Hz LoRa / Dual (FSK+LoRa TDM)]
│   │   └── Power: [adjustable]
│   ├── SiK Radio (MAVLink Telemetry)
│   │   ├── Band: [915 / 868 / 433]
│   │   ├── Air Speed: [64 / 125 / 250 kbps]
│   │   ├── Channels: [20 / 50]
│   │   └── Power: [adjustable]
│   ├── mLRS
│   │   ├── Band: [868 / 915 / 433]
│   │   ├── Rate: [19 Hz / 31 Hz / 50 Hz]
│   │   └── Power: [adjustable]
│   ├── Custom LoRa Direct
│   │   ├── Frequency / SF / BW / Hop Pattern: [all configurable]
│   │   └── Power: [adjustable]
│   └── FrSky R9 ACCESS [when parameters confirmed]
├── Infrastructure (False Positive Testing)
│   ├── LoRaWAN US915 SB2
│   │   ├── Nodes: [1 / 4 / 8]
│   │   ├── Interval: [30s / 60s / 300s]
│   │   └── SF: [SF7–SF12, random per TX]
│   ├── LoRaWAN EU868
│   ├── Meshtastic Beacon (16-sym preamble, sync 0x2B)
│   ├── Helium PoC Beacons (multi-hotspot rotating channels)
│   ├── Class B Gateway Beacon (128s periodic, frequency hopping)
│   └── Dense Mixed (ELRS + LoRaWAN + Meshtastic simultaneous)
├── Special Modes
│   ├── CW Tone / Band Sweep / Power Ramp (existing)
│   ├── Drone Swarm (WiFi Remote ID, 1–16 virtual drones)
│   └── Combined Dual-Core (RF + WiFi/BLE simultaneous)
```

---

## 8. Physical Layer Considerations

### 8.1 Adjacent Channel Leakage

Real SX1262 transmissions produce spectral energy beyond the nominal channel bandwidth. LoRa SF6/BW500 has a main lobe of ~500 kHz but spectral skirts extend 200–300 kHz beyond channel edges. SENTRY-RF's frequency matching tolerance (`TAP_FREQ_TOL`) must account for this. JJ's transmissions will naturally exhibit this leakage, making JJ testing realistic.

### 8.2 Crystal Drift

Without TCXO: ±10 ppm typical crystal drift → ±9.15 kHz at 915 MHz. With TCXO (standard on JJ's T3S3 and most modern ELRS hardware): ±1–2 ppm → ±0.9–1.8 kHz. SENTRY-RF's frequency matching must accommodate worst-case drift from non-TCXO targets.

### 8.3 Intermodulation Products

Strong local transmissions can create intermodulation products in the SX1262 receiver's front end, producing phantom signals. This is a potential source of false positives unrelated to ambient infrastructure. Not a JJ simulation requirement, but noted for SENTRY-RF diagnostic purposes.

---

## 9. References

### Regulatory Standards

[Ref R1] Federal Communications Commission, "47 CFR § 15.247 — Operation within the bands 902–928 MHz, 2400–2483.5 MHz, and 5725–5850 MHz." Available: https://www.ecfr.gov/current/title-47/chapter-I/subchapter-A/part-15/subpart-C/section-15.247

[Ref R2] ETSI, "EN 300 220-1 V3.1.1 (2017-02) — Short Range Devices operating in the frequency range 25 MHz to 1000 MHz." Available: https://www.etsi.org/deliver/etsi_en/300200_300299/30022001/

[Ref R3] ACMA, "AS/NZS 4268 — Radio equipment and systems." Australian 915–928 MHz ISM band.

[Ref R4] India WPC, "National Frequency Allocation Plan (NFAP)" — 865–867 MHz allocation.

[Ref R5] ARIB, "STD-T108 — 920 MHz band telemeter, telecontrol and data transmission radio equipment." Japanese 920–928 MHz band with mandatory LBT.

[Ref R6] LoRa Alliance, "LoRaWAN Regional Parameters RP002-1.0.2," 2020. Available: https://lora-alliance.org/wp-content/uploads/2020/11/RP_2-1.0.2.pdf

[Ref R7] The Things Network, "Frequency Plans." Available: https://www.thethingsnetwork.org/docs/lorawan/frequency-plans/

[Ref R8] Meshtastic, "Radio Settings Documentation." Available: https://meshtastic.org/docs/overview/radio-settings/

### Protocol Firmware Sources

[Ref P1] ExpressLRS Contributors, "ExpressLRS Firmware," GitHub GPL-3.0. Channel tables: `src/lib/FHSS/FHSS.cpp`. Hop intervals: `src/src/rx_main.cpp`. Available: https://github.com/ExpressLRS/ExpressLRS

[Ref P2] Betaflight Contributors, "ELRS SPI Receiver," `src/main/rx/expresslrs_common.c`. Available: https://github.com/betaflight/betaflight/blob/master/src/main/rx/expresslrs_common.c

[Ref P3] ExpressLRS, "Lua Script Documentation — Packet Rates and Telemetry." Available: https://www.expresslrs.org/quick-start/transmitters/lua-howto/

[Ref P4] NCC Group, "Technical Advisory — ExpressLRS vulnerabilities," June 2022. Confirms LCG constants and FHSS sequence generation. Available: https://www.nccgroup.com/research-blog/technical-advisory-expresslrs-vulnerabilities-allow-for-hijack-of-control-link/

[Ref P5] Garcia et al., "Implementation and Analysis of ExpressLRS Under Interference Using GNU Radio," GRCon 2025, Florida Atlantic University. Confirms LCG constants 0x343FD and 0x269EC3. Available: https://events.gnuradio.org/event/26/contributions/771/

[Ref P6] Team BlackSheep, "TBS Crossfire Product Specifications." Available: https://www.team-blacksheep.com/products/prod:crossfire_tx

[Ref P7] Team BlackSheep, "Crossfire Band Range." Available: https://team-blacksheep.freshdesk.com/support/solutions/articles/4000134179-crossfire-band-range

[Ref P8] Liang, Oscar, "How to Setup TBS Crossfire," May 2023. Available: https://oscarliang.com/crossfire-betaflight/

[Ref P9] ArduPilot, "SiK Radio — Advanced Configuration." Default parameters: MIN_FREQ=915000, MAX_FREQ=928000, NUM_CHANNELS=50, GFSK modulation, FHSS+TDM. Available: https://ardupilot.org/copter/docs/common-3dr-radio-advanced-configuration-and-technical-information.html

[Ref P10] ThunderFly-aerospace, "SiK Firmware Repository." Documents FHSS channel_width=(MAX_FREQ-MIN_FREQ)/(NUM_CHANNELS+2), NETID-based frequency skew, TDM synchronization. Available: https://github.com/ThunderFly-aerospace/SiK

[Ref P11] ArduPilot, "MissionPlanner Sikradio.cs." Confirms default parameters. Available: https://github.com/ArduPilot/MissionPlanner/blob/master/Radio/Sikradio.cs

[Ref P12] olliw42, "mLRS Firmware," GitHub GPL-3.0. Available: https://github.com/olliw42/mLRS

[Ref P13] ArduPilot, "mLRS project documentation." Available: https://ardupilot.org/copter/docs/common-mlrs-rc.html

[Ref P14] DeepWiki, "olliw42/mLRS analysis." Available: https://deepwiki.com/olliw42/mLRS

[Ref P15] FrSky Electronic Co., Ltd., "R9 System Documentation." Partially proprietary.

[Ref P16] Drone Warfare, "Counter-UAS 101 — RF Drone Detection," October 2024. Available: https://drone-warfare.com/2024/10/26/counter-uas-101-radio-frequency-rf-drone-detection/

[Ref P17] Seaforged, "SENTRY-RF Field Test Results 2026-04-01." Internal document: `docs/FIELD_TEST_RESULTS_2026-04-01.md`

### Hardware References

[Ref S1] Semtech, "SX1261/SX1262 Datasheet Rev 2.2." Sections: 6.1.6 (sync word), 13.4 (CAD), 13.1.1 (frequency resolution). Available: https://www.semtech.com/products/wireless-rf/lora-transceivers/sx1262

[Ref S2] Semtech, "AN1200.13 — LoRa Modem Designer's Guide." ToA calculation formulas. Available from Semtech technical documentation.

[Ref S3] Semtech, "AN1200.48 — SX126x CAD Performance Evaluation." CAD detection parameters per SF.

### Research Papers

[Ref P18] MDPI Drones, "An Intelligent Passive System for UAV Detection in Complex EM Environments," vol. 9, no. 10, p. 702, October 2025. Available: https://www.mdpi.com/2504-446X/9/10/702

### ELRS Source Code References

[Ref P19] JyeSmith, "Déjà Vu Diversity Aid (DVDA) — PR #1527," ExpressLRS GitHub. Reduced FHSShopInterval from 8 to 2 for DVDA modes. Available: https://github.com/ExpressLRS/ExpressLRS/pull/1527

[Ref P20] pkendall64, "Fix for FHSS hopping where freq_count is a power of 2 — PR #1706," ExpressLRS GitHub. Available: https://github.com/ExpressLRS/ExpressLRS/pull/1706

### Tools

[Ref T1] tanupoo, "lorawan_toa — LoRa Time on Air Calculator." Available: https://github.com/tanupoo/lorawan_toa

---

## 10. Items Requiring Verification

| Item | Current Value | Needed Source | Priority |
|---|---|---|---|
| ELRS IN866 hop interval | Assumed 8 | ELRS `rx_main.cpp` | Low |
| Crossfire FSK deviation | 50 kHz (estimated) | SDR capture or TBS documentation | Medium |
| Crossfire 868 channel count | ~27 (estimated from 863–870 at 260 kHz) | TBS confirmation | Medium |
| mLRS channel count per band | Unknown | mLRS `fhss.h` — clone and read | High |
| mLRS SF/BW per mode | Estimated | mLRS source code | High |
| mLRS hop interval | Unknown | mLRS source code | High |
| FrSky R9 channel count | ~20 (estimated) | SDR capture | Low |
| Meshtastic EU868 channel count | Unknown | Meshtastic source | Low |
| SiK EU868 default NUM_CHANNELS | Unknown | SiK firmware source | Medium |

---

## 11. Document History

| Version | Date | Changes |
|---|---|---|
| 1.0 | April 5, 2026 | Initial protocol reference |
| 2.0 | April 5, 2026 | Incorporated corrections from FCC regulatory review and RF protocol engineering review. Added: SiK radio protocol, scan probability model, ToA calculations, connection states, dual-modulation Crossfire, preamble/sync word discriminators, Helium PoC patterns, EU duty cycle enforcement, PSD compliance analysis, DVDA hop intervals. Fixed: FCC 15.247 tiered structure, ETSI version (V3.1.1 not V2.4.1), channel spacing formula (n-1 divisor), Australian band (915-928 not 902-928), Crossfire 868 band (863-870 not 850-870). |

---

*This document is a living reference. Update as [VERIFY] items are confirmed. Place copies in both `C:\Projects\Juh-Mak-In-Jammer\docs\` and `C:\Projects\sentry-rf\Sentry-RF-main\docs\`. Review before every JJ sprint.*
