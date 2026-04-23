#pragma once

// ============================================================================
// LR1121 wrapper — thin Phase 1 interface around RadioLib.
// Later phases will expand this into a full protocol-aware emitter; for now
// it just handles init, a two-band self-test, and status queries so main.cpp
// can report "XR1 READY" (or fail cleanly) and Phase 2 can bolt UART commands
// on top.
// ============================================================================

#include <Arduino.h>
#include <stdint.h>

struct Xr1RadioStatus {
    bool     initialized;
    uint32_t hwVersion;      // LR1121 HW/FW version word (0 if unread)
    int16_t  lastError;      // RadioLib error code from last op
    bool     subGhzOk;       // 915 MHz test TX succeeded
    bool     twoGhzOk;       // 2440 MHz test TX succeeded
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

// 1 Hz heartbeat for the RGB LED — called from loop(). Implementation just
// toggles the GPIO; the WS2812 colour engine lands in a later phase.
void xr1RadioLedHeartbeat();
