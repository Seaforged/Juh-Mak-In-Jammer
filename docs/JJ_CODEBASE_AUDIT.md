# JJ Codebase Audit — Gap Analysis Against Protocol Emulation Reference v2

**Date:** April 5, 2026  
**Auditor:** Claude (Anthropic) — read-only codebase analysis  
**Reference:** `docs/JJ_Protocol_Emulation_Reference_v2.md` (JJ-PROTOCOL-REF-v2.0)  
**Codebase:** JUH-MAK-IN JAMMER v1.1.0, commit `4ba5fa3` (post-Sprint 1)

---

## Quick Summary

| Metric | Value |
|---|---|
| **Total source files** | 9 .cpp + 11 .h = 20 |
| **Total existing modes** | 9 (CW, Sweep, ELRS, Crossfire, RID, FP LoRaWAN, FP ISM, FP Mixed, Combined, Swarm, Power Ramp) |
| **Modes matching v2 spec** | **1/9** (ELRS FCC915 is close after Sprint 1; others need work) |
| **Modes needing update** | **5** (ELRS preamble/CR/sync word/spacing, Crossfire hop rate, LoRaWAN channels, all info output) |
| **New modes needed** | **10** (SiK, mLRS, FrSky R9, Custom LoRa, ELRS EU868/AU915/binding, Meshtastic, Helium PoC, Crossfire dual-mod, ELRS multi-rate, Dense Mixed) |
| **Duplicated constants found** | **28 constants** across **4 files** |
| **[VERIFY] items blocking implementation** | **9** (mLRS ×4, Crossfire FSK deviation, Crossfire 868 ch count, FrSky R9 ch count, SiK EU868 channels, Meshtastic EU868 channels) |

---

## A. Current Mode Inventory

### A1. CW Tone (`src/rf_modes.cpp:22–98`)

| Aspect | Current | v2 Spec | Status |
|---|---|---|---|
| Purpose | Unmodulated carrier at preset frequencies | Listed under "Special Modes" (§7.3) | **CORRECT** — utility mode, not protocol-specific |
| Frequencies | 915.0, 868.0, 903.0, 925.0, 906.4 MHz | No specific requirement | OK |
| Serial command | `c` | No change needed | OK |

**Issues:** None. This is a utility mode not subject to protocol accuracy requirements.

### A2. Band Sweep (`src/rf_modes.cpp:100–224`)

| Aspect | Current | v2 Spec | Status |
|---|---|---|---|
| Purpose | Linear CW sweep 860–930 MHz | Listed under "Special Modes" (§7.3) | **CORRECT** |
| Range | 860–930 MHz | No specific requirement | OK |
| Serial command | `b` | No change needed | OK |

**Issues:** None. Utility mode.

### A3. ELRS 915 FHSS (`src/rf_modes.cpp:244–408`)

| Parameter | Current (line) | v2 Spec (§3.1) | Status |
|---|---|---|---|
| Channel count | 40 (L251) | 40 (§3.1.1 FCC915) | **CORRECT** |
| Band start | 903.5 MHz (L252) | 903.500 MHz | **CORRECT** |
| Band end | 926.9 MHz (L253) | 926.900 MHz | **CORRECT** |
| Channel spacing | `(926.9-903.5)/40` = 585 kHz (L254) | `(926.9-903.5)/(40-1)` = 600 kHz (§3.1.1 formula) | **INCORRECT** — v2 says divisor is `freq_count - 1` not `freq_count` |
| SF | SF6 (L321) | SF6 for 200 Hz mode (§3.1.2) | **CORRECT** |
| BW | 500 kHz (L320) | 500 kHz | **CORRECT** |
| CR | 4/5 = CR5 (L322) | **4/7 = CR7** (§3.1.2) | **INCORRECT** — JJ uses CR 4/5, real ELRS uses CR 4/7 |
| Preamble | 8 symbols (L325) | **6 symbols** (§3.1.2) | **INCORRECT** — JJ uses 8, real ELRS uses 6 |
| Sync word | `RADIOLIB_SX126X_SYNC_WORD_PRIVATE` (L323) | **0x12** (§3.1.2) | **NEEDS VERIFICATION** — RadioLib's PRIVATE sync word may be 0x12, but should be explicit |
| Hop interval | Every 4 packets (L256) | Every 4 packets (§3.1.3 FCC915) | **CORRECT** |
| Sync channel | Ch 20 (L257) | Ch 20 (§3.1.1 FCC915) | **CORRECT** |
| Packet rate | 200 Hz (L255) | 200 Hz (§3.1.2) | **CORRECT** |
| Payload | 8 bytes (L272) | 8 bytes for 200 Hz mode (§3.1.2) | **CORRECT** |
| LCG constants | 1664525, 1013904223 (L287) | **0x343FD, 0x269EC3** (§3.1.6) | **INCORRECT** — JJ uses Knuth LCG, real ELRS uses 0x343FD/0x269EC3 |
| Sequence length | 40 (array size) | `(256/freq_count)*freq_count` = 240 for FCC915 (§3.1.6) | **INCORRECT** — real ELRS sequence is 240 entries (6 full rotations of 40 channels), not 40 |
| Connection states | Connected only | Binding/Beacon state recommended (§3.1.7) | **MISSING** |
| Info output | 1 line (L349) | 3-line detailed output (§7.2) | **INCOMPLETE** — missing ToA, dwell, hops/s, preamble, sync word, P_detect |

### A4. Crossfire 915 FSK (`src/crossfire.cpp`)

| Parameter | Current (line) | v2 Spec (§3.2) | Status |
|---|---|---|---|
| Channel count | 100 (L11) | ~100 (§3.2.1) | **CORRECT** |
| Band start | 902.165 MHz (L12) | 902 MHz (§3.2.1) | **CLOSE** — minor offset |
| Channel spacing | 260 kHz (L13) | ~260 kHz (§3.2.1) | **CORRECT** |
| Hop rate | 50 Hz (L14: 20000 µs) | **150 Hz** (§3.2.3: 6667 µs) | **INCORRECT** — 3× too slow |
| FSK bitrate | 85.1 kbps (L71) | 85.1 kbps (§3.2.3) | **CORRECT** |
| FSK deviation | 50.0 kHz (L72) | ~50 kHz [VERIFY] (§3.2.3) | **TENTATIVELY CORRECT** |
| Modulation | FSK only | **Dual: FSK uplink + LoRa downlink** (§3.2.2) | **INCOMPLETE** — missing LoRa downlink simulation |
| 50 Hz LoRa mode | Not implemented | 50 Hz SF6/BW500 LoRa (§3.2.2) | **MISSING** |
| 868 band | Not implemented | 863–870 MHz, ~27 channels (§3.2.1) | **MISSING** |

### A5. Remote ID Spoofer (`src/rid_spoofer.cpp`)

| Aspect | Current | v2 Spec | Status |
|---|---|---|---|
| WiFi beacons | ASTM F3411 vendor IE | Not in v2 scope (2.4 GHz) | N/A |
| BLE advertising | ASTM F3411 service data | Not in v2 scope | N/A |

**Issues:** Outside v2 scope (sub-GHz only). Functioning correctly.

### A6. False Positive Generator (`src/false_positive.cpp`)

#### A6.1 LoRaWAN Mode

| Parameter | Current (line) | v2 Spec (§4.1) | Status |
|---|---|---|---|
| Frequency | Fixed 903.9 MHz (L45) | **8 SB2 channels**: 903.9, 904.1, 904.3, 904.5, 904.7, 904.9, 905.1, 905.3 (§4.1) | **INCORRECT** — single channel instead of 8-channel rotation |
| SF range | SF7–SF12 random (L54) | SF7–SF12 (§4.1) | **CORRECT** |
| BW | 125 kHz (L59) | 125 kHz for most; also needs BW500 DR4 mode | **INCOMPLETE** — missing BW500 uplink |
| Sync word | PRIVATE (L62) | Should be **0x34** (public LoRaWAN) (§6.3) | **INCORRECT** — LoRaWAN uses public sync word |
| TX interval | 30–60 seconds (L84) | Minutes to hours typical; 30–60s is acceptable for testing | OK (configurable would be better) |
| Preamble | 8 symbols (L64) | 8 symbols (§6.2) | **CORRECT** |
| Multi-node sim | Not implemented | 1/4/8 virtual nodes (§7.3) | **MISSING** |

#### A6.2 ISM Burst Mode

| Aspect | Current | v2 Spec | Status |
|---|---|---|---|
| Purpose | Random FSK bursts | Not explicitly in v2 | OK — supplementary mode |

#### A6.3 Mixed Mode

| Aspect | Current | v2 Spec (§7.3) | Status |
|---|---|---|---|
| ELRS parameters | Matches rf_modes.cpp (40ch, hop-every-4) | Per §3.1 | Same issues as A3 |
| LoRaWAN interleaving | Single channel 903.9 MHz | Should use 8 SB2 channels | **INCORRECT** |

### A7. Combined Mode (`src/combined_mode.cpp`)

| Aspect | Current | v2 Spec | Status |
|---|---|---|---|
| Architecture | RID on Core 0, ELRS on Core 1 | Listed under "Special Modes" (§7.3) | **CORRECT** architecture |
| ELRS parameters | Duplicated, matches rf_modes.cpp | Per §3.1 | Same issues as A3 + **DUPLICATED** constants |

### A8. Swarm Simulator (`src/swarm_sim.cpp`)

| Aspect | Current | v2 Spec | Status |
|---|---|---|---|
| Purpose | 1–16 virtual drones, WiFi RID beacons | Listed under "Special Modes" (§7.3) | **CORRECT** |

**Issues:** Outside v2 sub-GHz scope. Functioning correctly.

### A9. Power Ramp (`src/power_ramp.cpp`)

| Aspect | Current | v2 Spec | Status |
|---|---|---|---|
| Purpose | ELRS FHSS with ramping power | Listed under "Special Modes" (§7.3) | **CORRECT** concept |
| ELRS parameters | Duplicated, matches rf_modes.cpp | Per §3.1 | Same issues as A3 + **DUPLICATED** constants |
| Payload | 4 bytes (L45) | Should be 8 bytes per §3.1.2 | **INCORRECT** — inconsistent with other ELRS modes |

---

## B. Protocol Constants Audit

### B1. ELRS FCC915 Constants

| Constant | rf_modes.cpp | combined_mode.cpp | power_ramp.cpp | false_positive.cpp | v2 Correct Value | Status |
|---|---|---|---|---|---|---|
| NUM_CHANNELS | 40 (L251) | 40 (L28) | 40 (L11) | 40 (L148) | 40 | CORRECT × 4, **DUPLICATED** |
| BAND_START | 903.5 (L252) | 903.5 (L29) | 903.5 (L12) | 903.5 (L149) | 903.500 | CORRECT × 4, **DUPLICATED** |
| BAND_END | 926.9 (L253) | 926.9 (L30) | 926.9 (L13) | 926.9 (L150) | 926.900 | CORRECT × 4, **DUPLICATED** |
| CHAN_SPACING | /40 (L254) | /40 (L31) | /40 (L14) | /40 (L151) | **/(40-1)** per §3.1.1 | **INCORRECT** × 4, **DUPLICATED** |
| PACKET_INTERVAL | 5000 µs (L255) | 5000 µs (L32) | 5000 µs (L15) | 5000 µs (L152) | 5000 µs | CORRECT × 4, **DUPLICATED** |
| HOP_EVERY_N | 4 (L256) | 4 (L33) | 4 (L16) | 4 (L153) | 4 | CORRECT × 4, **DUPLICATED** |
| SYNC_CHANNEL | 20 (L257) | 20 (L34) | 20 (L17) | 20 (L154) | 20 | CORRECT × 4, **DUPLICATED** |
| LCG multiplier | 1664525 (L287) | 1664525 (L43) | 1664525 (L51) | 1664525 (L167) | **0x343FD** (214013) | **INCORRECT** × 4, **DUPLICATED** |
| LCG increment | 1013904223 (L287) | 1013904223 (L43) | 1013904223 (L51) | 1013904223 (L167) | **0x269EC3** (2531011) | **INCORRECT** × 4, **DUPLICATED** |
| SF | 6 (L321) | 6 (L62) | 6 (L85) | 6 (L179) | 6 | CORRECT × 4, **DUPLICATED** |
| BW | 500 (L320) | 500 (L62) | 500 (L85) | 500 (L179) | 500 | CORRECT × 4, **DUPLICATED** |
| CR | 5 (L322) | 5 (L62) | 5 (L85) | 5 (L179) | **7** (4/7) per §3.1.2 | **INCORRECT** × 4, **DUPLICATED** |
| Preamble | 8 (L325) | 8 (L64) | 8 (L88) | 8 (L181) | **6** per §3.1.2 | **INCORRECT** × 4, **DUPLICATED** |

**Total: 28 ELRS constants duplicated across 4 files. 16 of 28 are incorrect.**

### B2. Crossfire Constants (`src/crossfire.cpp`)

| Constant | Current (line) | v2 Correct | Status |
|---|---|---|---|
| CRSF_NUM_CHANNELS | 100 (L11) | ~100 | CORRECT |
| CRSF_BAND_START | 902.165 (L12) | 902 | **MINOR OFFSET** |
| CRSF_CHAN_SPACING | 0.260 MHz (L13) | ~260 kHz | CORRECT |
| CRSF_HOP_INTERVAL_US | 20000 (L14) | **6667** (150 Hz) | **INCORRECT** |
| FSK bitrate | 85.1 (L71) | 85.1 | CORRECT |
| FSK deviation | 50.0 (L72) | ~50 [VERIFY] | TENTATIVELY CORRECT |

### B3. LoRaWAN Constants (`src/false_positive.cpp`)

| Constant | Current (line) | v2 Correct | Status |
|---|---|---|---|
| LORAWAN_FREQ | 903.9 MHz (L45) | 8 SB2 frequencies | **INCORRECT** — single channel |
| BW | 125 kHz (L59) | 125 kHz (+ BW500 for DR4) | **INCOMPLETE** |
| Sync word | RADIOLIB_SX126X_SYNC_WORD_PRIVATE (L62) | **0x34** (public) | **INCORRECT** |
| Preamble | 8 (L64) | 8 | CORRECT |

### B4. Constants That Don't Exist Yet (MISSING)

| Constant | v2 Section | Required For |
|---|---|---|
| ELRS EU868 channel table (13ch, 863.275–869.575) | §3.1.1 | ELRS EU868 mode |
| ELRS AU915 channel table (20ch, 915.5–926.9) | §3.1.1 | ELRS AU915 mode |
| ELRS IN866 channel table (4ch, 865.375–866.950) | §3.1.1 | ELRS IN866 mode |
| ELRS 100/50/25 Hz mode parameters | §3.1.2 | Multi-rate ELRS |
| ELRS DVDA hop intervals (2 instead of 4) | §3.1.3 | D250/D500 modes |
| SiK channel parameters (50ch, 915–928, 250 kHz) | §3.3.1 | SiK mode |
| SiK GFSK air rates (64/125/250 kbps) | §3.3.1 | SiK mode |
| mLRS parameters (all [VERIFY]) | §3.4 | mLRS mode |
| Meshtastic sync word 0x2B, 16-sym preamble | §4.3 | Meshtastic FP mode |
| Crossfire 868 band (863–870 MHz) | §3.2.1 | Crossfire EU mode |
| LoRaWAN EU868 channels (868.1, 868.3, 868.5) | §4.2 | LoRaWAN EU868 FP mode |
| Helium PoC beacon parameters | §4.4 | Helium FP mode |

---

## C. New Protocols Needed

### C1. SiK Radio (MAVLink Telemetry) — §3.3

| Aspect | Requirement | SX1262 Feasible? |
|---|---|---|
| Modulation | GFSK, 64–250 kbps | **Yes** — SX1262 supports 0.6–300 kbps FSK |
| Band | 915–928 MHz (US), 868–869 MHz (EU) | **Yes** |
| Channels | 50 (US default) | **Yes** — simple channel table |
| Hop behavior | TDM + FHSS, synchronous | **Partial** — can simulate TX side; no true TDM without RX |
| Channel calculation | `(MAX-MIN)/(N+2)` with guard bands | Straightforward |
| [VERIFY] items | EU channel count | 1 |

**Priority: HIGH** — SiK is extremely common in ArduPilot/PX4 drones.

### C2. mLRS — §3.4

| Aspect | Requirement | SX1262 Feasible? |
|---|---|---|
| Modulation | LoRa (19/31 Hz), FSK (50 Hz) | **Yes** |
| Band | 868/915/433 MHz | **Yes** (SX1262 covers 150–960 MHz) |
| Rates | 19, 31, 50 Hz | **Yes** |
| [VERIFY] items | Channel count, SF/BW per mode, hop interval | **4 items** — blocks implementation |

**Priority: MEDIUM** — Growing in ArduPilot community. Blocked by [VERIFY] items.

### C3. FrSky R9 ACCESS — §3.5

| Aspect | Requirement | SX1262 Feasible? |
|---|---|---|
| Modulation | LoRa FHSS | **Yes** |
| Channels | ~20 (estimated) | **Yes** |
| [VERIFY] items | Channel count, exact parameters | **1 item** |

**Priority: LOW** — Largely replaced by ELRS in the community.

### C4. Custom LoRa Direct — §3.6

| Aspect | Requirement | SX1262 Feasible? |
|---|---|---|
| User-configurable SF, BW, freq, hop pattern | Menu-driven parameter selection | **Yes** |
| Fixed single-freq, 2-channel alternating, custom FHSS | Multiple sub-modes | **Yes** |

**Priority: MEDIUM** — Tests SENTRY-RF against non-library drones.

### C5. ELRS Multi-Domain (EU868, AU915, IN866, 433 MHz)

| Domain | Channels | Band | SX1262 Feasible? |
|---|---|---|---|
| EU868 | 13 | 863.275–869.575 MHz | **Yes** |
| AU915 | 20 | 915.5–926.9 MHz | **Yes** |
| IN866 | 4 | 865.375–866.950 MHz | **Yes** |
| AU433 | 3 | 433.420–434.420 MHz | **Yes** |
| EU433 | 3 | 433.100–434.450 MHz | **Yes** |

**Priority: HIGH for EU868** (different temporal pattern due to duty cycle), LOW for others.

### C6. ELRS Multi-Rate (100 Hz, 50 Hz, 25 Hz, D250, D500)

| Mode | SF | Payload | Hop Interval | SX1262 Feasible? |
|---|---|---|---|---|
| 100 Hz | SF7 | 10 bytes | 4 | **Yes** |
| 50 Hz | SF8 | 10 bytes | 4 | **Yes** |
| 25 Hz | SF9 | 10 bytes | 4 | **Yes** |
| D250 | SF6 | 8 bytes | 2 | **Yes** |
| D500 | SF5 | 8 bytes | 2 | **Yes** |

**Priority: HIGH** — Tests SENTRY-RF against all pilot types (racers vs long-range).

### C7. ELRS Binding/Beacon State — §3.1.7

Fixed-frequency transmission on sync channel. Simple to implement. Tests SENTRY-RF's ability to detect pre-flight drone activity.

**Priority: MEDIUM**

---

## D. Infrastructure Modes Needed

| Mode | v2 Section | Current Status | Priority |
|---|---|---|---|
| **LoRaWAN US915 SB2** (multi-channel) | §4.1, §7.3 | Single-channel only (903.9 MHz) | **HIGH** — must rotate 8 channels |
| **LoRaWAN US915 multi-node** (1/4/8 virtual nodes) | §7.3 | Single virtual node | **HIGH** — tests AAD ambient catalog |
| **LoRaWAN EU868** (868.1/868.3/868.5 + duty cycle) | §4.2, §7.3 | Not implemented | **MEDIUM** |
| **Meshtastic beacon** (16-sym preamble, sync 0x2B) | §4.3, §7.3 | Not implemented | **HIGH** — different preamble/sync word from ELRS |
| **Helium PoC beacons** (multi-hotspot, rotating channels) | §4.4, §7.3 | Not implemented | **MEDIUM** — most likely to fool sustained-diversity gate |
| **Class B gateway beacon** (128s periodic, freq hopping) | §4.1, §7.3 | Not implemented | **LOW** |
| **Dense mixed** (ELRS + LoRaWAN + Meshtastic TDM) | §7.3 | Current mixed mode is ELRS + LoRaWAN only | **MEDIUM** |
| **LoRaWAN BW500 DR4 uplink** | §4.1 | Not implemented | **HIGH** — only LoRaWAN mode that triggers CAD |

---

## E. Architecture Changes Needed

### E1. `protocol_params.h` — Does NOT Exist

**Required by:** v2 §7.1

All 28 ELRS constants are duplicated across 4 files. The v2 doc explicitly states: "The current duplication of ELRS constants across rf_modes.cpp, combined_mode.cpp, power_ramp.cpp, and false_positive.cpp must be eliminated."

**What must move to `protocol_params.h`:**

```
// ELRS FCC915
ELRS_FCC915_NUM_CHANNELS    40
ELRS_FCC915_FREQ_START      903.5f
ELRS_FCC915_FREQ_END        926.9f
ELRS_FCC915_SYNC_CHANNEL    20
ELRS_FCC915_HOP_INTERVAL    4
ELRS_LCG_MULTIPLIER         0x343FD
ELRS_LCG_INCREMENT          0x269EC3

// ELRS air rate configs (SF, BW, CR, preamble, payload per rate)
// Crossfire 915/868
// SiK US915/EU868
// LoRaWAN US915 SB2 channels
// LoRaWAN EU868 channels
// Meshtastic parameters
```

**Refactoring scope:** 4 source files need constants replaced with `#include "protocol_params.h"`.

### E2. Protocol Info Output — Does NOT Exist as Specified

**Required by:** v2 §7.2

v2 specifies a 3-line info output at mode start:
```
[ELRS-FCC915] 40ch 903.5-926.9MHz SF6/BW500 200Hz hop_every_4 sync_ch=20
  ToA: 4.1ms/pkt  Dwell: 20ms/freq  Hops: 50/s  Preamble: 6sym  SyncWord: 0x12
  Est. SENTRY-RF P_detect/cycle: >99%  Power: 10 dBm
```

**Current output** (`rf_modes.cpp:349`):
```
[ELRS] 40 channels, 903.5-926.9 MHz, hop every 4 packets, sync ch=20, 10 dBm
```

**Missing from output:** ToA calculation, dwell time, hops/s, preamble length, sync word value, P_detect estimate. This info is needed to make JJ a teaching tool.

### E3. Menu Structure — Partially Exists

**Required by:** v2 §7.3

**Current menu structure** (from `menu.h` and `menu.cpp`):
```
Main Menu
├── [1] RID Spoofer
├── [2] RF Sig Gen
│   ├── CW Tone
│   ├── Band Sweep
│   ├── ELRS 915 FHSS
│   ├── Crossfire 915
│   ├── Power Ramp
│   └── << Back
├── [3] False Pos Gen
│   ├── LoRaWAN Sim
│   ├── ISM Burst Noise
│   ├── Mixed (IoT+ELRS)
│   └── << Back
├── [4] Combined
└── [5] Swarm Sim
```

**v2 required structure** (§7.3):
```
Signal Generator
├── Drone Protocols
│   ├── ELRS FHSS (with domain/rate/state submenus)
│   ├── Crossfire (with band/mode submenus)
│   ├── SiK Radio (NEW)
│   ├── mLRS (NEW)
│   ├── Custom LoRa Direct (NEW)
│   └── FrSky R9 ACCESS (NEW, when confirmed)
├── Infrastructure (False Positive Testing)
│   ├── LoRaWAN US915 SB2 (with node count/interval submenus)
│   ├── LoRaWAN EU868 (NEW)
│   ├── Meshtastic Beacon (NEW)
│   ├── Helium PoC Beacons (NEW)
│   ├── Class B Gateway Beacon (NEW)
│   └── Dense Mixed (NEW)
├── Special Modes
│   ├── CW Tone / Band Sweep / Power Ramp
│   ├── Drone Swarm
│   └── Combined Dual-Core
```

**Gap:** The current flat submenu needs restructuring into a hierarchical drone-protocols / infrastructure / special taxonomy. The OLED 128×64 display can only show 4 menu items at a time, so deep nesting requires careful scrolling UX.

### E4. Code to Delete or Refactor

| File | What | Action |
|---|---|---|
| `rf_modes.cpp` L251–257 | ELRS constants | Move to `protocol_params.h` |
| `combined_mode.cpp` L28–34 | Duplicated ELRS constants | Replace with `#include` |
| `power_ramp.cpp` L11–17 | Duplicated ELRS constants | Replace with `#include` |
| `false_positive.cpp` L148–154 | Duplicated ELRS constants | Replace with `#include` |
| `rf_modes.cpp` L259 | Comment says "permutation of 0..79" | Fix stale comment |
| All 4 files | LCG constants (1664525, 1013904223) | Change to 0x343FD, 0x269EC3 |
| All 4 files | Fisher-Yates builds 40-entry sequence | Should build 240-entry sequence per §3.1.6 |
| `crossfire.cpp` L14 | `CRSF_HOP_INTERVAL_US = 20000` | Change to 6667 |

---

## F. Regional Compliance Gaps

### F1. EU 863–870 MHz Support

**Current:** No EU868 mode exists anywhere in the codebase. All ELRS simulation is FCC915 only.

**Required by v2:**
- ELRS EU868: 13 channels, 863.275–869.575 MHz, hop interval 8 (§3.1.1, §3.1.3)
- Crossfire 868: 863–870 MHz, ~27 channels (§3.2.1)
- LoRaWAN EU868: 868.1/868.3/868.5 MHz mandatory (§4.2)
- **Duty cycle enforcement** for EU868 (§2.2): 1% in 865–868.6 MHz sub-band

### F2. Duty Cycle Enforcement

**Current:** No duty cycle enforcement exists. JJ transmits continuously in all modes.

**Required by v2 §2.2:** ELRS EU868 at 200 Hz has 80% per-channel duty cycle, exceeding the 1% ETSI limit. JJ must enforce pauses or restrict EU868 modes to duty-cycle-compliant patterns. The v2 doc explicitly states: "JJ must enforce duty cycle compliance when simulating EU868 modes."

### F3. Australian Band

**Current:** No AU915 mode. The FCC915 band (902–928 MHz) is wider than Australia's 915–928 MHz.

**Required by v2 §2.3:** AU915 uses 915.5–926.9 MHz with 20 channels.

---

## G. Test Coverage

### G1. Existing Test Scripts

| Script | Location | What It Tests | JJ Modes Covered |
|---|---|---|---|
| `run_test.py` | `C:\Projects\sentry-rf\` | Single mode: JJ (COM6) → SENTRY-RF (COM9) | c, e, b, r, m, x, w, q (baseline) |
| `dual_test.py` | `C:\Projects\sentry-rf\` | Dual-device: JJ → SENTRY-RF, basic detection | All serial command modes |
| `threshold_test.py` | `C:\Projects\sentry-rf\` | Detection probability vs power level | CW, ELRS (power sweep) |
| `full_validation.py` | `C:\Projects\sentry-rf\` | All modes sequentially, 8/8 pass criteria | All current modes |

### G2. Tests Currently NOT Covered

| Test Needed | Why |
|---|---|
| ELRS multi-rate (SF7/8/9) detection | SENTRY-RF must detect all pilot types, not just 200 Hz racers |
| ELRS EU868 detection | Different channel plan and duty cycle produces different diversity pattern |
| Crossfire at 150 Hz | Current test runs at 50 Hz (incorrect hop rate) |
| Crossfire dual-modulation | SENTRY-RF needs both CAD and FSK Phase 3 |
| SiK GFSK detection | SENTRY-RF's FSK Phase 3 must detect SiK |
| mLRS slow-rate detection (19 Hz) | Tests lower bound of persistence gate |
| LoRaWAN multi-channel false positive rejection | Current LoRaWAN test is single-channel, doesn't stress AAD catalog |
| Meshtastic false positive rejection | Different sync word and preamble length |
| Helium PoC false positive rejection | Most likely to fool sustained-diversity gate |
| ELRS binding/beacon state detection | Fixed-frequency LoRa looks like infrastructure |
| Custom LoRa detection | Tests SENTRY-RF against non-library drones |
| Power ramp with corrected payload (8 bytes) | Current test uses 4-byte payload |

### G3. Test Script Port Discrepancy

The test scripts reference **COM6** (JJ) and **COM8/COM9** (SENTRY-RF). The Sprint 1 flash used **COM12**. Port assignments may need updating or parameterizing.

---

## H. Recommended Implementation Order

### Sprint 2: Architecture Foundation (LOW RISK, UNBLOCKS EVERYTHING)

1. **Create `include/protocol_params.h`** with all ELRS, Crossfire, and LoRaWAN constants
2. **Replace duplicated constants** in all 4 ELRS-embedding source files
3. **Fix channel spacing formula** to use `(N-1)` divisor
4. **Fix CR** from 4/5 to 4/7
5. **Fix preamble** from 8 to 6 symbols
6. **Fix LCG constants** to 0x343FD / 0x269EC3
7. **Fix Crossfire hop rate** from 50 Hz to 150 Hz
8. **Fix power_ramp payload** from 4 to 8 bytes
9. **Add protocol info output** per §7.2

**Why first:** Every subsequent sprint depends on correct shared constants and deduplication. This sprint changes no behavior for passing tests except making them more accurate.

### Sprint 3: ELRS Multi-Rate + EU868 (HIGH VALUE)

1. Add ELRS rate selection: 200/100/50/25 Hz (SF6/7/8/9, adjust packet interval)
2. Add ELRS EU868 domain (13 channels, hop interval 8)
3. Add EU duty cycle enforcement
4. Add ELRS binding/beacon state (fixed-frequency on sync channel)
5. Add DVDA modes (D250, D500 with hop interval 2)

**Why second:** Tests SENTRY-RF against all real pilot configurations. EU868 tests AAD's ability to handle different temporal patterns.

### Sprint 4: Fix LoRaWAN False Positive Generator (HIGH VALUE)

1. Rotate through all 8 SB2 channels
2. Fix sync word to 0x34 (public LoRaWAN)
3. Add multi-node simulation (1/4/8 virtual nodes)
4. Add BW500 DR4 uplink mode (triggers CAD — critical for testing)
5. Add configurable interval (30s/60s/300s)

**Why third:** The AAD architecture's ambient catalog is designed to filter LoRaWAN. Without accurate LoRaWAN simulation, we can't validate AAD.

### Sprint 5: New Infrastructure Modes (MEDIUM VALUE)

1. Meshtastic beacon (16-sym preamble, sync word 0x2B, periodic)
2. Helium PoC beacons (multi-hotspot, rotating channels)
3. Dense mixed mode (ELRS + LoRaWAN + Meshtastic TDM)

**Why fourth:** Tests the hardest false-positive scenarios.

### Sprint 6: SiK Radio (HIGH VALUE, INDEPENDENT)

1. Implement SiK GFSK FHSS (50 channels, TDM timing)
2. Support 64/125/250 kbps air rates
3. US 915 and EU 868 bands

**Why fifth:** SiK is common in real-world drones. Tests SENTRY-RF's FSK Phase 3 path.

### Sprint 7: Crossfire Improvements + mLRS (MEDIUM VALUE)

1. Add Crossfire 50 Hz LoRa mode
2. Add Crossfire dual-modulation (FSK + LoRa TDM)
3. Add Crossfire 868 band
4. Implement mLRS (after [VERIFY] items resolved)

### Sprint 8: Custom LoRa + Menu Restructure (LOW RISK)

1. Custom LoRa mode (user-configurable SF/BW/freq/hop)
2. Restructure menu per v2 §7.3 taxonomy
3. Add ELRS AU915/IN866/433 domains

---

*End of audit. This document is the deliverable for sprint planning.*
