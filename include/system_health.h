#ifndef SYSTEM_HEALTH_H
#define SYSTEM_HEALTH_H

#include <Arduino.h>

extern bool g_sx1262Failed;
extern bool g_oledFailed;
extern volatile bool g_sx1262Locked;

static inline bool sx1262ModeAvailable() {
    if (g_sx1262Failed) {
        Serial.println("[MODE] SX1262 not available");
        return false;
    }
    if (g_sx1262Locked) {
        Serial.println("[MODE] SX1262 locked by stuck ELRS task -- power cycle required");
        return false;
    }
    return true;
}

#endif // SYSTEM_HEALTH_H
