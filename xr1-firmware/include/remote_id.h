#pragma once

// ============================================================================
// Remote ID emission (Phase 4).
//
// Two independent transports, both via the ESP32C3's own radios (the LR1121
// is not involved in WiFi/BLE Remote ID):
//   - ASTM F3411 WiFi beacon with ODID vendor-specific IE
//   - ASTM F3411 BLE 4 Legacy advertisement with Service UUID 0xFFFA
//
// DJI's proprietary DroneID is a third WiFi variant handled separately.
//
// Phase 1 only declares the public entry points; encoding uses the
// opendroneid-core-c library placed under lib/opendroneid/.
// ============================================================================

#include <Arduino.h>

struct RemoteIdState {
    char    serial[21];     // FAA serial number, 20 chars + NUL
    double  latitude;       // degrees
    double  longitude;      // degrees
    float   altitudeMeters;
    float   speedMps;
    float   headingDeg;
};

void remoteIdWifiBegin();
void remoteIdWifiStart(const RemoteIdState &state);
void remoteIdWifiStop();

void remoteIdBleBegin();
void remoteIdBleStart(const RemoteIdState &state);
void remoteIdBleStop();

void djiDroneIdStart(const RemoteIdState &state, uint16_t modelCode);
void djiDroneIdStop();
