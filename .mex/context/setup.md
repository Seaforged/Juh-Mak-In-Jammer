---
name: setup
description: Build, flash, monitor.
triggers:
  - "setup"
  - "pio"
last_updated: 2026-04-10
---

# Setup

## Prerequisites

- **PlatformIO** (VS Code extension or CLI)
- **LilyGo T3S3 V1.3** board with antenna connected
- **USB-C cable**
- **Sub-GHz antenna** on the SMA connector — **never power on without it**

## First-time Setup

1. `git clone https://github.com/Seaforged/Juh-Mak-In-Jammer.git && cd Juh-Mak-In-Jammer`
2. `pio run -e t3s3` to build
3. Connect antenna, then USB-C
4. `pio run -e t3s3 --target upload`
5. `pio device monitor -b 115200`
6. On boot, JJ prints the help menu — type `e` to start ELRS 200 Hz FCC915 simulation

## Common Commands

- `pio run -e t3s3`
- `pio run -e t3s3 --target upload --upload-port COM<N>`
- `pio device monitor -b 115200 -p COM<N>`
- `pio run -e t3s3 --target clean` — clear build cache

## Common Gotchas

- **T3S3 uses native USB CDC** — requires `-DARDUINO_USB_CDC_ON_BOOT=1` (already in `platformio.ini`). Serial is unreliable for the first ~300ms after boot.
- **T3S3 I2C is SDA=18/SCL=17** — opposite of Heltec V3. Don't copy pins from Heltec code.
- **`esp_wifi_80211_tx()` auto-calculates FCS** — don't append your own checksum.
- **BLE + WiFi simultaneous:** works on ESP32-S3 but watch heap usage (~150KB combined).
- **RadioLib `radio.transmit()` blocks** — use in a FreeRTOS task, not in a tight loop.
