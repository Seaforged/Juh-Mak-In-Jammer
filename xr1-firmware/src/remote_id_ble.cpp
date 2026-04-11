#include "remote_id.h"

// TODO: ASTM F3411 BLE advertisement (Phase 4)
// BLE 4 Legacy ADV_NONCONN_IND with Service Data (AD type 0x16), UUID 0xFFFA.
// 25-byte ODID message per advertisement, rotating through Basic ID / Location
// / System / Operator ID. BLE 5 Coded PHY is a stretch goal.

void remoteIdBleBegin() {}
void remoteIdBleStart(const RemoteIdState &state) { (void)state; }
void remoteIdBleStop() {}
