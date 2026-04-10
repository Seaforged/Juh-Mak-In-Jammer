---
name: stack
description: ESP32-S3 + SX1262 + RadioLib.
triggers:
  - "RadioLib"
  - "SX1262"
  - "ESP32"
last_updated: 2026-04-10
---

# Stack

## Core Technologies

- **C++ Arduino** via PlatformIO
- **Espressif32** platform, **ESP32-S3** target
- **LilyGo T3S3 V1.3** board (native USB, 2MB PSRAM, SD card, QWIIC)
- **Semtech SX1262** radio (150-960 MHz, LoRa SF5-12, FSK 0.6-300 kbps, +22 dBm)

## Key Libraries

- **RadioLib v7+** — SX1262 LoRa/FSK TX for drone signal simulation
- **Adafruit SSD1306 + GFX** — OLED menu
- **opendroneid-core-c** (vendored in `lib/opendroneid/`) — ASTM F3411 Remote ID encoding
- **ESP-IDF WiFi API** — `esp_wifi_80211_tx()` for raw beacon injection
- **ESP-IDF BLE API** — `esp_ble_gap_config_adv_data_raw()` for BLE advertising

## What We Deliberately Do NOT Use

- **No LMIC** — we transmit test signals, not LoRaWAN payloads
- **No NimBLE** — ESP32-S3 Bluedroid is fine for advertising-only use
- **No dynamic allocation in TX loops** — fixed buffers, deterministic timing
