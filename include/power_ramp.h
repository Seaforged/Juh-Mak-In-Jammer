#ifndef POWER_RAMP_H
#define POWER_RAMP_H

#include <RadioLib.h>

// ============================================================
// Power Ramp — simulates drone approach/departure via ELRS FHSS
// with linearly increasing then decreasing TX power.
// ============================================================

struct PowerRampParams {
    float    currentMHz;
    int8_t   currentPowerDbm;
    uint32_t packetCount;
    uint32_t elapsedSec;
    uint32_t rampDurationSec;
    bool     ascending;       // true = approaching, false = departing
    bool     running;
};

void powerRampInit(SX1262 *radio);
void powerRampStart();
void powerRampStop();
void powerRampUpdate();          // call every loop()
void powerRampCycleDuration();   // cycle ramp duration: 30/60/120/300 sec
PowerRampParams powerRampGetParams();

#endif // POWER_RAMP_H
