---
name: architecture
description: Mode dispatcher + radio driver + serial command interface.
triggers:
  - "architecture"
  - "mode"
  - "dispatcher"
last_updated: 2026-04-10
---

# Architecture

## System Overview

Serial command interface parses operator commands (`e1f`, `g`, `r`, etc.) → mode dispatcher sets radio params per protocol → SX1262 (via RadioLib) transmits LoRa / FSK signals on the selected channel sequence → ESP32 WiFi driver broadcasts Remote ID beacons when in `r` / `w` / `x` modes → OLED displays current mode, frequency, power, and TX status. Boot button cycles through menu (short press) and selects (long press).

Protocols that use BOTH LoRa and WiFi (like Combined mode `x`) split work across cores: WiFi on Core 0, SX1262 on Core 1, to avoid radio contention.

## Key Components

- **Mode dispatcher** (`src/modes/`) — one file per protocol family, each exposing `start()` / `stop()` / `tick()`
- **Radio driver** — RadioLib SX1262 wrapper, configured per mode
- **Serial command parser** — `src/serial_cmd.cpp`, maps characters to mode transitions
- **Menu UI** — OLED rendering + boot button state machine
- **Protocol parameters** — all constants in `include/protocol_params.h` with v2 reference citations inline

## What Does NOT Exist Here

- No GPS spoofing / jamming — separate tool (`spoof` workspace uses AntSDR E200 via UHD)
- No packet capture / sniffing — this is TX only
- No SD card logging (yet) — test results logged via Python test scripts in SENTRY-RF repo
