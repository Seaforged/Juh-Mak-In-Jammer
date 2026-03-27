#ifndef CROSSFIRE_H
#define CROSSFIRE_H

#include <RadioLib.h>

// ============================================================
// TBS Crossfire 915 MHz FSK Simulation
// 85.1 kbaud FSK, 100 channels, 260 kHz spacing, 50 Hz hop rate
// ============================================================

struct CrossfireParams {
    float    currentMHz;
    uint8_t  channelIndex;
    uint32_t packetCount;
    uint32_t hopCount;
    int8_t   powerDbm;
    bool     running;
};

void crossfireInit(SX1262 *radio);
void crossfireStart();
void crossfireStop();
void crossfireUpdate();          // call every loop()
CrossfireParams crossfireGetParams();

#endif // CROSSFIRE_H
