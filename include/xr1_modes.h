#ifndef XR1_MODES_H
#define XR1_MODES_H

// ============================================================================
// 2.4 GHz protocol profiles driven through the XR1's LR1121 over UART. The
// T3S3 builds a channel list + modulation config for each supported protocol
// and ships it to the XR1 via xr1_driver calls. The XR1 then runs the FHSS
// pattern autonomously until xr1Stop() is called.
//
// All start functions are non-blocking from the operator's perspective
// (they return as soon as the XR1 ACKs the config). The XR1 owns the actual
// hop scheduling; main-loop code here just reports current-mode info to the
// OLED.
// ============================================================================

#include <Arduino.h>
#include <stdint.h>

// Public API: start a specific protocol. Returns true if the XR1 accepted
// the configuration. If false, the XR1 is left stopped.
bool xr1ModeElrs2g4Start(uint8_t rateIdx);   // 0..3 -> 500/250/150/50 Hz
bool xr1ModeGhostStart();
bool xr1ModeFrskyStart();
bool xr1ModeFlyskyStart();
bool xr1ModeDjiEnergyStart();

// Generic configurable 2.4 GHz mode. The operator supplies the full spec
// on a single command line (see main.cpp x9 parser).
struct Xr1GenericCfg {
    bool    isLora;        // true=LoRa, false=GFSK
    uint8_t sf;            // LoRa SF (5..12)
    float   bwKhz;         // LoRa BW (203.125 / 406.25 / 812.5) or GFSK rx BW (unused for tx)
    uint8_t cr;            // LoRa CR denominator (5..8)
    float   brKbps;        // GFSK bitrate
    float   devKhz;        // GFSK deviation
    uint8_t channelCount;  // 1..80
    float   startMhz;
    float   spacingMhz;
    uint16_t dwellMs;
    int8_t  powerDbm;
};
bool xr1ModeGenericStart(const Xr1GenericCfg &cfg);

// Stop whatever XR1 mode is running. Safe to call when nothing is active.
void xr1ModesStop();

// OLED snapshot.
struct Xr1ModesStatus {
    bool        running;
    char        label[32];
    float       startMhz;
    float       stopMhz;
    uint8_t     channels;
    uint16_t    dwellMs;
    int8_t      powerDbm;
    const char *modulation;   // short text like "LoRa SF7" or "GFSK 250k"
};
Xr1ModesStatus xr1ModesGetStatus();

#endif // XR1_MODES_H
