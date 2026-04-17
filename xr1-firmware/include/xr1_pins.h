#pragma once

// ============================================================================
// RadioMaster XR1 pin mapping (ESP32C3 host + LR1121 transceiver)
//
// Source of truth: docs/XR1_Hardware.json — downloaded from the ELRS web UI of
// THIS specific XR1 unit (ELRS v3.5.1). Every pin below was cross-checked
// against that file during Phase 2 Sprint 2.2 and matches the Generic C3
// LR1121 target exactly; no RadioMaster-specific overlay was applied.
// ============================================================================

// --- LR1121 SPI (ESP32C3 GP-SPI / FSPI) ---------------------------------
#define LR1121_SCK   6   // CONFIRMED via hardware.json radio_sck
#define LR1121_MOSI  4   // CONFIRMED via hardware.json radio_mosi
#define LR1121_MISO  5   // CONFIRMED via hardware.json radio_miso
#define LR1121_NSS   7   // CONFIRMED via hardware.json radio_nss (active LOW)
#define LR1121_RST   2   // CONFIRMED via hardware.json radio_rst  (NRESET, active LOW)
#define LR1121_BUSY  3   // CONFIRMED via hardware.json radio_busy (HIGH while busy)
#define LR1121_DIO9  1   // CONFIRMED via hardware.json radio_dio1 (IRQ line; ELRS calls this dio1)

// --- XR1 on-board peripherals -------------------------------------------
#define LED_RGB      8   // CONFIRMED via hardware.json led_rgb (WS2812B; see LED_RGB_IS_GRB in xr1_config.h)
#define BUTTON       9   // CONFIRMED via hardware.json button  (bind/boot button; also bootloader strap)

// --- UART to T3S3 host (Phase 2) ----------------------------------------
// T3S3 side: GPIO 43 (TX) / 44 (RX) per docs/XR1_Integration_Research.md §3.3.
// The XR1's secondary UART (hardware.json serial1_rx=18, serial1_tx=19) is
// available on PCB pads but not wired in this build.
#define UART_RX     20   // CONFIRMED via hardware.json serial_rx (host TX lands here)
#define UART_TX     21   // CONFIRMED via hardware.json serial_tx (our TX to host RX)

// --- LR1121 TCXO ---------------------------------------------------------
// hardware.json does not expose a tcxo/xtal field, so this value is still
// an educated guess: 3.0 V matches the T3S3 LR1121 V1.3 module and is the
// most common RadioMaster choice. A mismatch will not throw an init error
// but silently destroys RX sensitivity and frequency accuracy — if RX tests
// in later phases show poor SNR, re-check this against the XR1 schematic.
#define LR1121_TCXO_VOLTAGE  3.0f   // UNCONFIRMED — assumed from T3S3 module
