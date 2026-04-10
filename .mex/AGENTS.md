---
name: agents
description: Project anchor. Read first.
last_updated: 2026-04-10
---

# JUH-MAK-IN JAMMER

## What This Is

Sub-GHz drone signal emulator on ESP32-S3 + SX1262 (LilyGo T3S3). Calibrated RF signal generator that simulates ELRS, Crossfire, SiK Radio, mLRS, Custom LoRa, and WiFi Remote ID protocols for validating passive drone detection systems like SENTRY-RF. NOT a jammer — a test instrument. MIT licensed. v2.0.0.

## Non-Negotiables

- **This is not a jammer.** It does not disrupt communications. It is a calibrated RF signal generator for controlled testing.
- **Never hardcode pin numbers** — always use symbols from `board_config.h`
- **User is responsible for RF compliance** — shielded enclosures or inline attenuators for conducted testing
- **Do not use to impersonate real aircraft** — Remote ID spoofing generates test beacons only
- Commit after each feature works, not at end of sprint
- Keep functions under ~40 lines

## Commands

- Build: `pio run -e t3s3`
- Flash: `pio run -e t3s3 --target upload`
- Monitor: `pio device monitor -b 115200`
- Serial commands: type `h` or `?` at the JJ prompt for the full menu

## Navigation

Read `ROUTER.md` next.
