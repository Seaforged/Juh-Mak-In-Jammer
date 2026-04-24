#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

// ============================================================
// JUH-MAK-IN JAMMER-RF — Board Configuration
// Target: LilyGo T3S3 (ESP32-S3 + SX1262)
// ============================================================

#ifdef BOARD_T3S3

// --- LoRa SX1262 SPI Pins ---
#define LORA_SCK    5
#define LORA_MISO   3
#define LORA_MOSI   6
#define LORA_CS     7
#define LORA_RST    8
#define LORA_DIO1   33
#define LORA_BUSY   34

// --- OLED SSD1306 I2C ---
#define OLED_SDA    18
#define OLED_SCL    17
#define OLED_ADDR   0x3C
#define OLED_WIDTH  128
#define OLED_HEIGHT 64

// --- User Interface ---
#define LED_PIN     37
#define BOOT_BTN    0    // Menu navigation button

// --- XR1 UART link (Phase 3) ---
// T3S3 side: Serial1 on these pins, 115200 8N1. Cross-wired to the XR1's
// UART0 (GPIO 20 RX / GPIO 21 TX) through the physical harness.
#define XR1_UART_TX_PIN  43    // T3S3 TX -> XR1 GPIO 20 (RX)
#define XR1_UART_RX_PIN  44    // T3S3 RX <- XR1 GPIO 21 (TX)

// --- Board Identification ---
#define BOARD_NAME  "LilyGo T3S3"

#else
#error "No board defined! Add -DBOARD_T3S3 to build_flags."
#endif

#endif // BOARD_CONFIG_H
