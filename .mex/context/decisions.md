---
name: decisions
description: Protocol and architecture decisions.
triggers:
  - "why"
  - "decision"
last_updated: 2026-04-10
---

# Decisions

## Decision Log

### "Not a jammer" — framing and legal posture
**Date:** 2025
**Status:** Active — core product identity
**Decision:** JJ is a calibrated RF signal generator, NOT a jammer. All documentation, UI text, and marketing material reinforces this.
**Reasoning:** Actual jamming is illegal in most jurisdictions. JJ generates the specific signals a drone detector expects to see, at bench-safe power, for validation. The name is a joke — the function is test instrumentation. Clear framing protects users and Seaforged legally.
**Consequences:** All power levels default to low. Shielded enclosure / attenuator guidance is explicit in the README. Remote ID broadcasts are labeled as test beacons with fake serial numbers.

### All protocol params in a single header with citations
**Date:** 2025
**Status:** Active
**Decision:** Every protocol constant (channel tables, hop intervals, LCG seeds, bit rates, sync words) lives in `include/protocol_params.h` with an inline comment citing the source.
**Reasoning:** Two goals: (1) operators / reviewers can audit the constants against primary sources (ExpressLRS firmware, SiK docs, LoRa Alliance spec, FCC 47 CFR 15.247), (2) adding a new protocol is a single-file edit plus a new mode file.
**Alternatives considered:** Per-protocol header files (rejected — duplicates citations, harder to cross-check).
**Consequences:** `protocol_params.h` is large. Constants marked `[VERIFY]` still need primary-source confirmation.

### Core 0 WiFi, Core 1 SX1262 in Combined mode
**Date:** 2025
**Status:** Active
**Decision:** When a mode uses both WiFi and the SX1262 (e.g., Combined mode `x`), WiFi runs on Core 0 and the LoRa TX loop runs on Core 1.
**Reasoning:** Avoids SPI contention and interrupt priority inversion. WiFi has its own driver loop that runs best isolated. LoRa hop timing is tight and benefits from exclusive core access.
**Consequences:** FreeRTOS task architecture is mandatory for multi-radio modes. Single-radio modes can run in `loop()` directly.
