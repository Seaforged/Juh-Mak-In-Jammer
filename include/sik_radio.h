#ifndef SIK_RADIO_H
#define SIK_RADIO_H

#include <RadioLib.h>

// ============================================================
// SiK Radio (RFD900/3DR) GFSK MAVLink Telemetry Simulation
// v2 ref §3.3 — 50 channels, GFSK FHSS+TDM, 915-928 MHz
// ============================================================

struct SikParams {
    float    currentMHz;
    uint8_t  channelIndex;
    uint32_t packetCount;
    uint32_t hopCount;
    float    airSpeedKbps;
    int8_t   powerDbm;
    bool     running;
};

void sikInit(SX1262 *radio);
void sikSetSpeed(uint8_t speedIndex);  // 0-3: indexes SIK_AIR_SPEEDS_KBPS
void sikStart();
void sikStop();
void sikUpdate();           // call every loop()
SikParams sikGetParams();

#endif // SIK_RADIO_H
