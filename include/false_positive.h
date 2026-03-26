#ifndef FALSE_POSITIVE_H
#define FALSE_POSITIVE_H

#include <RadioLib.h>

// ============================================================
// Mode 3: False Positive Generator
// Generates ISM signals that are NOT drones — tests SENTRY-RF's
// ability to tell the difference between IoT and threat signals.
// ============================================================

// Sub-mode selection
enum FpMode {
    FP_LORAWAN = 0,
    FP_ISM_BURST,
    FP_MIXED,       // LoRaWAN background + ELRS foreground
    FP_BACK,
    FP_COUNT
};

// Status snapshot for OLED display
struct FpParams {
    FpMode mode;
    uint32_t loraPacketCount;    // LoRaWAN packets sent
    uint32_t burstCount;         // ISM bursts sent
    uint32_t elrsPacketCount;    // ELRS packets (mixed mode only)
    float lastFreqMHz;           // last TX frequency
    uint8_t lastSF;              // last spreading factor used
    int8_t powerDbm;
    bool running;
};

// --- Public API ---
void fpInit(SX1262 *radio);
void fpStart(FpMode mode);
void fpStop();
void fpUpdate();              // call every loop() iteration
FpParams fpGetParams();

#endif // FALSE_POSITIVE_H
