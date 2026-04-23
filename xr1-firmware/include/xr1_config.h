#pragma once

#include <RadioLib.h>
#include "xr1_pins.h"

// ============================================================================
// LR1121 RF switch table — decoded from hardware.json radio_rfsw_ctrl
//
// ELRS encodes the LR1121's SetDioAsRfSwitch command (opcode 0x0109) as an
// 8-byte array:
//   [enableMask, standbyCfg, rxCfg, txCfg, txHpCfg, txHfCfg, gnssCfg, wifiCfg]
// where each *Cfg byte is a bitmask over the enabled DIOs, bit N = DIO(5+N):
//   bit 0 = DIO5, bit 1 = DIO6, bit 2 = DIO7, bit 3 = DIO8, bit 4 = DIO10
//
// XR1 hardware.json:  radio_rfsw_ctrl = [15, 0, 12, 8, 8, 6, 0, 5]
//   enable    = 0b01111  -> DIO5, DIO6, DIO7, DIO8 all drive the RF switch
//   standby   = 0b00000  -> all LOW
//   rx        = 0b01100  -> DIO7 + DIO8 HIGH
//   tx        = 0b01000  -> DIO8 HIGH           (low-power / sub-GHz TX path)
//   tx_hp     = 0b01000  -> DIO8 HIGH           (sub-GHz high-power, same path on XR1)
//   tx_hf     = 0b00110  -> DIO6 + DIO7 HIGH    (2.4 GHz PA path)  <-- important
//   gnss      = 0b00000
//   wifi      = 0b00101  -> DIO5 + DIO7 HIGH
//
// RadioLib 7.6.0 exposes LR11x0-specific RF switch modes for the extra
// tx_hp/tx_hf/gnss/wifi states above. For LR11x0 devices these table rows are
// encoded directly into the chip's SetDioAsRfSwitch command, so we can mirror
// hardware.json exactly instead of bit-banging DIO lines around each transmit.
// ============================================================================
static const uint32_t XR1_RFSW_DIO_PINS[] = {
    RADIOLIB_LR11X0_DIO5,
    RADIOLIB_LR11X0_DIO6,
    RADIOLIB_LR11X0_DIO7,
    RADIOLIB_LR11X0_DIO8,
    RADIOLIB_NC
};

static const Module::RfSwitchMode_t XR1_RFSW_TABLE[] = {
    // mode                 DIO5  DIO6  DIO7  DIO8
    { LR11x0::MODE_STBY,  { LOW,  LOW,  LOW,  LOW  } },  // standby = 0
    { LR11x0::MODE_RX,    { LOW,  LOW,  HIGH, HIGH } },  // rx      = 12
    { LR11x0::MODE_TX,    { LOW,  LOW,  LOW,  HIGH } },  // tx      = 8  (sub-GHz LP)
    { LR11x0::MODE_TX_HP, { LOW,  LOW,  LOW,  HIGH } },  // tx_hp   = 8  (sub-GHz HP)
    { LR11x0::MODE_TX_HF, { LOW,  HIGH, HIGH, LOW  } },  // tx_hf   = 6  (2.4 GHz)
    { LR11x0::MODE_GNSS,  { LOW,  LOW,  LOW,  LOW  } },  // gnss    = 0
    { LR11x0::MODE_WIFI,  { HIGH, LOW,  HIGH, LOW  } },  // wifi    = 5
    END_OF_MODE_TABLE,
};

// ============================================================================
// Power level tables — register values from hardware.json, not raw dBm.
// RadioLib's setOutputPower() translates to the active band's PA settings.
// ============================================================================

// 2.4 GHz band (hardware.json power_values):
//   index 0..3 -> register 12, 16, 19, 22  (matches XR1 100 mW spec at max)
static const int8_t XR1_POWER_2G4_DBM[] = { 12, 16, 19, 22 };

// Sub-GHz band (hardware.json power_values_dual):
//   index 0..3 -> register -10, -6, -3, 2
// The XR1 C3 variant's sub-GHz PA is weak by design; the T3S3 SX1262 remains
// our primary sub-GHz emitter. Values are +2 dB across the board vs. the
// Generic C3 LR1121 defaults we previously assumed.
static const int8_t XR1_POWER_SUBGHZ_DBM[] = { -10, -6, -3, 2 };

// LNA gain — hardware.json omits power_lna_gain, so keep the ELRS default.
// Unused until Phase 3+ introduces CAD / listen-before-talk.
static const uint8_t XR1_LNA_GAIN = 12;

// ============================================================================
// Regulator + LED color order (both from hardware.json)
// ============================================================================

// hardware.json radio_dcdc = true -> use internal DC-DC, not LDO. xr1_radio.cpp
// already calls setRegulatorDCDC(); this constant exists so the choice is
// grep-able against the JSON field name if it ever changes.
#define XR1_DCDC_ENABLE  1

// hardware.json led_rgb_isgrb = true -> WS2812 expects byte order G,R,B
// (not R,G,B). Any driver that treats the pixel as 0xRRGGBB will paint red
// where green is intended. This flag is consumed by the future WS2812 driver.
#define LED_RGB_IS_GRB   1

// ============================================================================
// SPI timing
//
// Conservative 500 kHz for bring-up — LR1121 datasheet permits up to 16 MHz,
// but the first-flash goal is to trace SPI on a logic analyzer if getVersion()
// fails. Raise to NORMAL after the self-test passes on real hardware.
// ============================================================================
#define XR1_SPI_HZ_BRINGUP  500000UL
#define XR1_SPI_HZ_NORMAL   8000000UL
