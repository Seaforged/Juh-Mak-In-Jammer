#pragma once

#include <RadioLib.h>
#include "xr1_pins.h"

// ============================================================================
// LR1121 RF switch table
//
// The LR1121 drives an external antenna-path switch through DIO5/DIO6. The
// exact truth table depends on how RadioMaster wired the XR1 RF switch — this
// mirrors the ELRS Generic C3 LR1121 layout documented in
// docs/XR1_Integration_Research.md §6.
//
// VERIFY: Wrong switch mapping yields clean SPI + silent RF. If Phase 1 shows
// SPI working but SENTRY-RF sees nothing on-air, suspect this table first.
// ============================================================================
static const uint32_t XR1_RFSW_DIO_PINS[] = {
    RADIOLIB_LR11X0_DIO5,
    RADIOLIB_LR11X0_DIO6,
    RADIOLIB_NC,
    RADIOLIB_NC,
    RADIOLIB_NC
};

static const Module::RfSwitchMode_t XR1_RFSW_TABLE[] = {
    { Module::MODE_IDLE, { LOW,  LOW  } },
    { Module::MODE_RX,   { HIGH, LOW  } },
    { Module::MODE_TX,   { LOW,  HIGH } },
    END_OF_MODE_TABLE,
};

// ============================================================================
// Power level tables (from ELRS Generic C3 LR1121 target)
// These are LR1121 register values, NOT dBm directly — RadioLib's
// setOutputPower() converts appropriately for the active band.
// ============================================================================

// 2.4 GHz band: XR1 datasheet claims 100 mW (+20 dBm) telemetry output.
static const int8_t XR1_POWER_2G4_DBM[] = { 12, 16, 19, 22 };

// Sub-GHz band: XR1 C3 variant has a weak sub-GHz PA path. Keep realistic
// expectations — the T3S3 SX1262 is our primary sub-GHz emitter.
static const int8_t XR1_POWER_SUBGHZ_DBM[] = { -12, -9, -6, -2 };

// LNA gain for future RX work (Phase 3+ may use CAD or listen-before-talk).
static const uint8_t XR1_LNA_GAIN = 12;

// ============================================================================
// SPI timing
//
// Start conservative for first-bringup debugging. RadioLib default is 2 MHz
// and the LR1121 datasheet permits up to 16 MHz; raise this only after the
// 500 kHz bring-up succeeds on real hardware.
// ============================================================================
#define XR1_SPI_HZ_BRINGUP  500000UL
#define XR1_SPI_HZ_NORMAL   8000000UL
