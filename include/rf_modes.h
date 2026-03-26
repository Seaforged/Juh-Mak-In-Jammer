#ifndef RF_MODES_H
#define RF_MODES_H

#include <RadioLib.h>

// ============================================================
// RF Signal Generator Modes
// ============================================================

// CW tone parameters (read-only snapshot for display)
struct CwParams {
    float freqMHz;
    int8_t powerDbm;
    bool transmitting;
};

// --- CW Tone API ---
void cwInit(SX1262 *radio);   // call once from setup()
void cwStart();                // begin transmitting CW carrier
void cwStop();                 // return radio to standby
void cwCycleFreq();            // advance to next preset frequency
CwParams cwGetParams();        // get current state for display

// Sweep parameters (read-only snapshot for display)
struct SweepParams {
    float startMHz;
    float endMHz;
    float stepMHz;
    float currentMHz;
    uint16_t dwellUs;
    uint16_t stepIndex;
    uint16_t totalSteps;
    int8_t powerDbm;
    bool running;
};

// --- Frequency Sweep API ---
void sweepStart();             // begin sweeping 860→930 MHz
void sweepStop();              // stop sweep and return to standby
void sweepUpdate();            // call every loop() — advances one step when dwell elapses
void sweepCycleStep();         // cycle through step size presets
void sweepCycleDwell();        // cycle through dwell time presets
SweepParams sweepGetParams();  // get current state for display

// --- Shared Power Control ---
void rfCyclePower();           // cycle TX power preset across all modes
int8_t rfGetPower();           // current power setting in dBm

// ELRS simulation parameters (read-only snapshot for display)
struct ElrsParams {
    float currentMHz;
    uint8_t channelIndex;
    uint32_t packetCount;
    uint32_t hopCount;
    int8_t powerDbm;
    bool running;
};

// --- ELRS 915 Simulation API ---
void elrsStart();              // begin FHSS transmission
void elrsStop();               // stop and return to standby
void elrsUpdate();             // call every loop() — handles hop timing
ElrsParams elrsGetParams();    // get current state for display

#endif // RF_MODES_H
