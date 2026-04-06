# JUH-MAK-IN JAMMER v2.0 — Phased Development Plan

**Date:** April 5, 2026  
**Author:** Claude (Anthropic) — Technical Lead & Architecture  
**Executor:** Claude Code (CC) — Implementation  
**Decision Authority:** ND (Seaforged) — Final approval, all hardware tasks  
**Reference:** `docs/JJ_Protocol_Emulation_Reference_v2.md` (JJ-PROTOCOL-REF-v2.0)  
**Baseline:** JJ v1.1.0, 8/8 automated tests passing, commit `4ba5fa3`

---

## How to Use This Document

This plan contains **5 phases with 12 sprint prompts**. Each sprint prompt is designed to be copied directly into Claude Code. 

**Workflow for every sprint:**
1. Copy the sprint prompt into CC
2. CC executes the code changes
3. You flash the board: `pio run --target upload`
4. Open serial monitor: `pio device monitor`
5. Test the mode and paste serial output back to CC
6. Run automated tests if applicable: `python run_test.py`
7. Commit when acceptance criteria pass
8. Come back to this chat for review before the next sprint

**Critical rules for CC:**
- Flash to hardware after EVERY sprint — no stacking unverified changes
- Run `python run_test.py` after every sprint to verify no regressions
- Commit after each passing sprint, not at the end of a phase

---

## Phase Overview

| Phase | Sprints | What It Does | Risk Level |
|---|---|---|---|
| **1: Foundation** | 1A, 1B | Create `protocol_params.h`, rewire all files to use it, add protocol info output | Medium — touches every file but changes no behavior |
| **2: Fix What Exists** | 2A, 2B, 2C | Correct ELRS parameters, fix Crossfire timing, fix LoRaWAN false positive generator | Low — correcting values in place |
| **3: ELRS Full Coverage** | 3A, 3B | Multi-rate ELRS (all 6 air rates), multi-domain (EU868, AU915, 433), connection states | Low — extending existing working mode |
| **4: New Protocols** | 4A, 4B, 4C | SiK Radio (GFSK), mLRS (LoRa FHSS), Custom LoRa Direct (user-configurable) | Medium — new radio configurations |
| **5: Infrastructure & Polish** | 5A, 5B | Meshtastic/Helium/LoRaWAN EU868 false positive modes, interactive serial menu | Low — additive, no changes to existing modes |

**Estimated total:** 8–12 CC sessions depending on complexity per sprint.

---

## PHASE 1: Foundation — Single Source of Truth (Est. 2 sessions)

### Why This Phase Exists

The audit found 28 ELRS constants duplicated across 4 files. Every bug fix currently has to be applied 4 times. Before we fix any values or add any protocols, we need a single header file that owns all protocol parameters. This is the concept of **"Don't Repeat Yourself" (DRY)** — one of the most important principles in software engineering. When the same information exists in multiple places, they will inevitably get out of sync.

### Sprint 1A — Create `protocol_params.h` and Consolidate All Constants

**CC Prompt:**

```
Read the reference document at docs/JJ_Protocol_Emulation_Reference_v2.md — this is the authoritative source for all protocol parameters.

Create a new file include/protocol_params.h that becomes the SINGLE SOURCE OF TRUTH for every protocol constant in JJ. This file must contain:

1. ELRS CHANNEL PLANS (from v2 ref Section 3.1.1):
   - Struct: ElrsDomain { const char* name; float freqStart; float freqStop; uint8_t channels; uint8_t syncChannel; }
   - All 9 domains: FCC915, AU915, EU868, IN866, AU433, EU433, US433, US433W, ISM2G4
   - Channel frequency formula: freq = freqStart + (channelIndex * (freqStop - freqStart) / (channels - 1))
   - NOTE: divisor is (channels - 1), NOT channels. This is a correction from v1.

2. ELRS AIR RATE MODES (from v2 ref Section 3.1.2):
   - Struct: ElrsAirRate { const char* name; uint16_t rateHz; uint8_t sf; uint32_t bw; uint8_t cr; uint8_t preambleLen; uint8_t payloadLen; uint8_t hopInterval; bool isDvda; }
   - 6 modes: 200Hz(SF6), 100Hz(SF7), 50Hz(SF8), 25Hz(SF9), D250(SF6), D500(SF5)
   - CRITICAL CORRECTIONS from v2: CR is 4/7 (not 4/5), preamble is 6 symbols (not 8), DVDA modes use hopInterval=2

3. ELRS FHSS LCG CONSTANTS (from v2 ref Section 3.1.6):
   - LCG_MULTIPLIER = 0x343FD
   - LCG_INCREMENT = 0x269EC3
   - NOT the Knuth constants currently in the code

4. LORA SYNC WORDS (from v2 ref Section 6.3):
   - ELRS: 0x12 (private)
   - LoRaWAN: 0x34 (public)
   - Meshtastic: 0x2B (private)

5. CROSSFIRE PARAMETERS (from v2 ref Section 3.2):
   - 915 band: 902-928 MHz, ~100 channels, 260 kHz spacing
   - 868 band: 863-870 MHz (NOT 850-870), ~27 channels, 260 kHz spacing
   - FSK uplink: 85.1 kbps GFSK, 150 Hz packet rate
   - LoRa downlink: 50 Hz
   - FSK deviation: 50 kHz [VERIFY]

6. SIK RADIO PARAMETERS (from v2 ref Section 3.3):
   - US915: 915.000-928.000 MHz, 50 channels, channel_width = (MAX-MIN)/(NUM+2)
   - Modulation: GFSK
   - Air speeds: 4, 64, 125, 250 kbps
   - Default NETID: 25, default power: 20 dBm

7. LORAWAN US915 (from v2 ref Section 4.1):
   - Sub-band 2 (TTN/Helium default): 903.9, 904.1, 904.3, 904.5, 904.7, 904.9, 905.1, 905.3 MHz
   - Sync word: 0x34 (public)
   - SF range: SF7-SF12
   - Preamble: 8 symbols

8. LORAWAN EU868 (from v2 ref Section 4.2):
   - Mandatory channels: 868.1, 868.3, 868.5 MHz
   - Downlink RX2: 869.525 MHz, SF9/BW125

9. MESHTASTIC PARAMETERS (from v2 ref Section 4.3):
   - US: 902.0-928.0 MHz, ~104 channels
   - Preamble: 16 symbols
   - Sync word: 0x2B

10. MLRS PARAMETERS (from v2 ref Section 3.4):
    - Rates: 19 Hz (LoRa), 31 Hz (LoRa, SX1262 only), 50 Hz (FSK, SX1262 only)
    - Bands: 868/915/433 MHz
    - Channel counts: [VERIFY] — mark as placeholder with TODO comment

Every constant MUST have an inline comment citing the v2 reference section (e.g., "// v2 ref §3.1.1 [Ref P1]").

Include a header guard and keep the file organized with clear section headers.

After creating protocol_params.h, update ALL files that currently have hardcoded protocol constants to #include "protocol_params.h" and use the new structs/constants instead of their local definitions. The files to update are:
- src/rf_modes.cpp (ELRS FHSS mode)
- src/combined_mode.cpp (combined dual-core mode)
- src/power_ramp.cpp (power ramp mode)
- src/false_positive.cpp (LoRaWAN false positive mode)
- src/crossfire_mode.cpp (Crossfire FSK mode — if it exists as a separate file, otherwise wherever Crossfire is implemented)

DO NOT change any mode behavior yet — this sprint only moves constants to protocol_params.h and replaces hardcoded values with references to the header. The modes should work exactly as before, just sourcing their values from one place.

After making changes, verify:
- pio run compiles cleanly
- All existing serial commands still work (c, e, b, r, m, x, w, n, p, d, s, q)

Acceptance criteria:
- protocol_params.h exists with ALL constants listed above
- ZERO hardcoded protocol frequencies, SFs, BWs, or channel counts remain in any .cpp file
- Every constant has a v2 reference citation comment
- Compiles cleanly
- Existing automated tests still pass (python run_test.py)
```

---

### Sprint 1B — Protocol Info Output System

**CC Prompt:**

```
Read docs/JJ_Protocol_Emulation_Reference_v2.md Section 7.2.

Add a protocol info output system to JJ. When any mode starts, it should print a complete parameter summary to serial. This turns JJ into a teaching tool — anyone reading the serial output can see exactly what RF parameters are being simulated and why.

Create a new file src/protocol_info.cpp (and include/protocol_info.h) with a function:

void printProtocolInfo(const char* protocolName, float freqStart, float freqStop, 
                       uint8_t channels, uint8_t sf, uint32_t bw, uint16_t rateHz,
                       uint8_t hopInterval, uint8_t preambleLen, uint8_t syncWord,
                       int8_t txPower, const char* modulationType);

This function prints output in this format:
[ELRS-FCC915] 40ch 903.5-926.9MHz SF6/BW500 200Hz hop_every_4 sync_ch=20
  Mod: LoRa  ToA: 4.1ms/pkt  Dwell: 20ms/freq  Hops: 50/s
  Preamble: 6sym  SyncWord: 0x12  Power: 10 dBm

The function should CALCULATE these derived values from the input parameters:
- ToA (time on air) using the LoRa formula from v2 ref Section 3.1.4
- Dwell time = hopInterval * (1000 / rateHz) ms
- Hops per second = rateHz / hopInterval

For FSK modes (Crossfire, SiK), use a simpler output:
[Crossfire-915] ~100ch 902-928MHz GFSK@85.1kbps 150Hz
  Dwell: 6.7ms/freq  Hops: 150/s  Power: 10 dBm

Call printProtocolInfo() at the start of every existing mode:
- ELRS FHSS mode (command 'e')
- Crossfire FSK mode (command 'x')
- LoRaWAN false positive mode (command 'n')
- Combined mode (command 'b')
- Power ramp mode (command 'p')

Acceptance criteria:
- Every mode prints a complete parameter summary when started
- ToA calculation matches v2 ref Section 3.1.4 table values (within 0.5 ms)
- Compiles cleanly
- Existing tests still pass
```

---

## PHASE 2: Fix What Exists (Est. 2–3 sessions)

### Why This Phase Exists

The audit found that existing modes work but use wrong values. ELRS has the wrong coding rate, preamble length, FHSS constants, and channel formula. Crossfire hops at 50 Hz instead of 150 Hz. LoRaWAN uses the wrong sync word and only one channel. These are all "the mode runs but it's not simulating the real protocol accurately" problems.

### Sprint 2A — Correct All ELRS Parameters

**CC Prompt:**

```
The ELRS FHSS mode currently works but uses incorrect parameters. Using protocol_params.h as the source of truth (which now has the correct v2 values), update the ELRS mode to use accurate parameters:

CORRECTIONS NEEDED (all values from protocol_params.h):

1. CODING RATE: Change from CR 4/5 to CR 4/7.
   In RadioLib, this is radio.setCodingRate(7) — the parameter is the denominator.

2. PREAMBLE LENGTH: Change from 8 symbols to 6 symbols.
   radio.setPreambleLength(6)

3. SYNC WORD: Set to 0x12 (ELRS private network).
   radio.setSyncWord(0x12)

4. CHANNEL FREQUENCY FORMULA: Change from:
     freq = startFreq + (channel * (stopFreq - startFreq) / numChannels)
   to:
     freq = startFreq + (channel * (stopFreq - startFreq) / (numChannels - 1))
   The (numChannels - 1) divisor means channel 0 = startFreq and channel (N-1) = stopFreq exactly.

5. FHSS SEQUENCE GENERATION: Replace the current LCG constants with the real ELRS values:
   - Multiplier: 0x343FD (was using Knuth constant)
   - Increment: 0x269EC3 (was using Knuth constant)
   If the current code uses a simple sequential or random hop, replace it with the actual ELRS FHSS algorithm from protocol_params.h / v2 ref Section 3.1.6:
   a. Use a fixed "binding phrase" (e.g., "JJ_TEST") → MD5 hash → first 6 bytes = UID
   b. Last 4 bytes of UID → seed for LCG
   c. LCG seeds Fisher-Yates shuffle on channel index array
   d. Insert sync channel at every freq_count-th position

6. HOP INTERVAL: Verify the hop interval is 4 for FCC915 200Hz mode (hop every 4 packets).

Do NOT add new air rate modes yet — that's Sprint 3A. This sprint only corrects the 200 Hz FCC915 mode that already exists.

After making corrections, the ELRS mode ('e' command) should produce this serial output via the protocol info system:
[ELRS-FCC915] 40ch 903.5-926.9MHz SF6/BW500 200Hz hop_every_4 sync_ch=20
  Mod: LoRa  ToA: ~4.1ms/pkt  Dwell: 20ms/freq  Hops: 50/s
  Preamble: 6sym  SyncWord: 0x12  Power: 10 dBm

Acceptance criteria:
- CR is 4/7 (verify via serial debug or RadioLib config print)
- Preamble is 6 symbols
- Sync word is 0x12
- Channel 0 = 903.500 MHz, Channel 39 = 926.900 MHz (verify first and last channel frequencies in serial output)
- FHSS sequence uses 0x343FD / 0x269EC3 LCG
- Protocol info prints on mode start
- Compiles cleanly
- Existing automated tests still pass
```

---

### Sprint 2B — Fix Crossfire Timing and Dual-Modulation

**CC Prompt:**

```
Read docs/JJ_Protocol_Emulation_Reference_v2.md Section 3.2.

The Crossfire mode currently works but has two major issues:
1. Hop rate is 50 Hz — real Crossfire 150 Hz mode hops at 150 Hz (6.667 ms per hop)
2. Only simulates FSK — real Crossfire uses dual-modulation (FSK uplink + LoRa downlink)

Fix the Crossfire mode:

1. TIMING: Change the packet interval from 20 ms (50 Hz) to 6.667 ms (150 Hz).
   This is 150 hops per second across ~100 channels.

2. CHANNEL PLAN: Verify the 915 band uses 902-928 MHz with ~260 kHz spacing (~100 channels).
   For 868 band: use 863-870 MHz (NOT 850-870) with ~260 kHz spacing (~27 channels).
   Use values from protocol_params.h.

3. FSK PARAMETERS: Verify 85.1 kbps GFSK with ~50 kHz deviation.

4. DUAL-MODULATION MODE (NEW): Add a new sub-mode that alternates between FSK and LoRa packets on the same channel sequence, simulating real Crossfire TDM:
   - Every 3rd packet: LoRa (SF7/BW500, 50 Hz equivalent rate)
   - Other packets: GFSK 85.1 kbps
   This requires switching between LoRa and FSK packet types. Use SPI opcode 0x8A (SetPacketType) — do NOT call radio.begin() to switch, as that resets the chip.
   
   To switch to FSK: send SPI command {0x8A, 0x00} (GFSK packet type)
   To switch to LoRa: send SPI command {0x8A, 0x01} (LoRa packet type)
   After switching packet type, reconfigure the relevant parameters (bitrate for FSK, SF/BW for LoRa).

5. SERIAL COMMAND: The existing Crossfire command ('x') should now show a sub-menu:
   x1 = Crossfire 150 Hz FSK only (915 band)
   x2 = Crossfire 50 Hz LoRa only (915 band)
   x3 = Crossfire Dual-Mode FSK+LoRa TDM (915 band)
   x4 = Crossfire 150 Hz FSK only (868 band)
   Just typing 'x' with no number defaults to x1 (backward compatible).

Acceptance criteria:
- 'x' or 'x1' runs FSK at 150 Hz hop rate (6.667 ms interval) — verify via serial timing output
- 'x2' runs LoRa at 50 Hz
- 'x3' alternates FSK and LoRa packets
- 'x4' uses 863-870 MHz band (NOT below 863)
- Protocol info prints for each sub-mode
- Compiles cleanly
- Existing automated tests still pass
```

---

### Sprint 2C — Fix LoRaWAN False Positive Generator

**CC Prompt:**

```
Read docs/JJ_Protocol_Emulation_Reference_v2.md Section 4.1.

The LoRaWAN false positive mode currently works but is not realistic:
1. Uses private sync word (0x12) — real LoRaWAN uses PUBLIC sync word 0x34
2. Uses a single channel — real LoRaWAN US915 Sub-Band 2 uses 8 channels
3. Uses fixed SF — real LoRaWAN nodes vary SF7-SF12

Fix the LoRaWAN false positive mode:

1. SYNC WORD: Set to 0x34 (LoRaWAN public network, from protocol_params.h).

2. CHANNELS: Transmit on all 8 Sub-Band 2 channels, rotating through them:
   903.9, 904.1, 904.3, 904.5, 904.7, 904.9, 905.1, 905.3 MHz
   (values from protocol_params.h)

3. SPREADING FACTOR: Randomize SF per transmission — pick SF7, SF8, SF9, SF10, SF11, or SF12 randomly for each packet. This simulates ADR (Adaptive Data Rate) where different nodes use different SFs.

4. BANDWIDTH: Use BW125 (125 kHz), not BW500. LoRaWAN US915 uplink (125 kHz channels) uses BW125.

5. PREAMBLE: Set to 8 symbols (LoRaWAN standard, from protocol_params.h).

6. TIMING: LoRaWAN Class A nodes transmit sporadically — once every 30-300 seconds per node. The mode should simulate N nodes (default 4) each transmitting at random intervals within a configurable range.
   
   Implementation: Use a simple timer array. Each "virtual node" has a random countdown timer between 30-60 seconds. When a node's timer fires, transmit one packet on a random SB2 channel at a random SF, then reset that node's timer.

7. SERIAL COMMAND: Keep 'n' as the command. Add optional parameter for node count:
   n = 4 nodes (default)
   n1 = 1 node
   n4 = 4 nodes
   n8 = 8 nodes

8. The mode should print when each virtual node transmits:
   [LoRaWAN-US915-SB2] Node 3/4 TX: 904.3MHz SF10/BW125 SyncWord:0x34

Acceptance criteria:
- Sync word is 0x34 (verify by checking that SENTRY-RF does NOT flag these as private-network LoRa)
- Transmissions rotate across all 8 SB2 channels
- SF varies per transmission (SF7-SF12)
- BW is 125 kHz
- Preamble is 8 symbols
- Multi-node timing is sporadic (30-60s intervals), not periodic
- Protocol info prints on mode start
- Compiles cleanly
- Existing automated tests still pass
```

---

## PHASE 3: ELRS Full Coverage (Est. 2 sessions)

### Why This Phase Exists

Real ELRS transmitters can run at 6 different air rates (25/50/100/200/D250/D500 Hz) across 9 different regional frequency domains. Currently JJ only simulates 200 Hz FCC915. To properly test SENTRY-RF's detection across all spreading factors, we need all rates and the major regional bands.

### Sprint 3A — Multi-Rate ELRS

**CC Prompt:**

```
Extend the ELRS mode to support all 6 air rate modes from protocol_params.h:

| Mode | Rate | SF | Hop Interval | DVDA? |
|------|------|----|-------------|-------|
| 200 Hz | 200 | SF6 | 4 | No |
| 100 Hz | 100 | SF7 | 4 | No |
| 50 Hz | 50 | SF8 | 4 | No |
| 25 Hz | 25 | SF9 | 4 | No |
| D250 | 250 | SF6 | 2 | Yes |
| D500 | 500 | SF5 | 2 | Yes |

All modes use BW500, CR 4/7, preamble 6, sync word 0x12 (from protocol_params.h).

The key difference per mode is:
- SF (spreading factor) — affects CAD detection and ToA
- Packet interval — 1000/rateHz milliseconds between packets
- Hop interval — standard modes hop every 4 packets, DVDA modes hop every 2

Update the serial command for ELRS:
e = ELRS 200 Hz FCC915 (default, backward compatible)
e1 = ELRS 200 Hz FCC915
e2 = ELRS 100 Hz FCC915
e3 = ELRS 50 Hz FCC915
e4 = ELRS 25 Hz FCC915
e5 = ELRS D250 FCC915
e6 = ELRS D500 FCC915

Each mode should print its protocol info with the correct derived values (ToA, dwell time, hops/s).

This is important for SENTRY-RF testing because:
- SF6 (200 Hz) is racing/freestyle drones — fastest, hardest to catch
- SF9 (25 Hz) is long-range drones — slowest, longest dwell time
- SF5 (D500) is the fastest mode ELRS supports — tests SENTRY-RF's upper detection limit

Acceptance criteria:
- All 6 air rates selectable via serial commands e1-e6
- Each mode uses correct SF from protocol_params.h
- Hop interval is 4 for standard modes, 2 for DVDA modes
- Protocol info prints correct ToA for each mode
- SF6 ToA ~4.1 ms, SF9 ToA ~19.5 ms (from v2 ref Section 3.1.4)
- Compiles cleanly
- Existing tests still pass
```

---

### Sprint 3B — Multi-Domain ELRS + Connection States

**CC Prompt:**

```
Read docs/JJ_Protocol_Emulation_Reference_v2.md Sections 3.1.1, 3.1.3, and 3.1.7.

Extend the ELRS mode to support multiple regional frequency domains AND connection state simulation.

PART 1 — MULTI-DOMAIN:

Add domain selection as a second character after the rate:
e1f = ELRS 200 Hz FCC915 (default)
e1a = ELRS 200 Hz AU915
e1u = ELRS 200 Hz EU868
e1i = ELRS 200 Hz IN866
e4u = ELRS 25 Hz EU868 (for testing slow EU mode)

Domain parameters (from protocol_params.h):
| Domain | Key | Channels | Freq Range | Hop Interval |
|--------|-----|----------|------------|-------------|
| FCC915 | f | 40 | 903.5-926.9 MHz | 4 |
| AU915 | a | 20 | 915.5-926.9 MHz | 8 |
| EU868 | u | 13 | 863.275-869.575 MHz | 8 |
| IN866 | i | 4 | 865.375-866.950 MHz | 8 |

Note: AU915 and EU868 use hop interval 8 (not 4 like FCC915). DVDA modes always use 2 regardless of domain.

For EU868 mode specifically: display a warning that ETSI duty cycle limits apply in real deployments (1% for most sub-bands). JJ does NOT enforce duty cycle because it's a test instrument, but the warning educates the operator.

PART 2 — CONNECTION STATES:

Add a binding/beacon state simulation. When activated, JJ transmits on a SINGLE fixed frequency (the sync channel for the selected domain) at 1 Hz for 10 seconds, then transitions to full FHSS. This simulates a pilot powering on their transmitter before the receiver connects.

Serial command: append 'b' for binding state
e1fb = ELRS 200 Hz FCC915 Binding→Connected sequence

Serial output during binding:
[ELRS-FCC915] BINDING: TX on sync channel (915.2 MHz) 1 Hz...
[ELRS-FCC915] BINDING: 10s elapsed, transitioning to FHSS...
[ELRS-FCC915] CONNECTED: 40ch FHSS active

This tests whether SENTRY-RF can detect the binding-to-FHSS transition — a real-world scenario where a pilot powers up nearby.

Acceptance criteria:
- At least FCC915, AU915, EU868, and IN866 domains work
- Channel counts and frequency ranges match protocol_params.h exactly
- EU868 prints duty cycle warning
- Binding state transmits on sync channel for 10s then transitions to FHSS
- Hop intervals are domain-correct (4 for FCC, 8 for AU/EU)
- Protocol info prints correct parameters for each domain
- Compiles cleanly
- Existing tests still pass
```

---

## PHASE 4: New Protocols (Est. 3 sessions)

### Why This Phase Exists

SENTRY-RF needs to detect more than just ELRS. SiK radios are the standard MAVLink telemetry link on ArduPilot/PX4 drones. mLRS is growing in the long-range FPV community. Custom LoRa links are used by DIY and military drones. Each protocol has a different RF signature that tests different parts of SENTRY-RF's detection engine.

### Sprint 4A — SiK Radio (GFSK MAVLink Telemetry)

**CC Prompt:**

```
Read docs/JJ_Protocol_Emulation_Reference_v2.md Section 3.3.

Implement a new SiK Radio simulation mode. SiK radios (RFD900, 3DR Radio) are the standard MAVLink telemetry link for ArduPilot and PX4 drones. They use GFSK modulation with FHSS, which is fundamentally different from ELRS LoRa — SENTRY-RF must use FSK Phase 3 detection (not CAD) to find SiK links.

PROTOCOL PARAMETERS (from protocol_params.h):
- Band: 915.000-928.000 MHz (US)
- Channels: 50 (default)
- Channel width: (928000-915000) / (50+2) = 250 kHz
- Channel formula: channel[n] = 915000 + (250/2) + (n * 250) kHz (guard_delta + n * channel_width)
- Modulation: GFSK
- Default air speed: 64 kbps
- TDM: Synchronous — TX window then RX window on each frequency, hop after each TDM frame

IMPLEMENTATION:
1. Configure the SX1262 in FSK mode using RadioLib:
   - radio.beginFSK() or use SPI opcode 0x8A to switch to FSK packet type
   - radio.setBitRate(64.0) — 64 kbps default
   - radio.setFrequencyDeviation(25.0) — standard GFSK deviation for 64 kbps
   - radio.setRxBandwidth(156.2) — RadioLib FSK RX bandwidth
   - radio.setPreambleLength(32) — SiK uses 32-bit preamble
   
2. Generate 50 channel frequencies using the SiK formula with NETID=25 skew.

3. Hop timing: SiK TDM uses ~20 ms TX windows. Simulate by:
   - Transmit a short GFSK burst (10-20 ms of data) on the current channel
   - Hop to the next channel
   - Repeat at ~50 hops/second (20 ms per channel)

4. The TX payload should be random bytes (we don't need valid MAVLink framing — SENTRY-RF only detects the RF energy, not the protocol content).

SERIAL COMMANDS:
k = SiK Radio 64 kbps, 50 channels, US915 (default)
k1 = SiK Radio 64 kbps
k2 = SiK Radio 125 kbps
k3 = SiK Radio 250 kbps

Print protocol info on start:
[SiK-US915] 50ch 915.0-928.0MHz GFSK@64kbps TDM
  Channel width: 250kHz  Dwell: ~20ms/freq  Hops: 50/s  Power: 10 dBm

Acceptance criteria:
- 'k' command starts SiK simulation
- SX1262 is in FSK mode (not LoRa) — verify by confirming SENTRY-RF CAD does NOT detect it, but RSSI sweep does
- 50 channels span 915-928 MHz with correct spacing
- Hop rate is approximately 50 hops/second
- Protocol info prints on start
- Compiles cleanly
- Existing tests still pass
- NEW TEST: SENTRY-RF RSSI sweep shows energy across 915-928 MHz band when SiK mode is active
```

---

### Sprint 4B — mLRS (Slow LoRa FHSS)

**CC Prompt:**

```
Read docs/JJ_Protocol_Emulation_Reference_v2.md Section 3.4.

Implement an mLRS simulation mode. mLRS is important because its 19 Hz mode represents the SLOWEST FHSS pattern SENTRY-RF needs to detect — it tests the lower bound of the frequency diversity detector.

PROTOCOL PARAMETERS (from protocol_params.h):
- mLRS 19 Hz mode: LoRa, ~19 Hz packet rate
- mLRS 31 Hz mode: LoRa, SX1262 only, ~31 Hz
- mLRS 50 Hz mode: FSK, SX1262 only, ~50 Hz
- Bands: 868/915/433 MHz
- FHSS: LoRa FHSS with symmetric TX/RX alternation
- Channel counts: [VERIFY — use placeholder of 20 channels for 915 band]

NOTE: mLRS channel counts and exact SF/BW parameters are marked [VERIFY] in the reference doc. Use reasonable estimates for now:
- 19 Hz: SF8/BW500 (estimated — gives ~11 ms ToA, fits in 52 ms frame)
- 31 Hz: SF7/BW500 (estimated — gives ~6.7 ms ToA, fits in 32 ms frame)  
- 50 Hz: GFSK 64 kbps (similar to SiK)
- 20 channels for 915 band (estimated)

Mark all estimated values with TODO comments so we can correct them when we clone and read the mLRS repo.

SYMMETRIC HOPPING: mLRS alternates TX and RX frames. The hop rate is half the packet rate (since TX and RX alternate). For 19 Hz: ~9.5 hops/second. This means:
- Dwell time per frequency: ~105 ms (one TX frame + one RX frame before hopping)
- This is MUCH slower than ELRS 200 Hz (20 ms dwell) — it's the hardest FHSS pattern for SENTRY-RF to detect quickly

SERIAL COMMANDS:
l = mLRS 19 Hz, 915 band (default)
l1 = mLRS 19 Hz
l2 = mLRS 31 Hz
l3 = mLRS 50 Hz (FSK)

Acceptance criteria:
- 'l' command starts mLRS 19 Hz simulation
- 19 Hz mode uses LoRa (CAD-detectable by SENTRY-RF)
- Hop rate is approximately 9.5 hops/second (not 19 — symmetric alternation)
- 50 Hz mode uses FSK (not LoRa)
- All estimated parameters have TODO comments
- Protocol info prints on start
- Compiles cleanly
- Existing tests still pass
```

---

### Sprint 4C — Custom LoRa Direct (User-Configurable)

**CC Prompt:**

```
Read docs/JJ_Protocol_Emulation_Reference_v2.md Section 3.6.

Implement a Custom LoRa Direct mode — a fully user-configurable LoRa transmitter for simulating non-standard drone links. Many custom-built drones use point-to-point LoRa without any standardized FHSS protocol. This mode tests whether SENTRY-RF can detect unknown drone signatures.

CONFIGURABLE PARAMETERS (all set via serial commands):
- Frequency: any single frequency 860-930 MHz (default: 915.0 MHz)
- SF: SF5-SF12 (default: SF7)
- BW: 7.8, 10.4, 15.6, 20.8, 31.25, 41.7, 62.5, 125, 250, 500 kHz (default: 125 kHz)
- Power: -9 to +22 dBm (default: 10 dBm)
- Packet rate: 1-100 Hz (default: 10 Hz)
- Hop mode: none (fixed frequency), 2-channel alternating, or random N-channel
- Sync word: 0x12, 0x34, or custom (default: 0x12)

SERIAL INTERFACE:
u = Start Custom LoRa with current settings (default: 915 MHz, SF7, BW125, 10 Hz, no hop)
u? = Print current Custom LoRa settings
uf915.5 = Set frequency to 915.5 MHz
us7 = Set SF to 7
ub125 = Set BW to 125 kHz
ur20 = Set rate to 20 Hz
uh0 = No hopping (fixed frequency)
uh2 = 2-channel alternating (±500 kHz from center)
uh5 = Random 5-channel hopping (channels spread across ±2 MHz from center)
up10 = Set power to 10 dBm
uw12 = Set sync word to 0x12

This mode is deliberately flexible — it lets the operator create any LoRa signal to test SENTRY-RF's detection of unknown patterns.

IMPLEMENTATION:
- Store settings in a CustomLoraConfig struct
- Parse serial input character by character after 'u' prefix
- Settings persist until changed or device reset
- When no hopping: transmit on fixed frequency at the specified rate
- When 2-channel: alternate between (center - 500kHz) and (center + 500kHz)
- When N-channel: generate N evenly-spaced channels within ±2 MHz of center, hop randomly

Acceptance criteria:
- 'u' starts transmission with default settings
- 'u?' prints all current settings
- Can change frequency, SF, BW, rate, hop mode, power, sync word via serial
- Fixed frequency mode works (single frequency, steady rate)
- 2-channel alternating works
- N-channel random hopping works
- Protocol info prints on start showing all configured parameters
- Compiles cleanly
- Existing tests still pass
```

---

## PHASE 5: Infrastructure Modes & Polish (Est. 2 sessions)

### Why This Phase Exists

SENTRY-RF's biggest real-world challenge isn't detecting drones — it's NOT detecting things that aren't drones. Meshtastic mesh networks, Helium hotspots, and LoRaWAN gateways all produce LoRa energy that can trigger false positives. JJ needs to simulate these patterns so we can tune SENTRY-RF's discrimination.

### Sprint 5A — Meshtastic, Helium PoC, and LoRaWAN EU868

**CC Prompt:**

```
Read docs/JJ_Protocol_Emulation_Reference_v2.md Sections 4.2, 4.3, and 4.4.

Add three new infrastructure simulation modes for false positive testing:

MODE 1 — MESHTASTIC BEACON:
Meshtastic nodes transmit periodic beacons on a fixed channel with long preambles.
- Frequency: 906.875 MHz (US Long-Fast default channel)
- SF: SF11/BW250 (Long-Fast preset)
- Preamble: 16 symbols (from protocol_params.h) — this is 2.67x longer than ELRS
- Sync word: 0x2B (Meshtastic-specific)
- TX pattern: One beacon every 15 minutes + occasional message bursts (3-5 packets in 2 seconds, simulating mesh relay cascade)

Serial command: f1 = Meshtastic beacon

Why this matters: Meshtastic uses a DIFFERENT sync word (0x2B) and MUCH longer preamble (16 vs 6) than ELRS. If SENTRY-RF's CAD triggers on Meshtastic and doesn't discriminate, we have a false positive source. The single fixed frequency means diversity count stays at 1 — this should NOT trigger SENTRY-RF's FHSS diversity threshold.

MODE 2 — HELIUM PoC BEACONS:
Helium hotspots transmit LoRa beacons for Proof-of-Coverage.
- Frequencies: Rotate through LoRaWAN US915 SB2 channels (8 channels from protocol_params.h)
- SF: SF8-SF10 (variable)
- Sync word: 0x34 (public LoRaWAN)
- Preamble: 8 symbols
- TX pattern: Multiple simulated hotspots, each transmitting ~1 beacon per 30 seconds, staggered. With 5 simulated hotspots, this creates ~10 beacons per minute across different channels.

Serial command: f2 = Helium PoC (5 hotspots)

Why this matters: Helium PoC is the HARDEST false positive pattern because multiple hotspots on different channels create slow frequency diversity — it looks like a very slow FHSS drone. SENTRY-RF's diversity velocity check (requiring 3+ NEW frequencies per scan cycle) should reject this, but we need to test it.

MODE 3 — LORAWAN EU868:
European LoRaWAN nodes on the mandatory channels.
- Frequencies: 868.1, 868.3, 868.5 MHz (3 mandatory channels)
- SF: SF7-SF12 (random per TX, simulating ADR)
- Sync word: 0x34
- BW: 125 kHz
- Preamble: 8 symbols
- TX pattern: 1 node, transmitting every 60 seconds on a random mandatory channel
- Duty cycle: Display a note that EU 1% duty cycle applies

Serial command: f3 = LoRaWAN EU868

SERIAL COMMAND STRUCTURE:
f = Infrastructure false positive modes
f1 = Meshtastic
f2 = Helium PoC
f3 = LoRaWAN EU868
f4 = Dense Mixed (FUTURE — not this sprint)

Acceptance criteria:
- f1 transmits with 16-symbol preamble and sync word 0x2B
- f2 creates multi-hotspot beacon pattern across SB2 channels
- f3 transmits on 868.1/868.3/868.5 MHz with public sync word
- Each mode prints protocol info on start
- SENTRY-RF should NOT escalate to WARNING or CRITICAL on any of these modes (they should stay CLEAR or at most ADVISORY) — test this!
- Compiles cleanly
- Existing tests still pass
```

---

### Sprint 5B — Interactive Serial Menu & Help System

**CC Prompt:**

```
Read docs/JJ_Protocol_Emulation_Reference_v2.md Section 7.3.

Replace the current flat serial command system with an organized, hierarchical menu. The current system uses single-letter commands (c, e, b, r, m, x, w, n, p, d, s, q) which doesn't scale now that we have 15+ modes.

NEW MENU SYSTEM:

When JJ boots or when the user types 'h' or '?', print:

=== JUH-MAK-IN JAMMER v2.0 — Drone Signal Emulator ===

DRONE PROTOCOLS:
  e  ELRS FHSS      e1-e6=rate  f/a/u/i=domain  b=binding
  x  Crossfire       x1=FSK150  x2=LoRa50  x3=Dual  x4=EU868
  k  SiK Radio       k1=64k  k2=125k  k3=250k
  l  mLRS            l1=19Hz  l2=31Hz  l3=50Hz(FSK)
  u  Custom LoRa     u?=settings  uf/us/ub/ur/uh/up/uw=config

INFRASTRUCTURE (False Positive Testing):
  n  LoRaWAN US915   n1/n4/n8=nodes
  f1 Meshtastic      16-sym preamble, sync 0x2B
  f2 Helium PoC      5 hotspots, rotating SB2 channels
  f3 LoRaWAN EU868   868.1/868.3/868.5 MHz

SPECIAL MODES:
  c  CW Tone         w=power ramp  b=combined  p=approach sim
  r  Remote ID       WiFi+BLE broadcast
  d  Drone Swarm     d1-d16=drone count
  m  Mixed FP        Combined false positive patterns

CONTROLS:
  s  Stop            q=stop  h/?=this menu

Each mode group shows its sub-commands inline so the user can see all options at a glance.

IMPLEMENTATION:
- Modify the serial command parser to handle multi-character commands
- Keep ALL existing single-letter commands working (backward compatibility)
- Add 'h' and '?' as aliases for the help menu
- When an invalid command is entered, print "Unknown command. Type 'h' for help."
- The menu should fit in an 80-column terminal without wrapping

Acceptance criteria:
- 'h' or '?' prints the full menu
- All existing single-letter commands still work exactly as before
- All new multi-character commands (e1-e6, x1-x4, k1-k3, l1-l3, f1-f3, etc.) work
- Invalid commands show help hint
- Menu fits in 80 columns
- Compiles cleanly
- ALL automated tests still pass (this is the final sprint — nothing should be broken)
```

---

## Post-Phase Checklist

After all 5 phases are complete, verify:

- [ ] `protocol_params.h` is the ONLY location for protocol constants
- [ ] Zero hardcoded frequencies, SFs, BWs, or channel counts in any .cpp file
- [ ] Every mode prints protocol info on start
- [ ] All 8 original automated tests still pass
- [ ] All new modes are accessible via serial commands
- [ ] Help menu ('h') shows all available commands
- [ ] EU868 bands respect 863-870 MHz limits
- [ ] LoRaWAN modes use public sync word 0x34
- [ ] ELRS modes use private sync word 0x12 with CR 4/7 and 6-symbol preamble
- [ ] Crossfire 150 Hz mode runs at correct hop rate
- [ ] SiK mode uses GFSK (not LoRa)
- [ ] mLRS 19 Hz mode has symmetric hop rate (~9.5 hops/s)

---

## Items Deferred (Future Phases)

These are NOT in this plan but are documented for future work:

| Item | Why Deferred | When |
|---|---|---|
| FrSky R9 ACCESS | Parameters not confirmed (proprietary protocol) | After [VERIFY] items resolved |
| mLRS exact channel plans | Requires cloning mLRS repo and reading fhss.h | Before mLRS field validation |
| Dense Mixed mode (f4) | Requires all individual infrastructure modes working first | After Phase 5 |
| ELRS 2.4 GHz (ISM2G4) | Requires LR1121 hardware (not SX1262) | LR1121 sprint |
| EU duty cycle enforcement | JJ is a test instrument — enforcement not required but could be added | Low priority |
| Automated tests for new modes | Need new test scripts for SiK, mLRS, Custom LoRa, infrastructure modes | After Phase 5 |
| OLED display for mode selection | Would allow untethered operation without serial terminal | After menu system proven |

---

*This plan is a living document. Update sprint prompts as audit findings are resolved and field testing reveals new requirements. Review with Claude (this chat) before each sprint to catch issues early.*
