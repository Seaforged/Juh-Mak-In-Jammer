// ============================================================================
// ASTM F3411-22a WiFi beacon emission on the XR1's ESP32C3 WiFi radio.
// ODID Message Pack (Basic ID + Location + System + Operator ID) wrapped in
// the ASTM vendor-specific Information Element (OUI FA:0B:BC, type 0x0D)
// inside a standard 802.11 beacon frame. One beacon per second, channel
// rotation 1 -> 6 -> 11 every 3 seconds.
//
// Encoding is delegated to the vendored opendroneid-core-c library; we only
// marshal the application-level drone state into its ODID_*_data structs
// and call odid_message_build_pack() to get the wire bytes.
// ============================================================================

#include "remote_id.h"

#include <Arduino.h>
#include <string.h>
#include <time.h>

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"

extern "C" {
    #include "opendroneid.h"
}

// ----- shared status + init flags ------------------------------------------
// Cross-TU symbol used by all remote-id transports to flip their active flags
// without each file carrying its own copy. Defined in remote_id_common.cpp.
extern RemoteIdStatus g_ridStatus;
extern bool           g_ridCommonInitDone;
void                  ridCommonInit();   // one-time NVS / netif / event loop

// ----- WiFi stack bring-up state -------------------------------------------
static bool s_wifiStackUp = false;

static bool bringUpWifiIfNeeded() {
    if (s_wifiStackUp) return true;
    ridCommonInit();   // NVS + esp_netif + event loop

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    if (esp_wifi_init(&cfg) != ESP_OK)                                return false;
    if (esp_wifi_set_storage(WIFI_STORAGE_RAM) != ESP_OK)             return false;
    if (esp_wifi_set_mode(WIFI_MODE_STA) != ESP_OK)                   return false;
    // Enable 11b/g/n so beacon TX works on channels 1/6/11.
    esp_wifi_set_protocol(WIFI_IF_STA,
        WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
    if (esp_wifi_start() != ESP_OK)                                   return false;
    s_wifiStackUp = true;
    return true;
}

// Called by DJI transport too — exposed via extern "C" symbol so dji_droneid.cpp
// can reuse the same WiFi stack without duplicating init logic.
extern "C" bool ridWifiStackUp()     { return s_wifiStackUp; }
extern "C" bool ridWifiStackBringUp() { return bringUpWifiIfNeeded(); }

// Number of currently-active WiFi-consuming transports (ODID beacon + DJI).
// We don't tear down the stack until both have stopped.
static int s_wifiRefCount = 0;
extern "C" void ridWifiRefUp()   { ++s_wifiRefCount; }
extern "C" void ridWifiRefDown() {
    if (s_wifiRefCount > 0) --s_wifiRefCount;
    if (s_wifiRefCount == 0 && s_wifiStackUp) {
        esp_wifi_stop();
        esp_wifi_deinit();
        s_wifiStackUp = false;
    }
}

// ----- ODID state building -------------------------------------------------
static RemoteIdState s_state = {};

static void fillBasicId(ODID_BasicID_data &b) {
    memset(&b, 0, sizeof(b));
    b.UAType = ODID_UATYPE_HELICOPTER_OR_MULTIROTOR;
    b.IDType = ODID_IDTYPE_SERIAL_NUMBER;
    strncpy(b.UASID, s_state.serial, sizeof(b.UASID) - 1);
}

static void fillLocation(ODID_Location_data &l) {
    memset(&l, 0, sizeof(l));
    l.Status           = ODID_STATUS_AIRBORNE;
    l.Direction        = s_state.headingDeg;
    l.SpeedHorizontal  = s_state.speedMps;
    l.SpeedVertical    = 0.0f;
    l.Latitude         = s_state.latitude;
    l.Longitude        = s_state.longitude;
    l.AltitudeBaro     = s_state.altitudeMeters;
    l.AltitudeGeo      = s_state.altitudeMeters;
    l.HeightType       = ODID_HEIGHT_REF_OVER_TAKEOFF;
    l.Height           = 50.0f;
    l.HorizAccuracy    = ODID_HOR_ACC_10_METER;
    l.VertAccuracy     = ODID_VER_ACC_10_METER;
    l.BaroAccuracy     = ODID_VER_ACC_10_METER;
    l.SpeedAccuracy    = ODID_SPEED_ACC_1_METERS_PER_SECOND;
    l.TSAccuracy       = ODID_TIME_ACC_1_0_SECOND;
    // Tenths of a second since the top of the UTC hour — derived from millis()
    // so the value monotonically increments during a run without needing an
    // RTC. Good enough for protocol-level validity.
    l.TimeStamp = (float)((millis() / 100) % 36000) / 10.0f;
}

static void fillSystem(ODID_System_data &sy) {
    memset(&sy, 0, sizeof(sy));
    sy.OperatorLocationType = ODID_OPERATOR_LOCATION_TYPE_TAKEOFF;
    sy.ClassificationType   = ODID_CLASSIFICATION_TYPE_UNDECLARED;
    sy.OperatorLatitude     = s_state.latitude;
    sy.OperatorLongitude    = s_state.longitude;
    sy.AreaCount            = 1;
    sy.AreaRadius           = 0;
    sy.AreaCeiling          = -1000.0f;   // invalid sentinel
    sy.AreaFloor            = -1000.0f;
    sy.OperatorAltitudeGeo  = s_state.altitudeMeters - 5.0f;
    sy.Timestamp            = 0;          // unknown; receivers tolerate 0
}

static void fillOperator(ODID_OperatorID_data &o) {
    memset(&o, 0, sizeof(o));
    o.OperatorIdType = ODID_OPERATOR_ID;
    snprintf(o.OperatorId, sizeof(o.OperatorId), "OP-%s", s_state.serial);
    o.OperatorId[sizeof(o.OperatorId) - 1] = '\0';
}

// Encode each ODID message individually and concatenate with a 3-byte pack
// header. We hand-roll this because the vendored opendroneid.c does not
// define odid_message_build_pack() even though the header declares it;
// encodeMessagePack() exists but adds a different framing we don't want.
//
// Pack layout:
//   [0] = (0xF << 4) | ODID_PROTOCOL_VERSION   -- message type 0xF = pack
//   [1] = ODID_MESSAGE_SIZE (25)
//   [2] = number of messages (4 here)
//   [3..102] = 4 × 25-byte encoded messages
static int buildMessagePack(uint8_t *packOut, size_t packMax) {
    const size_t needed = 3 + 4 * ODID_MESSAGE_SIZE;
    if (packMax < needed) return -1;

    ODID_BasicID_data   b;  fillBasicId(b);
    ODID_Location_data  l;  fillLocation(l);
    ODID_System_data    sy; fillSystem(sy);
    ODID_OperatorID_data op; fillOperator(op);

    ODID_BasicID_encoded    b_enc;
    ODID_Location_encoded   l_enc;
    ODID_System_encoded     sy_enc;
    ODID_OperatorID_encoded op_enc;

    if (encodeBasicIDMessage(&b_enc, &b)      != ODID_SUCCESS) return -1;
    if (encodeLocationMessage(&l_enc, &l)     != ODID_SUCCESS) return -1;
    if (encodeSystemMessage(&sy_enc, &sy)     != ODID_SUCCESS) return -1;
    if (encodeOperatorIDMessage(&op_enc, &op) != ODID_SUCCESS) return -1;

    packOut[0] = (uint8_t)((0xF << 4) | ODID_PROTOCOL_VERSION);
    packOut[1] = ODID_MESSAGE_SIZE;
    packOut[2] = 4;
    memcpy(packOut + 3 + 0 * ODID_MESSAGE_SIZE, &b_enc,  ODID_MESSAGE_SIZE);
    memcpy(packOut + 3 + 1 * ODID_MESSAGE_SIZE, &l_enc,  ODID_MESSAGE_SIZE);
    memcpy(packOut + 3 + 2 * ODID_MESSAGE_SIZE, &sy_enc, ODID_MESSAGE_SIZE);
    memcpy(packOut + 3 + 3 * ODID_MESSAGE_SIZE, &op_enc, ODID_MESSAGE_SIZE);
    return (int)needed;
}

// ----- 802.11 beacon frame assembly ----------------------------------------
static constexpr uint8_t BEACON_HEADER[] = {
    0x80, 0x00,                          // Frame Control: beacon, toDS=0 fromDS=0
    0x00, 0x00,                          // Duration
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // Destination (broadcast)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Source (filled in at boot)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // BSSID (same as source)
    0x00, 0x00,                          // Sequence control (HW will fill)
    // Fixed params: timestamp (8) + interval (2) + capability (2) = 12 bytes
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x64, 0x00,                          // Beacon interval 100 TU
    0x01, 0x00,                          // Capability: ESS
    // SSID IE (hidden, length 0)
    0x00, 0x00,
};
static constexpr size_t BEACON_HEADER_SZ = sizeof(BEACON_HEADER);

static constexpr uint8_t ODID_OUI[3] = { 0xFA, 0x0B, 0xBC };
static constexpr uint8_t ODID_OUI_TYPE = 0x0D;

static uint8_t s_srcMac[6] = { 0x02, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE };

static uint8_t s_frame[256];
static size_t  s_frameLen = 0;

// Rebuild the full beacon frame with the latest ODID pack. Called on every
// 1 Hz tick so location timestamps stay fresh.
static void rebuildFrame() {
    memcpy(s_frame, BEACON_HEADER, BEACON_HEADER_SZ);
    memcpy(s_frame + 10, s_srcMac, 6);   // source
    memcpy(s_frame + 16, s_srcMac, 6);   // BSSID

    // Encode ODID pack into scratch buffer. Need 3 header + 4×25 = 103 bytes.
    uint8_t pack[3 + ODID_MESSAGE_SIZE * 4];
    const int packLen = buildMessagePack(pack, sizeof(pack));
    if (packLen <= 0) { s_frameLen = 0; return; }

    size_t pos = BEACON_HEADER_SZ;
    // Vendor IE: element ID 0xDD, length, OUI, OUI type, pack
    s_frame[pos++] = 0xDD;
    s_frame[pos++] = (uint8_t)(3 + 1 + packLen);  // IE length
    s_frame[pos++] = ODID_OUI[0];
    s_frame[pos++] = ODID_OUI[1];
    s_frame[pos++] = ODID_OUI[2];
    s_frame[pos++] = ODID_OUI_TYPE;
    memcpy(s_frame + pos, pack, packLen);
    pos += packLen;

    s_frameLen = pos;
}

// ----- 1 Hz scheduler ------------------------------------------------------
// Channel rotation lives in remote_id_common.cpp (ridWifiChanTick) — both
// ODID and DJI share it so they don't fight over esp_wifi_set_channel. We
// just transmit on whatever channel the controller has picked.
static uint32_t s_lastTxMs = 0;

// Called from remoteIdUpdate().
extern "C" void ridWifiTick() {
    if (!g_ridStatus.wifiActive)    return;
    const uint32_t now = millis();
    if (now - s_lastTxMs < 1000) return;
    s_lastTxMs = now;

    rebuildFrame();
    if (s_frameLen == 0) return;
    if (esp_wifi_80211_tx(WIFI_IF_STA, s_frame, s_frameLen, true) == ESP_OK) {
        ++g_ridStatus.wifiFrameCount;
    }
}

// ----- public API ----------------------------------------------------------
bool remoteIdWifiStart(const RemoteIdState &state) {
    if (g_ridStatus.wifiActive) return true;
    s_state = state;

    if (!bringUpWifiIfNeeded()) {
        Serial.println("[XR1-RID] WiFi stack init failed");
        return false;
    }
    ridWifiRefUp();

    // Build an LAA MAC derived from the serial so multiple XR1s don't clash.
    // Bit 1 of the first byte = locally administered; bit 0 = unicast.
    uint32_t hash = 0x811c9dc5u;
    for (const char *p = s_state.serial; *p; ++p) {
        hash = (hash ^ (uint8_t)*p) * 0x01000193u;
    }
    s_srcMac[0] = 0x02;
    s_srcMac[1] = (hash >> 24) & 0xFF;
    s_srcMac[2] = (hash >> 16) & 0xFF;
    s_srcMac[3] = (hash >>  8) & 0xFF;
    s_srcMac[4] = (hash      ) & 0xFF;
    s_srcMac[5] = 0x01;

    // Channel selection is owned by the unified controller in
    // remote_id_common.cpp; no set_channel call here (see H1 fix).
    s_lastTxMs = 0;

    g_ridStatus.wifiActive      = true;
    g_ridStatus.wifiFrameCount  = 0;
    Serial.printf("[XR1-RID] WiFi beacon ON: %s @ %.4f,%.4f alt=%.1f\n",
                  s_state.serial, s_state.latitude, s_state.longitude,
                  s_state.altitudeMeters);
    return true;
}

void remoteIdWifiStop() {
    if (!g_ridStatus.wifiActive) return;
    g_ridStatus.wifiActive = false;
    ridWifiRefDown();
    Serial.printf("[XR1-RID] WiFi beacon OFF (%u frames sent)\n",
                  (unsigned)g_ridStatus.wifiFrameCount);
}
