#ifndef CROSSFIRE_H
#define CROSSFIRE_H

#include <RadioLib.h>

// ============================================================
// TBS Crossfire 915 MHz FSK Simulation
// 85.1 kbaud FSK, 100 channels, 260 kHz spacing, 50 Hz hop rate
// ============================================================

struct CrossfireParams {
    float       currentMHz;
    uint8_t     channelIndex;
    uint32_t    packetCount;
    uint32_t    hopCount;
    int8_t      powerDbm;
    bool        running;
    bool        isLoRa;
    const char *bandName;
};

void crossfireInit(SX1262 *radio);
void crossfireSetBand(uint8_t bandIdx);  // 0=CRSF_BAND_915, 1=CRSF_BAND_868
void crossfireStart();                    // FSK 150 Hz (backward compatible)
void crossfireStartLoRa();               // LoRa 50 Hz (new)
void crossfireStop();
void crossfireUpdate();                   // call every loop()
CrossfireParams crossfireGetParams();

#endif // CROSSFIRE_H
