---
name: conventions
description: Firmware conventions.
triggers:
  - "convention"
  - "firmware"
last_updated: 2026-04-10
---

# Conventions

## Naming

- **Files:** snake_case (`elrs_mode.cpp`, `crossfire_mode.cpp`)
- **Functions:** camelCase (`elrsStart`, `elrsTick`)
- **Constants:** SCREAMING_SNAKE_CASE (`ELRS_FCC915_CHANNELS`, `SF6_BW500`)
- **Pins:** `PIN_*` prefix, only in `board_config.h`

## Structure

- One file per protocol family in `src/modes/`
- All protocol constants in `include/protocol_params.h` (single source of truth with citations)
- Serial command dispatch in `src/serial_cmd.cpp`
- Display rendering in `src/display.cpp`
- Never mix protocols in the same source file

## Verify Checklist

- [ ] `pio run` compiles clean (zero warnings)
- [ ] New protocol constants added to `protocol_params.h` with a citation comment
- [ ] Serial help menu (`h` command) updated if a new mode was added
- [ ] Test against SENTRY-RF serial log shows the mode was detected
- [ ] No pin numbers hardcoded outside `board_config.h`
