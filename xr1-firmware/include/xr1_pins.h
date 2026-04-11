#pragma once

// ============================================================================
// RadioMaster XR1 pin mapping (ESP32C3 host + LR1121 transceiver)
// Source: ELRS "Generic C3 LR1121" target — default values, NOT confirmed
//         against XR1 hardware.json yet.
//
// VERIFY: Every pin in this file must be cross-checked against the XR1's
// hardware.json (downloaded from the ELRS web UI AP at http://10.0.0.1 after
// holding the bind button 5-7 seconds). Any pin that differs must be updated
// before the first custom-firmware flash or the LR1121 will not enumerate.
// ============================================================================

// --- LR1121 SPI (ESP32C3 GP-SPI / FSPI) ---------------------------------
#define LR1121_SCK   6   // VERIFY against hardware.json radio.sck
#define LR1121_MOSI  4   // VERIFY against hardware.json radio.mosi
#define LR1121_MISO  5   // VERIFY against hardware.json radio.miso
#define LR1121_NSS   7   // VERIFY — active LOW chip select
#define LR1121_RST   2   // VERIFY — NRESET, active LOW
#define LR1121_BUSY  3   // VERIFY — HIGH while LR1121 is processing
#define LR1121_DIO9  1   // VERIFY — IRQ line (ELRS calls this radio.dio1)

// --- XR1 on-board peripherals -------------------------------------------
#define LED_RGB      8   // VERIFY — WS2812B status LED
#define BUTTON       9   // VERIFY — bind/boot button, also bootloader strap

// --- UART to T3S3 host (Phase 2) ----------------------------------------
// The T3S3 side of this link is GPIO 43 (TX) and GPIO 44 (RX) per
// docs/XR1_Integration_Research.md §3.3. Baud rate chosen in Phase 2.
#define UART_RX     20   // VERIFY — host TX reaches us here
#define UART_TX     21   // VERIFY — our TX reaches host RX here

// --- LR1121 TCXO ---------------------------------------------------------
// T3S3's LR1121 V1.3 module uses a 3.0 V TCXO. The XR1 variant is unverified
// but we assume 3.0 V until confirmed; a mismatch will silently break RX
// sensitivity without throwing an init error.
// VERIFY against XR1 schematic or ELRS target JSON.
#define LR1121_TCXO_VOLTAGE  3.0f
