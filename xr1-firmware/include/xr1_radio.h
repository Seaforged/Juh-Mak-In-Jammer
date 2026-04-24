#pragma once

// ============================================================================
// LR1121 wrapper — thin interface around RadioLib. Low-level radio operations
// plus a WS2812 LED state renderer. Higher-level command parsing lives in
// xr1_uart.cpp; this file keeps SPI/RadioLib calls in one place.
// ============================================================================

#include <Arduino.h>
#include <stdint.h>

enum Xr1Modulation {
    XR1_MOD_LORA = 0,
    XR1_MOD_GFSK = 1,
};

enum Xr1LedMode {
    XR1_LED_AUTO  = 0,   // LED follows radio state
    XR1_LED_OFF   = 1,
    XR1_LED_RED   = 2,
    XR1_LED_GREEN = 3,
    XR1_LED_BLUE  = 4,
};

struct Xr1RadioStatus {
    bool     initialized;
    uint32_t hwVersion;      // LR1121 HW/FW version word (0 if unread)
    int16_t  lastError;      // RadioLib error code from last op
    bool     subGhzOk;       // 915 MHz test TX succeeded
    bool     twoGhzOk;       // 2440 MHz test TX succeeded

    // Operational state, updated by xr1RadioSet*/xr1RadioTransmit
    float        freqMhz;
    Xr1Modulation mod;
    int8_t       powerDbm;
    uint8_t      sf;         // LoRa spreading factor
    float        bwKhz;      // LoRa bandwidth
    uint8_t      cr;         // LoRa coding rate denominator (5..8)
    float        brKbps;     // GFSK bitrate
    float        devKhz;     // GFSK frequency deviation
};

// Bring up SPI, reset LR1121, call begin() with the 3.0 V TCXO assumption,
// then apply the hardware.json RF switch table. Returns true on success.
bool xr1RadioBegin();

// Transmit a small LoRa packet on 915.0 MHz, then switch to 2440.0 MHz and
// transmit again. Populates the status struct. Returns true if both TXs
// reported RADIOLIB_ERR_NONE.
bool xr1RadioHelloSelfTest();

// Snapshot current radio status (thread-unsafe; main loop only).
const Xr1RadioStatus &xr1RadioGetStatus();

// Operational setters — each returns a RadioLib error code (0 = OK).
int16_t xr1RadioSetFrequency(float mhz);
int16_t xr1RadioSetLoRa(uint8_t sf, float bwKhz, uint8_t cr);
int16_t xr1RadioSetFSK(float bitrateKbps, float devKhz);
int16_t xr1RadioSetPower(int8_t dbm);
int16_t xr1RadioTransmit(const uint8_t *data, size_t len);

// LoRa header / preamble knobs for protocols like ELRS that need implicit
// headers and short preambles. Call after xr1RadioSetLoRa to override the
// defaults set by loraBegin (preamble=8, explicit header).
int16_t xr1RadioSetPreamble(uint16_t symbols);
int16_t xr1RadioSetImplicitHeader(uint8_t payloadLen);
int16_t xr1RadioSetExplicitHeader();

// Hardware-reset the LR1121 and re-init at the currently configured settings.
// Returns 0 on success, RadioLib error code otherwise.
int16_t xr1RadioReset();

// -------- LED --------
// Render the WS2812 according to override mode and recent TX activity.
// Idempotent and cheap; call ~20 Hz from loop().
void xr1LedUpdate();

// Force a colour override or return to auto.
void xr1LedSetOverride(Xr1LedMode mode);
