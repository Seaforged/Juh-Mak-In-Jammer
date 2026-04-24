#pragma once

// ============================================================================
// Remote ID emission from the XR1's ESP32C3 radios (NOT the LR1121). Three
// independent transports that can run simultaneously:
//
//   - remoteIdWifi  -> ASTM F3411-22a WiFi beacon frame with ODID vendor IE
//   - remoteIdBle   -> ASTM F3411-22a BLE 4 Legacy advertisement (UUID 0xFFFA)
//   - djiDroneId    -> DJI proprietary DroneID WiFi beacon (OUI 26:37:12)
//
// A fourth (remoteIdNan) is declared for API completeness but is a stub on
// ESP32C3 — NaN is not available in the Arduino-ESP32 WiFi build for this
// chip.
//
// Encoding uses the vendored opendroneid-core-c library (lib/opendroneid/)
// for the ASTM transports; DJI DroneID is hand-encoded per the RUB-SysSec
// NDSS 2023 paper and the Kismet Kaitai definition since no open library
// covers that format.
// ============================================================================

#include <Arduino.h>
#include <stdint.h>

struct RemoteIdState {
    char    serial[21];       // FAA serial number, 20 chars + NUL
    double  latitude;         // degrees (positive = N)
    double  longitude;        // degrees (positive = E)
    float   altitudeMeters;   // geodetic altitude MSL
    float   speedMps;         // horizontal speed
    float   headingDeg;       // true north, 0..359
};

struct RemoteIdStatus {
    bool     wifiActive;
    bool     bleActive;
    bool     djiActive;
    bool     nanActive;
    uint32_t wifiFrameCount;
    uint32_t bleFrameCount;
    uint32_t djiFrameCount;
};

// One-time init at boot: NVS + esp_netif + event loop. Does NOT start WiFi
// or BLE stacks — those spin up on first transport start and stay up.
void remoteIdInit();

// Call from loop() every iteration. Drives 1 Hz beacon cadence for each
// active transport. Non-blocking; returns fast.
void remoteIdUpdate();

// Stop all four transports cleanly and tear down radio stacks.
void remoteIdStopAll();

const RemoteIdStatus &remoteIdGetStatus();

// Per-transport control. Each returns true on successful stack bring-up.
bool remoteIdWifiStart(const RemoteIdState &state);
void remoteIdWifiStop();

bool remoteIdBleStart(const RemoteIdState &state);
void remoteIdBleStop();

bool djiDroneIdStart(const RemoteIdState &state, uint16_t modelCode);
void djiDroneIdStop();

// NaN is a stub on ESP32C3 — API kept for symmetry with the other transports.
// Returns false after logging a note so callers can report "not supported"
// without special-casing their own flow.
bool remoteIdNanStart(const RemoteIdState &state);
void remoteIdNanStop();
