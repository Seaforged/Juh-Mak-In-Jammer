#ifndef MLRS_SIM_H
#define MLRS_SIM_H

#include <RadioLib.h>

// ============================================================
// mLRS (MAVLink Long Range System) Simulation
// v2 ref §3.4 — Slow LoRa/FSK FHSS, symmetric TX/RX alternation
// Tests the lower bound of SENTRY-RF's frequency diversity detector
// ============================================================

struct MlrsParams {
    float    currentMHz;
    uint8_t  channelIndex;
    uint32_t packetCount;
    uint32_t hopCount;
    uint16_t rateHz;
    bool     isLoRa;
    int8_t   powerDbm;
    bool     running;
};

void mlrsInit(SX1262 *radio);
void mlrsSetMode(uint8_t modeIndex);  // 0-2: indexes MLRS_MODES
void mlrsStart();
void mlrsStop();
void mlrsUpdate();          // call every loop()
MlrsParams mlrsGetParams();

#endif // MLRS_SIM_H
