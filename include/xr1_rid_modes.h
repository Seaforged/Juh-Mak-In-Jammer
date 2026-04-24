#ifndef XR1_RID_MODES_H
#define XR1_RID_MODES_H

// ============================================================================
// T3S3-side wrapper for the XR1's Remote ID emission (WiFi beacon, BLE 4
// advertisement, DJI DroneID, WiFi NaN). Sends RID commands over the
// existing UART link and tracks which transports are active so the OLED
// renderer can show status.
//
// Default drone parameters (Virginia Beach test coords, matching the T3S3's
// own rid_spoofer defaults) are used when the operator doesn't override.
// ============================================================================

#include <Arduino.h>
#include <stdint.h>

// Bitmask of active XR1 RID transports.
enum Xr1RidMask : uint8_t {
    XR1_RID_WIFI = 0x01,
    XR1_RID_BLE  = 0x02,
    XR1_RID_DJI  = 0x04,
    XR1_RID_NAN  = 0x08,
    XR1_RID_ALL  = XR1_RID_WIFI | XR1_RID_BLE | XR1_RID_DJI,  // excl. NaN on C3
};

// Start one or more RID transports on the XR1 using default drone params.
// Returns true if at least one transport came up cleanly.
bool xr1RidStart(uint8_t mask);

// Stop all XR1 RID emission.
void xr1RidStop();

// Snapshot for OLED.
struct Xr1RidStatus {
    bool     running;
    uint8_t  activeMask;
    char     serial[24];
    double   latitude;
    double   longitude;
    float    altitudeMeters;
};
Xr1RidStatus xr1RidGetStatus();

#endif // XR1_RID_MODES_H
