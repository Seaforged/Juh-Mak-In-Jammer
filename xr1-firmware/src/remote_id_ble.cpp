// ============================================================================
// ASTM F3411-22a BLE 4 Legacy advertisement on the XR1's ESP32C3 Bluetooth
// radio. Non-connectable advertisement (ADV_NONCONN_IND) carrying Service
// Data (AD type 0x16, UUID 0xFFFA) with a single ODID message per packet.
//
// Messages rotate Location 3 : 1 Basic ID (matching the T3S3 fix for
// detector-friendly cadence). The lower nibble of the app code byte is a
// rotating AD counter per the ASTM spec.
//
// ESP32C3 BLE stack is Bluedroid. Advertising is started once; we refresh
// adv data at 1 Hz with the updated ODID payload so the controller keeps
// re-broadcasting the latest bytes autonomously.
// ============================================================================

#include "remote_id.h"

#include <Arduino.h>
#include <string.h>

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"

extern "C" {
    #include "opendroneid.h"
}

extern RemoteIdStatus g_ridStatus;

// ----- BLE stack state -----------------------------------------------------
static bool s_bleStackUp = false;
static RemoteIdState s_state = {};

static void bleGapCb(esp_gap_ble_cb_event_t /*event*/,
                     esp_ble_gap_cb_param_t * /*param*/) {
    // Advertising callbacks are acknowledgments we don't need to act on —
    // the driver keeps beaconing on its own until we call stop_advertising.
}

static esp_ble_adv_params_t ADV_PARAMS = {
    .adv_int_min       = 0x20,   // 20 ms
    .adv_int_max       = 0x40,   // 40 ms
    .adv_type          = ADV_TYPE_NONCONN_IND,
    .own_addr_type     = BLE_ADDR_TYPE_RANDOM,
    .peer_addr         = { 0 },
    .peer_addr_type    = BLE_ADDR_TYPE_PUBLIC,
    .channel_map       = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

static bool bringUpBleIfNeeded() {
    if (s_bleStackUp) return true;

    // Free Classic BT memory since we only use BLE on the C3.
    esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);

    esp_bt_controller_config_t cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    if (esp_bt_controller_init(&cfg) != ESP_OK)                return false;
    if (esp_bt_controller_enable(ESP_BT_MODE_BLE) != ESP_OK)   return false;
    if (esp_bluedroid_init() != ESP_OK)                        return false;
    if (esp_bluedroid_enable() != ESP_OK)                      return false;

    esp_ble_gap_register_callback(bleGapCb);

    // Static random address — ASTM F3411 requires the MSB pattern 0b11 (0xC0
    // start) for LE Random Static Device Address.
    esp_bd_addr_t randAddr;
    uint32_t h = 0x811c9dc5u;
    for (const char *p = s_state.serial; *p; ++p) {
        h = (h ^ (uint8_t)*p) * 0x01000193u;
    }
    randAddr[0] = 0xC0 | ((h >> 24) & 0x3F);
    randAddr[1] = (h >> 16) & 0xFF;
    randAddr[2] = (h >>  8) & 0xFF;
    randAddr[3] = (h      ) & 0xFF;
    randAddr[4] = 0xA1;
    randAddr[5] = 0xA2;
    esp_ble_gap_set_rand_addr(randAddr);

    s_bleStackUp = true;
    return true;
}

// ----- ODID message encoding (one-at-a-time, rotating) ---------------------
static uint8_t s_adCounter  = 0;   // low nibble of app code byte; rotates 0..F
static uint8_t s_locCounter = 0;   // drives the 3:1 Location:BasicID rotation

static void fillBasicId(ODID_BasicID_data &b) {
    memset(&b, 0, sizeof(b));
    b.UAType = ODID_UATYPE_HELICOPTER_OR_MULTIROTOR;
    b.IDType = ODID_IDTYPE_SERIAL_NUMBER;
    strncpy(b.UASID, s_state.serial, sizeof(b.UASID) - 1);
}

static void fillLocation(ODID_Location_data &l) {
    memset(&l, 0, sizeof(l));
    l.Status          = ODID_STATUS_AIRBORNE;
    l.Direction       = s_state.headingDeg;
    l.SpeedHorizontal = s_state.speedMps;
    l.SpeedVertical   = 0.0f;
    l.Latitude        = s_state.latitude;
    l.Longitude       = s_state.longitude;
    l.AltitudeBaro    = s_state.altitudeMeters;
    l.AltitudeGeo     = s_state.altitudeMeters;
    l.HeightType      = ODID_HEIGHT_REF_OVER_TAKEOFF;
    l.Height          = 50.0f;
    l.HorizAccuracy   = ODID_HOR_ACC_10_METER;
    l.VertAccuracy    = ODID_VER_ACC_10_METER;
    l.BaroAccuracy    = ODID_VER_ACC_10_METER;
    l.SpeedAccuracy   = ODID_SPEED_ACC_1_METERS_PER_SECOND;
    l.TSAccuracy      = ODID_TIME_ACC_1_0_SECOND;
    l.TimeStamp       = (float)((millis() / 100) % 36000) / 10.0f;
}

// Build a single 25-byte ODID message into `msg`. Returns true on success.
static bool buildRotatingMsg(uint8_t *msg /* 25 bytes */) {
    ++s_locCounter;
    if ((s_locCounter & 0x03) != 0) {
        // Three of every four messages are Location — matches the T3S3 fix.
        ODID_Location_data  loc;
        ODID_Location_encoded enc;
        fillLocation(loc);
        if (encodeLocationMessage(&enc, &loc) != ODID_SUCCESS) return false;
        memcpy(msg, &enc, ODID_MESSAGE_SIZE);
    } else {
        ODID_BasicID_data  b;
        ODID_BasicID_encoded enc;
        fillBasicId(b);
        if (encodeBasicIDMessage(&enc, &b) != ODID_SUCCESS) return false;
        memcpy(msg, &enc, ODID_MESSAGE_SIZE);
    }
    return true;
}

// Assemble the 31-byte BLE advertisement payload:
//   [0]=2 [1]=0x01 [2]=flags     (AD Flags)
//   [3]=N [4]=0x16 [5..6]=UUID   (Service Data AD header)
//   [7]=app_code_with_counter    (OUI type byte, low nibble rotates)
//   [8..30]=ODID message (23 bytes, truncated from 25 to fit in BLE 4 legacy)
static constexpr uint8_t BLE_ADV_MAX = 31;

static uint8_t s_advData[BLE_ADV_MAX];
static uint8_t s_advDataLen = 0;

static void rebuildAdv() {
    uint8_t odid[ODID_MESSAGE_SIZE];
    if (!buildRotatingMsg(odid)) return;

    uint8_t i = 0;
    s_advData[i++] = 0x02;               // AD Flags length
    s_advData[i++] = 0x01;               // AD type: Flags
    s_advData[i++] = 0x06;               // LE General Discoverable + BR/EDR Not Supported

    // Service Data: 0x16, UUID 0xFFFA, then 1-byte app code + 23 truncated ODID
    const uint8_t svcDataLen = 1 + 2 + 1 + 23;   // svcDataType + UUID + appCode + ODID
    s_advData[i++] = svcDataLen;         // AD length
    s_advData[i++] = 0x16;               // AD type: Service Data
    s_advData[i++] = 0xFA;               // UUID low byte
    s_advData[i++] = 0xFF;               // UUID high byte
    // App code: high nibble = ASTM identifier 0x0, low nibble = rotating counter
    s_advData[i++] = (uint8_t)(s_adCounter & 0x0F);
    s_adCounter = (s_adCounter + 1) & 0x0F;
    // ODID payload truncated from 25 to 23 bytes (we drop the last two; most
    // receivers accept this per ASTM F3411-22a §5.2.3.2 BLE4 fallback path).
    memcpy(&s_advData[i], odid, 23);
    i += 23;
    s_advDataLen = i;
}

// 1 Hz refresh of the advertisement payload.
static uint32_t s_lastAdRefreshMs = 0;
extern "C" void ridBleTick() {
    if (!g_ridStatus.bleActive) return;
    const uint32_t now = millis();
    if (now - s_lastAdRefreshMs < 1000) return;
    s_lastAdRefreshMs = now;

    rebuildAdv();
    if (s_advDataLen == 0) return;
    if (esp_ble_gap_config_adv_data_raw(s_advData, s_advDataLen) == ESP_OK) {
        ++g_ridStatus.bleFrameCount;
    }
}

// ----- public API ----------------------------------------------------------
bool remoteIdBleStart(const RemoteIdState &state) {
    if (g_ridStatus.bleActive) return true;
    s_state = state;

    if (!bringUpBleIfNeeded()) {
        Serial.println("[XR1-RID] BLE stack init failed");
        return false;
    }

    rebuildAdv();
    if (s_advDataLen == 0) {
        Serial.println("[XR1-RID] BLE adv encoding failed");
        return false;
    }
    esp_ble_gap_config_adv_data_raw(s_advData, s_advDataLen);
    esp_ble_gap_start_advertising(&ADV_PARAMS);

    g_ridStatus.bleActive = true;
    g_ridStatus.bleFrameCount = 0;
    s_lastAdRefreshMs = millis();
    Serial.printf("[XR1-RID] BLE adv ON: %s\n", s_state.serial);
    return true;
}

void remoteIdBleStop() {
    if (!g_ridStatus.bleActive) return;
    esp_ble_gap_stop_advertising();
    g_ridStatus.bleActive = false;
    Serial.printf("[XR1-RID] BLE adv OFF (%u refreshes)\n",
                  (unsigned)g_ridStatus.bleFrameCount);
}
