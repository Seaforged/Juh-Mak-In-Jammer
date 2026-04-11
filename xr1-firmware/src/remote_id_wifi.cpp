#include "remote_id.h"

// TODO: ASTM F3411 WiFi beacon (Phase 4)
// Use ESP32C3's internal WiFi radio (NOT the LR1121) to broadcast ODID
// messages packed as a vendor-specific Information Element in standard
// 802.11 beacon frames. Message rotation: Basic ID → Location → System
// → Operator ID on channels 1/6/11.
// Encoding from lib/opendroneid/opendroneid.c.

void remoteIdWifiBegin() {}
void remoteIdWifiStart(const RemoteIdState &state) { (void)state; }
void remoteIdWifiStop() {}
