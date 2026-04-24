// ============================================================================
// ASTM F3411-22a BLE 4 Legacy advertisement on the XR1's ESP32C3 Bluetooth
// radio. Non-connectable advertisement (ADV_NONCONN_IND) carrying Service
// Data (AD type 0x16, UUID 0xFFFA) with a single ODID message per packet.
//
// Messages rotate Location x3, then Basic ID, System, and Operator ID. The
// lower nibble of the app code byte is a rotating AD counter per message type.
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
static volatile bool s_advConfigPending = false;
static volatile bool s_advStartPending = false;
static volatile bool s_advRunning = false;

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

static void bleGapCb(esp_gap_ble_cb_event_t event,
                     esp_ble_gap_cb_param_t *param) {
    switch (event) {
        case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
            s_advConfigPending = false;
            if (param->adv_data_raw_cmpl.status != ESP_BT_STATUS_SUCCESS) {
                Serial.printf("[XR1-RID] BLE raw adv config failed: %d\n",
                              (int)param->adv_data_raw_cmpl.status);
                s_advStartPending = false;
                if (!s_advRunning) g_ridStatus.bleActive = false;
                break;
            }
            if (!s_advRunning) s_advStartPending = true;
            break;
        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
            if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
                Serial.printf("[XR1-RID] BLE adv start failed: %d\n",
                              (int)param->adv_start_cmpl.status);
                s_advRunning = false;
                g_ridStatus.bleActive = false;
            } else {
                s_advRunning = true;
                g_ridStatus.bleActive = true;
            }
            break;
        case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
            if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS) {
                Serial.printf("[XR1-RID] BLE adv stop failed: %d\n",
                              (int)param->adv_stop_cmpl.status);
            }
            s_advRunning = false;
            break;
        default:
            break;
    }
}

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
// Per-message-type AD counters (Fix E). Indexed by the ODID message type
// nibble (msg[0] >> 4): 0 BasicID, 1 Location, 4 System, 5 OperatorID.
// Size 6 covers the four we emit over BLE plus two unused slots.
static uint8_t s_adCounters[6] = { 0 };
static uint8_t s_locCounter    = 0;   // drives the 6-slot rotation

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

static void fillSystem(ODID_System_data &sy) {
    memset(&sy, 0, sizeof(sy));
    static constexpr double OPERATOR_OFFSET_DEG = 0.001;
    sy.OperatorLocationType = ODID_OPERATOR_LOCATION_TYPE_TAKEOFF;
    sy.ClassificationType   = ODID_CLASSIFICATION_TYPE_UNDECLARED;
    sy.OperatorLatitude     = s_state.latitude - OPERATOR_OFFSET_DEG;
    sy.OperatorLongitude    = s_state.longitude - OPERATOR_OFFSET_DEG;
    sy.AreaCount            = 1;
    sy.AreaRadius           = 0;
    sy.AreaCeiling          = -1000.0f;
    sy.AreaFloor            = -1000.0f;
    sy.OperatorAltitudeGeo  = s_state.altitudeMeters - 5.0f;
    sy.Timestamp            = 0;
}

static void fillOperator(ODID_OperatorID_data &o) {
    memset(&o, 0, sizeof(o));
    o.OperatorIdType = ODID_OPERATOR_ID;
    snprintf(o.OperatorId, sizeof(o.OperatorId), "OP-%s", s_state.serial);
    o.OperatorId[sizeof(o.OperatorId) - 1] = '\0';
}

// Build a single 25-byte ODID message into `msg`. Returns true on success.
// Fix D: 6-slot rotation covers BasicID, Location×3, System, OperatorID —
// all four ASTM F3411 message types required on BLE, with Location
// oversampled 3:1 so receivers get fast position updates.
static bool buildRotatingMsg(uint8_t *msg /* 25 bytes */) {
    const uint8_t slot = (uint8_t)(s_locCounter % 6);
    ++s_locCounter;

    if (slot < 3) {
        ODID_Location_data  loc;
        ODID_Location_encoded enc;
        fillLocation(loc);
        if (encodeLocationMessage(&enc, &loc) != ODID_SUCCESS) return false;
        memcpy(msg, &enc, ODID_MESSAGE_SIZE);
        return true;
    }
    if (slot == 3) {
        ODID_BasicID_data b;
        ODID_BasicID_encoded enc;
        fillBasicId(b);
        if (encodeBasicIDMessage(&enc, &b) != ODID_SUCCESS) return false;
        memcpy(msg, &enc, ODID_MESSAGE_SIZE);
        return true;
    }
    if (slot == 4) {
        ODID_System_data sy;
        ODID_System_encoded enc;
        fillSystem(sy);
        if (encodeSystemMessage(&enc, &sy) != ODID_SUCCESS) return false;
        memcpy(msg, &enc, ODID_MESSAGE_SIZE);
        return true;
    }
    // slot == 5
    ODID_OperatorID_data op;
    ODID_OperatorID_encoded enc;
    fillOperator(op);
    if (encodeOperatorIDMessage(&enc, &op) != ODID_SUCCESS) return false;
    memcpy(msg, &enc, ODID_MESSAGE_SIZE);
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
    // App code byte: high nibble = ASTM identifier 0x0, low nibble = rotating
    // counter per message type (Fix E). The type lives in the upper nibble of
    // byte 0 of the encoded ODID message. Using a per-type counter matches
    // how real drones increment: each message type's counter advances only
    // when that type is actually transmitted, giving receivers a monotonic
    // per-type sequence.
    const uint8_t typeNibble = (uint8_t)(odid[0] >> 4);
    const uint8_t typeIdx    = (typeNibble < 6) ? typeNibble : 0;
    s_advData[i++] = (uint8_t)(s_adCounters[typeIdx] & 0x0F);
    s_adCounters[typeIdx] = (uint8_t)((s_adCounters[typeIdx] + 1) & 0x0F);
    // ASTM F3411 BLE4 Legacy: 23/25 bytes. Full 25B requires BLE 5 Extended ADV (not implemented).
    memcpy(&s_advData[i], odid, 23);
    i += 23;
    s_advDataLen = i;
}

// 1 Hz refresh of the advertisement payload.
static uint32_t s_lastAdRefreshMs = 0;
extern "C" void ridBleTick() {
    if (s_advStartPending) {
        s_advStartPending = false;
        if (esp_ble_gap_start_advertising(&ADV_PARAMS) != ESP_OK) {
            Serial.println("[XR1-RID] BLE adv start request failed");
            s_advRunning = false;
            g_ridStatus.bleActive = false;
        }
        return;
    }
    if (!g_ridStatus.bleActive || s_advConfigPending) return;
    const uint32_t now = millis();
    if (now - s_lastAdRefreshMs < 1000) return;
    s_lastAdRefreshMs = now;

    rebuildAdv();
    if (s_advDataLen == 0) return;
    if (esp_ble_gap_config_adv_data_raw(s_advData, s_advDataLen) == ESP_OK) {
        s_advConfigPending = true;
        ++g_ridStatus.bleFrameCount;
    }
}

// ----- public API ----------------------------------------------------------
bool remoteIdBleStart(const RemoteIdState &state) {
    if (g_ridStatus.bleActive || s_advConfigPending || s_advStartPending) return true;
    s_state = state;

    if (!bringUpBleIfNeeded()) {
        Serial.println("[XR1-RID] BLE stack init failed");
        return false;
    }

    memset(s_adCounters, 0, sizeof(s_adCounters));
    s_locCounter = 0;
    rebuildAdv();
    if (s_advDataLen == 0) {
        Serial.println("[XR1-RID] BLE adv encoding failed");
        return false;
    }
    s_advRunning = false;
    s_advStartPending = false;
    s_advConfigPending = true;
    g_ridStatus.bleActive = false;
    if (esp_ble_gap_config_adv_data_raw(s_advData, s_advDataLen) != ESP_OK) {
        s_advConfigPending = false;
        s_advStartPending = false;
        Serial.println("[XR1-RID] BLE raw adv config request failed");
        return false;
    }

    g_ridStatus.bleFrameCount = 0;
    s_lastAdRefreshMs = millis();
    Serial.printf("[XR1-RID] BLE adv requested: %s\n", s_state.serial);
    return true;
}

void remoteIdBleStop() {
    if (!g_ridStatus.bleActive && !s_advConfigPending && !s_advStartPending && !s_advRunning) return;
    s_advConfigPending = false;
    s_advStartPending = false;
    if (s_advRunning) {
        esp_ble_gap_stop_advertising();
    }
    s_advRunning = false;
    g_ridStatus.bleActive = false;
    Serial.printf("[XR1-RID] BLE adv OFF (%u refreshes)\n",
                  (unsigned)g_ridStatus.bleFrameCount);
}
