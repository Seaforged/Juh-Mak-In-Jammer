# JJ XR1 Firmware

Custom firmware for the **RadioMaster XR1 Nano** (ESP32C3 + Semtech LR1121).
Runs alongside the main T3S3 JJ firmware in the repository root as part of the
**Juh-Mak-In Jammer v3.0** three-emitter architecture:

| Emitter | Chip | Role |
|---|---|---|
| T3S3 onboard | SX1262 | Primary sub-GHz emitter (ELRS 900, Crossfire, mLRS, etc.) |
| XR1 LR1121 | LR1121 | 2.4 GHz LoRa/FSK + supplementary sub-GHz |
| XR1 WiFi/BLE | ESP32C3 built-in | ASTM F3411 Remote ID + DJI DroneID |

The T3S3 controls the XR1 over UART (Phase 2+).

## This is not a jammer

The XR1 firmware is a calibrated test-signal generator for validating passive
detection systems like SENTRY-RF. Do not impersonate real aircraft, do not
transmit outside permitted ISM bands, and do not radiate without a shielded
enclosure or an inline attenuator for conducted testing.

## Build + flash

```bash
pio run -e xr1                    # compile
pio run -e xr1 --target upload    # flash (XR1 must be in bootloader)
pio device monitor -b 115200      # serial console
```

**Bootloader:** hold the XR1 `BOOT`/`BIND` button (GPIO 9) while applying
power, then run the upload target.

**ELRS rollback:** the stock ELRS firmware binary should be saved to
`docs/xr1_elrs_backup.bin` before the first custom flash (see Sprint 0.3 in
`docs/JJ_XR1_Phased_Development_Plan.md`). Re-flash via the ELRS web UI at
`http://10.0.0.1` (hold bind button 5–7 s to start the AP).

## Pin mapping

All pins default to the ELRS **Generic C3 LR1121** target. Every pin in
`include/xr1_pins.h` is marked with a `VERIFY` comment and must be checked
against the XR1's `hardware.json` (downloadable from the ELRS web UI) before
the first flash. See `docs/XR1_Integration_Research.md` §3.2.

| Function | ESP32C3 GPIO |
|---|---|
| LR1121 SCK / MOSI / MISO / NSS | 6 / 4 / 5 / 7 |
| LR1121 RST / BUSY / DIO9 (IRQ) | 2 / 3 / 1 |
| RGB LED (WS2812) | 8 |
| BOOT / BIND button | 9 |
| UART to T3S3 RX / TX | 20 / 21 |

TCXO voltage defaults to 3.0 V (matches T3S3 LR1121 V1.3 module).

## Phase state

See `docs/JJ_XR1_Phased_Development_Plan.md` for the full 6-phase plan.

| Phase | Status | Files |
|---|---|---|
| 0 — Wiring + ELRS backup | done | — |
| 1 — Hello Hardware (self-test two bands) | done | `main.cpp`, `xr1_radio.cpp` |
| 2 — UART command protocol | done | `xr1_uart.cpp`, `xr1_radio.cpp` (operational setters) |
| 3 — WS2812 LED + T3S3 driver | done (T3S3 side) | `xr1_radio.cpp` LED, T3S3 `xr1_driver.cpp` |
| 4 — 2.4 GHz protocol profiles (T3S3-driven) | done (T3S3 side) | T3S3 `xr1_modes.cpp` |
| 5 — Remote ID (WiFi + BLE + DJI) | done | `remote_id_{wifi,ble,common,nan}.cpp`, `dji_droneid.cpp` |
| 6 — Combined scenarios | done (T3S3 side) | T3S3 `combined_scenarios.cpp` |
| 7 — Polish + validation | — | — |

## Development rules

- **Pins:** only ever referenced via symbols in `include/xr1_pins.h`. Never
  hardcode a GPIO number anywhere else.
- **Protocol constants:** every SF/BW/CR, bitrate, deviation, channel
  frequency, and dwell time cited back to
  `docs/JJ_Protocol_Emulation_Reference_v2.md` or a primary source. No
  "approximately" values — the signal must be RF-layer valid (see the
  kickoff prompt for the rationale).
- **Comments explain *why*, not *what*.** Obvious code gets no comment.
  Non-obvious choices — TCXO voltage, RF-switch ordering, ESP32C3 SPI quirks
  — get a short note with the reason.
- **Functions** aim for under ~40 lines. Split when they grow.
- **Commits** are atomic per working feature, not per sprint boundary. Use
  conventional-commit style: `feat(xr1): ...`, `fix(xr1): ...`.
- **LR1121 custom firmware caveat:** ELRS v3.6+ writes custom firmware into
  the LR1121 chip itself. If RadioLib can't talk to the LR1121 after flash,
  re-flash the stock Semtech transceiver image
  (`lr1121_transceiver_0103.bin` from
  `github.com/Lora-net/radio_firmware_images`) via the ELRS web UI's
  `lr1121.html` page before retrying.

## References

- `docs/JJ_XR1_Phased_Development_Plan.md` — roadmap and sprint details
- `docs/XR1_Integration_Research.md` — hardware and RF capability analysis
- `docs/JJ_Protocol_Emulation_Reference_v2.md` — authoritative protocol parameters
- [RadioLib LR1121 API](https://jgromes.github.io/RadioLib/class_l_r1121.html)
- [ELRS Generic C3 LR1121 target](https://github.com/ExpressLRS/targets/blob/master/RX/Generic%20C3%20LR1121.json)
- [opendroneid-core-c](https://github.com/opendroneid/opendroneid-core-c) (vendored in `lib/opendroneid/`)
