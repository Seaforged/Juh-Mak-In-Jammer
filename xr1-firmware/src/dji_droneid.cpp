#include "remote_id.h"

// TODO: DJI DroneID WiFi beacon (Phase 4)
// Proprietary WiFi beacon with DJI vendor-specific IE (OUI 0x26 0x37 0x12).
// Field layout must match SENTRY-RF's remote_id_parser.h so we can validate
// our own detector end-to-end by emitting here and parsing there.

void djiDroneIdStart(const RemoteIdState &state, uint16_t modelCode) {
    (void)state;
    (void)modelCode;
}

void djiDroneIdStop() {}
