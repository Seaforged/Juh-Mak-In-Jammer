---
name: router
description: Session bootstrap.
edges:
  - target: context/architecture.md
    condition: when working on signal emulation modes or the mode dispatcher
  - target: context/stack.md
    condition: when working with RadioLib, SX1262, or ESP32 radios
  - target: context/conventions.md
    condition: when writing firmware
  - target: context/decisions.md
    condition: when making protocol decisions
  - target: context/setup.md
    condition: when setting up PlatformIO or the hardware
last_updated: 2026-04-10
---

# Session Bootstrap

Read `AGENTS.md` first.

## Current Project State

**Version:** v2.0.0 — 16+ operating modes across 5 protocol families

**Working:**
- ExpressLRS FHSS (6 air rates: e1-e6, 4 domains: FCC915/AU915/EU868/IN866, binding state)
- TBS Crossfire GFSK (150 Hz, 85.1 kbps, 100 channels)
- SiK Radio GFSK TDM+FHSS (64/125/250 kbps)
- mLRS LoRa/FSK FHSS (19/31/50 Hz modes)
- Custom LoRa (configurable SF/BW/frequency/hop pattern)
- WiFi Remote ID + BLE beacon spoofing (ASTM F3411)
- Drone Swarm (1-16 virtual drones, orbit patterns)
- False positive testing: LoRaWAN US915, LoRaWAN EU868, Meshtastic, Helium PoC, Mixed
- Serial command interface (type `h` for help)

**Not yet built:**
- GPS spoofing / jamming (separate tool: `spoof` / `juh-mak-in-jammer-gps`)
- Combined multi-protocol scenario sequencer
- Automated regression tests against SENTRY-RF

**Known issues:**
- Constants marked `[VERIFY]` in `docs/JJ_Protocol_Emulation_Reference_v2.md` are not yet cross-checked against primary sources — TODO comments in code
- Drone Swarm mode memory usage climbs with cycle count — stable at 16, untested beyond

## Routing Table

| Task | Load |
|------|------|
| Signal emulation / mode dispatcher | `context/architecture.md` |
| RadioLib / SX1262 / ESP32 specifics | `context/stack.md` |
| Writing firmware | `context/conventions.md` |
| Protocol decisions / citations | `context/decisions.md` |
| PlatformIO / hardware setup | `context/setup.md` |

## Behavioural Contract

Standard loop. VERIFY includes: `pio run` with zero errors across the active environments, serial command interface responsive, RF output confirmed via SENTRY-RF or spectrum analyzer.
