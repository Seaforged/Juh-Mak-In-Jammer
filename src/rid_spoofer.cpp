#include <Arduino.h>
#include "rid_spoofer.h"
#include "esp_idf_version.h"

// Two delivery backends, picked at compile time:
//   - ESP-IDF < 5 (Arduino-ESP32 v2.x):  Bluedroid GAP + raw WiFi beacons
//   - ESP-IDF >= 5 (Arduino-ESP32 v3.x): NimBLE advertising only
//
// In v3.0 the WiFi ODID + DJI IE 221 transports moved to the XR1, leaving
// the T3-S3 2.4 GHz radio idle. The NimBLE branch uses it for BLE 4 Legacy
// ASTM F3411 ODID broadcasts so the system has a functional BLE emitter
// without coexistence contention. The Bluedroid branch stays as reference
// for older toolchains; it never compiles on the current Arduino-ESP32 v3.

// ============================================================
// SHARED: ASTM F3411 Open Drone ID encoding
// ============================================================
// Each ODID message is 25 bytes: 1 byte header + 24 bytes payload.
// Header: [msg_type(4 bits) | proto_version(4 bits)]
// Both backends share these helpers — no Bluedroid/NimBLE deps.

static constexpr uint8_t ODID_MSG_TYPE_BASIC_ID    = 0x00;
static constexpr uint8_t ODID_MSG_TYPE_LOCATION    = 0x10;
static constexpr uint8_t ODID_MSG_TYPE_SYSTEM      = 0x40;
static constexpr uint8_t ODID_MSG_TYPE_OPERATOR_ID = 0x50;
static constexpr uint8_t ODID_PROTO_VERSION        = 0x02;  // F3411-22a
static constexpr uint8_t ODID_MSG_SIZE             = 25;

// BLE 4 Legacy advertising has a hard 31-byte cap on the entire payload.
// After Flags AD (3 B) and the Service Data AD header (length, type,
// 2-byte UUID, 1-byte app code = 5 B), only 23 bytes remain for ODID
// payload. Spec-correct full 25-byte messages require BLE 5 Extended
// Advertising; SENTRY-RF's parser zero-pads 23->25 before decode (commit
// 650db49) so the truncation is benign for that receiver.
static constexpr uint8_t BLE4_ODID_MSG_SIZE = 23;

// Message pack header (used by the WiFi vendor IE in the <5 branch only).
static constexpr uint8_t ODID_MSG_PACK_TYPE        = 0xF0;
static constexpr uint8_t ODID_MSG_PACK_MAX_MSGS    = 4;

// WiFi vendor-specific element OUI for ODID (CTA-2063-A) — used by the
// Bluedroid/WiFi branch only; XR1 owns WiFi RID on the v3.0 toolchain.
static const uint8_t ODID_WIFI_OUI[] = { 0xFA, 0x0B, 0xBC };
static constexpr uint8_t ODID_WIFI_OUI_TYPE = 0x0D;

// ----- shared runtime state -----
static bool         _ridRunning = false;
static DroneState   _drone;
static uint32_t     _wifiCount  = 0;   // stays 0 in NimBLE branch
static uint32_t     _bleCount   = 0;
static uint8_t      _bleAdCounters[6] = { 0 };  // per-msg-type rotating counter
static uint8_t      _bleRotation = 0;            // 6-slot LLLBSO rotation index

// ----- shared encoding helpers -----
static int32_t encodeLatLon(double deg) {
    return (int32_t)(deg * 1e7);
}

static uint16_t encodeAlt(float meters) {
    // ODID altitude: offset by -1000 m, in 0.5 m increments
    return (uint16_t)((meters + 1000.0f) * 2.0f);
}

static uint8_t encodeSpeed(float mps) {
    if (mps < 0) mps = 0;
    if (mps <= 63.75f) return (uint8_t)(mps * 4.0f);
    return 0xFF;
}

static void buildBasicIdMsg(uint8_t *buf) {
    memset(buf, 0, ODID_MSG_SIZE);
    buf[0] = ODID_MSG_TYPE_BASIC_ID | ODID_PROTO_VERSION;
    // [id_type(4) | ua_type(4)]; id_type=1 (Serial Number)
    buf[1] = (0x01 << 4) | (_drone.uaType & 0x0F);
    strncpy((char *)&buf[2], _drone.serialNumber, 20);
}

static void buildLocationMsg(uint8_t *buf) {
    memset(buf, 0, ODID_MSG_SIZE);
    buf[0] = ODID_MSG_TYPE_LOCATION | ODID_PROTO_VERSION;

    // Status byte (ASTM F3411-22a §A.5.2): bits 7-4 = Status,
    // bit 1 = E/W Direction Segment.
    float h = fmodf(_drone.heading, 360.0f);
    if (h < 0) h += 360.0f;
    bool ewBit = (h >= 180.0f);
    if (ewBit) h -= 180.0f;
    buf[1] = 0x20 | (ewBit ? 0x02 : 0x00);  // Airborne + optional EW segment
    buf[2] = (uint8_t)h;                    // 0-179 degrees

    buf[3] = encodeSpeed(_drone.speed);

    // Vertical speed: signed int8 in 0.5 m/s increments, NO offset (range +/-63.5).
    int8_t vs = (int8_t)constrain((int)(_drone.vspeed * 2.0f), -127, 127);
    buf[4] = (uint8_t)vs;

    int32_t lat = encodeLatLon(_drone.latitude);
    memcpy(&buf[5], &lat, 4);
    int32_t lon = encodeLatLon(_drone.longitude);
    memcpy(&buf[9], &lon, 4);
    uint16_t altBaro = encodeAlt(_drone.altBaro);
    memcpy(&buf[13], &altBaro, 2);
    uint16_t altGeo = encodeAlt(_drone.altGeo);
    memcpy(&buf[15], &altGeo, 2);
    uint16_t height = encodeAlt(_drone.height);
    memcpy(&buf[17], &height, 2);
    // [19..21]: accuracy fields (0 = unknown)

    // [22..23]: Timestamp uint16 LE, tenths of a second since the last full
    // UTC hour (0..35999). No RTC/GPS, so derived from millis() as a rolling
    // "tenths since boot-hour".
    uint32_t nowMs = millis();
    uint32_t secSinceBoot = nowMs / 1000;
    uint16_t tenthsSinceHour = (uint16_t)((secSinceBoot % 3600) * 10
                                         + (nowMs % 1000) / 100);
    if (tenthsSinceHour > 35999) tenthsSinceHour = 35999;
    buf[22] = (uint8_t)(tenthsSinceHour & 0xFF);
    buf[23] = (uint8_t)((tenthsSinceHour >> 8) & 0xFF);
}

static void buildSystemMsg(uint8_t *buf) {
    memset(buf, 0, ODID_MSG_SIZE);
    buf[0] = ODID_MSG_TYPE_SYSTEM | ODID_PROTO_VERSION;
    buf[1] = 0x00;  // takeoff location, undeclared classification
    int32_t lat = encodeLatLon(_drone.opLat);
    memcpy(&buf[2], &lat, 4);
    int32_t lon = encodeLatLon(_drone.opLon);
    memcpy(&buf[6], &lon, 4);
    buf[10] = 1;   // area count
    buf[11] = 0;   // area radius (point)
}

static void buildOperatorIdMsg(uint8_t *buf) {
    memset(buf, 0, ODID_MSG_SIZE);
    buf[0] = ODID_MSG_TYPE_OPERATOR_ID | ODID_PROTO_VERSION;
    buf[1] = 0x00;  // operator ID type = CAA
    strncpy((char *)&buf[2], _drone.operatorId, 20);
}

// 6-slot rotation: Location x3 -> Basic -> System -> Operator (LLLBSO).
// Matches ASTM F3411 oversampling guidance for Location messages.
static void buildBleRotatingMsg(uint8_t *buf) {
    const uint8_t slot = _bleRotation % 6;
    _bleRotation = (_bleRotation + 1) % 6;
    if (slot < 3)        buildLocationMsg(buf);
    else if (slot == 3)  buildBasicIdMsg(buf);
    else if (slot == 4)  buildSystemMsg(buf);
    else                 buildOperatorIdMsg(buf);
}

static uint8_t nextBleAdCounter(const uint8_t *msg) {
    const uint8_t typeNibble = (uint8_t)(msg[0] >> 4);
    const uint8_t idx = (typeNibble < 6) ? typeNibble : 0;
    const uint8_t counter = _bleAdCounters[idx] & 0x0F;
    _bleAdCounters[idx] = (_bleAdCounters[idx] + 1) & 0x0F;
    return counter;
}

// Default test-drone identity (Virginia Beach test site, near SENTRY-RF).
static void initDroneDefaults() {
    strncpy(_drone.serialNumber, "JJ-T3S3-BLE-001", sizeof(_drone.serialNumber));
    _drone.uaType    = 2;          // Helicopter / Multirotor
    _drone.latitude  = 36.8529;
    _drone.longitude = -75.978;
    _drone.altGeo    = 120.0f;
    _drone.altBaro   = 119.5f;
    _drone.height    = 50.0f;
    _drone.speed     = 5.0f;
    _drone.vspeed    = 0.0f;
    _drone.heading   = 90.0f;
    _drone.opLat     = 36.8520;
    _drone.opLon     = -75.979;
    strncpy(_drone.operatorId, "OP-JJ-T3S3-BLE", sizeof(_drone.operatorId));
}

// ============================================================
// Backend selection
// ============================================================

#if defined(ESP_IDF_VERSION_MAJOR) && ESP_IDF_VERSION_MAJOR < 5

// ------------------------------------------------------------
// LEGACY: Bluedroid + raw WiFi beacons (Arduino-ESP32 v2.x).
// Dead code on the v3 toolchain; kept as reference.
// ------------------------------------------------------------

#include <esp_wifi.h>
#include <esp_event.h>
#include <nvs_flash.h>
#include <esp_bt.h>
#include <esp_bt_main.h>
#include <esp_gap_ble_api.h>

static bool _wifiReady = false;
static bool _bleReady = false;
static volatile bool _bleAdvConfigPending = false;
static volatile bool _bleAdvRunning = false;
static volatile bool _bleAdvStartPending = false;

static esp_ble_adv_params_t _bleAdvParams = {
    .adv_int_min       = 0x20,
    .adv_int_max       = 0x40,
    .adv_type          = ADV_TYPE_NONCONN_IND,
    .own_addr_type     = BLE_ADDR_TYPE_RANDOM,
    .peer_addr         = { 0 },
    .peer_addr_type    = BLE_ADDR_TYPE_PUBLIC,
    .channel_map       = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

static unsigned long _lastTxMs = 0;
static constexpr unsigned long RID_TX_INTERVAL_MS = 1000;
static uint8_t _beaconMac[6];

static constexpr size_t BEACON_MAX_LEN = 256;
static uint8_t _beaconFrame[BEACON_MAX_LEN];

static size_t buildBeaconFrame() {
    size_t pos = 0;
    _beaconFrame[pos++] = 0x80; _beaconFrame[pos++] = 0x00;          // Beacon
    _beaconFrame[pos++] = 0x00; _beaconFrame[pos++] = 0x00;          // Duration
    memset(&_beaconFrame[pos], 0xFF, 6); pos += 6;                   // DA bcast
    memcpy(&_beaconFrame[pos], _beaconMac, 6); pos += 6;             // SA
    memcpy(&_beaconFrame[pos], _beaconMac, 6); pos += 6;             // BSSID
    _beaconFrame[pos++] = 0x00; _beaconFrame[pos++] = 0x00;          // Seq
    memset(&_beaconFrame[pos], 0, 8); pos += 8;                      // Timestamp
    _beaconFrame[pos++] = 0xE8; _beaconFrame[pos++] = 0x03;          // Interval 1024 ms
    _beaconFrame[pos++] = 0x01; _beaconFrame[pos++] = 0x00;          // Capability
    _beaconFrame[pos++] = 0x00; _beaconFrame[pos++] = 0x00;          // SSID IE empty

    _beaconFrame[pos++] = 0xDD;                                      // Vendor IE
    size_t vendorLenPos = pos;
    _beaconFrame[pos++] = 0x00;                                      // length placeholder
    memcpy(&_beaconFrame[pos], ODID_WIFI_OUI, 3); pos += 3;
    _beaconFrame[pos++] = ODID_WIFI_OUI_TYPE;
    _beaconFrame[pos++] = ODID_MSG_PACK_TYPE | ODID_PROTO_VERSION;
    _beaconFrame[pos++] = ODID_MSG_SIZE;
    _beaconFrame[pos++] = ODID_MSG_PACK_MAX_MSGS;
    buildBasicIdMsg(&_beaconFrame[pos]);    pos += ODID_MSG_SIZE;
    buildLocationMsg(&_beaconFrame[pos]);   pos += ODID_MSG_SIZE;
    buildSystemMsg(&_beaconFrame[pos]);     pos += ODID_MSG_SIZE;
    buildOperatorIdMsg(&_beaconFrame[pos]); pos += ODID_MSG_SIZE;
    _beaconFrame[vendorLenPos] = (uint8_t)(pos - vendorLenPos - 1);
    return pos;
}

static void wifiInit() {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();
    esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
    _wifiReady = true;
    Serial.println("RID: WiFi initialized for beacon TX");
}

static void wifiTransmitBeacon() {
    if (!_wifiReady) return;
    size_t len = buildBeaconFrame();
    if (esp_wifi_80211_tx(WIFI_IF_STA, _beaconFrame, len, true) == ESP_OK) {
        _wifiCount++;
    }
}

static uint8_t _bleAdvData[31];

static void bleGapCallback(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    switch (event) {
        case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
            _bleAdvConfigPending = false;
            if (param->adv_data_raw_cmpl.status != ESP_BT_STATUS_SUCCESS) {
                _bleAdvStartPending = false;
                break;
            }
            if (_ridRunning && !_bleAdvRunning) _bleAdvStartPending = true;
            break;
        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
            _bleAdvRunning = (param->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS);
            break;
        case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
            _bleAdvRunning = false;
            break;
        default: break;
    }
}

static void bleInit() {
    esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_bt_controller_init(&bt_cfg);
    esp_bt_controller_enable(ESP_BT_MODE_BLE);
    esp_bluedroid_init();
    esp_bluedroid_enable();
    esp_ble_gap_register_callback(bleGapCallback);
    uint8_t rand_addr[6];
    esp_fill_random(rand_addr, 6);
    rand_addr[0] |= 0xC0;
    esp_ble_gap_set_rand_addr(rand_addr);
    _bleReady = true;
    Serial.println("RID: BLE initialized for advertising");
}

static void bleTransmitOdid() {
    if (!_bleReady || _bleAdvConfigPending) return;
    if (_bleAdvStartPending) {
        _bleAdvStartPending = false;
        esp_ble_gap_start_advertising(&_bleAdvParams);
        return;
    }
    size_t pos = 0;
    _bleAdvData[pos++] = 0x02;
    _bleAdvData[pos++] = 0x01;
    _bleAdvData[pos++] = 0x06;
    uint8_t svcDataLen = 2 + 1 + BLE4_ODID_MSG_SIZE;
    _bleAdvData[pos++] = svcDataLen + 1;
    _bleAdvData[pos++] = 0x16;
    _bleAdvData[pos++] = 0xFA;
    _bleAdvData[pos++] = 0xFF;
    uint8_t tempMsg[ODID_MSG_SIZE];
    buildBleRotatingMsg(tempMsg);
    _bleAdvData[pos++] = nextBleAdCounter(tempMsg);
    memcpy(&_bleAdvData[pos], tempMsg, BLE4_ODID_MSG_SIZE);
    pos += BLE4_ODID_MSG_SIZE;
    if (esp_ble_gap_config_adv_data_raw(_bleAdvData, pos) == ESP_OK) {
        _bleAdvConfigPending = true;
        _bleCount++;
    }
}

void ridInit() {
    _ridRunning = false;
    _wifiCount = 0;
    _bleCount = 0;
    esp_fill_random(_beaconMac, 6);
    _beaconMac[0] = (_beaconMac[0] | 0x02) & 0xFE;
    initDroneDefaults();
}

void ridStart() {
    if (!_wifiReady) wifiInit();
    if (!_bleReady)  bleInit();
    _wifiCount = 0; _bleCount = 0;
    memset(_bleAdCounters, 0, sizeof(_bleAdCounters));
    _bleRotation = 0;
    _bleAdvRunning = false; _bleAdvConfigPending = false; _bleAdvStartPending = false;
    _ridRunning = true;
    _lastTxMs = millis();
    Serial.println("RID: Spoofing started (WiFi beacons + BLE adverts)");
}

void ridStop() {
    _ridRunning = false;
    _bleAdvConfigPending = false; _bleAdvStartPending = false;
    if (_bleReady && _bleAdvRunning) esp_ble_gap_stop_advertising();
    _bleAdvRunning = false;
    Serial.printf("RID: Stopped -- %lu WiFi beacons, %lu BLE adverts\n",
                  (unsigned long)_wifiCount, (unsigned long)_bleCount);
}

void ridUpdate() {
    if (!_ridRunning) return;
    unsigned long now = millis();
    if ((now - _lastTxMs) < RID_TX_INTERVAL_MS) return;
    _lastTxMs = now;
    wifiTransmitBeacon();
    bleTransmitOdid();
}

RidParams ridGetParams() {
    return RidParams{ _wifiCount, _bleCount, _drone.latitude, _drone.longitude,
                      _drone.altGeo, _ridRunning };
}

#else  // ESP_IDF_VERSION_MAJOR >= 5 -- NimBLE BLE-only on the T3-S3.

// ------------------------------------------------------------
// NimBLE BLE 4 Legacy ODID emitter for Arduino-ESP32 v3.
// XR1 owns WiFi ODID + DJI IE 221 over UART; the T3-S3's 2.4 GHz
// radio is otherwise idle, so BLE advertising lives here without
// coexistence contention.
// ------------------------------------------------------------

#include <NimBLEDevice.h>
// Direct NimBLE host API: bypass NimBLEAdvertisementData entirely. The C++
// wrapper buffers advertisement bytes at the host and never pushes them to
// the controller's adv buffer -- nRF Connect confirmed an empty
// ADV_NONCONN_IND PDU despite host-side isAdvertising()==true. These
// headers expose ble_gap_adv_set_data / ble_gap_adv_start /
// ble_gap_adv_active (controller ground truth) and ble_hs_id_set_rnd.
#include "host/ble_gap.h"
#include "host/ble_hs_id.h"

static bool          _bleReady   = false;
static bool          _advStarted = false;
static unsigned long _lastTxMs   = 0;
// 200 ms refresh cadence: at the 6-slot rotation rate, a passive 1 Hz
// scan window sees ~5 ODID frames per pass, enough for SENTRY-RF to
// observe at least one of each message type in the LLLBSO cycle within a
// few seconds. Stays well under the legacy adv interval ceiling.
static constexpr unsigned long RID_TX_INTERVAL_MS = 200;

static bool bleInit() {
    if (_bleReady) return true;

    // NimBLEDevice::init() brings up the NimBLE host stack (controller +
    // host task). The "JJ-RID" name is informational here only -- we no
    // longer use NimBLEAdvertising, so the name does not appear in the
    // advert. SENTRY-RF identifies JJ via the ASTM Service Data UUID
    // 0xFFFA, not the device name.
    NimBLEDevice::init("JJ-RID");
    // Crank advertising TX to the API max. NimBLE-Arduino takes int8_t
    // dBm; 21 will be clamped by the controller to whatever the chip
    // and the ESP-IDF coex config allow (typically +9 dBm out-of-box,
    // up to +20 dBm with CONFIG_BT_CTRL_TX_POWER tuned).
    NimBLEDevice::setPower(21, NimBLETxPowerType::Advertise);
    const int actualDbm = NimBLEDevice::getPower(NimBLETxPowerType::Advertise);

    // NimBLEDevice::init blocks until host/controller sync, but we have
    // observed the controller's advertising buffer being clobbered by
    // post-sync activity (GAP service characteristic writes, default
    // adv-instance setup, coex negotiation). 500 ms gives that activity
    // time to settle before we start writing our own adv data.
    vTaskDelay(pdMS_TO_TICKS(500));

    // Static random address. Per Bluetooth Core: top two bits of the MSB
    // (little-endian, so byte index 5) must be 1-1. ble_hs_id_set_rnd
    // takes a bare 6-byte buffer in host (little-endian) byte order.
    uint8_t rnd_addr[6];
    esp_fill_random(rnd_addr, 6);
    rnd_addr[5] |= 0xC0;
    int rc = ble_hs_id_set_rnd(rnd_addr);
    if (rc != 0) {
        Serial.printf("[RID] ble_hs_id_set_rnd FAILED rc=%d\n", rc);
        return false;
    }

    _bleReady = true;
    Serial.printf("[RID] NimBLE direct HCI init: power=%d dBm addr=%02X:%02X:%02X:%02X:%02X:%02X\n",
                  actualDbm,
                  rnd_addr[5], rnd_addr[4], rnd_addr[3],
                  rnd_addr[2], rnd_addr[1], rnd_addr[0]);
    return true;
}

static void bleTransmitOdid() {
    if (!_bleReady) return;

    // Build one 25-byte ODID message, take the first 23 (BLE 4 cap), and
    // prepend the 1-byte rotating per-msg-type counter that ASTM expects in
    // the upper byte of the Service Data app-code field. Total Service
    // Data payload = 1 (counter) + 23 (truncated ODID) = 24 bytes; with
    // the AD header (length + type 0x16 + UUID 0xFFFA) and the Flags AD
    // (3 bytes) the whole advert lands at 31 bytes exactly.
    uint8_t tempMsg[ODID_MSG_SIZE];
    buildBleRotatingMsg(tempMsg);

    uint8_t svcData[1 + BLE4_ODID_MSG_SIZE];
    svcData[0] = nextBleAdCounter(tempMsg);
    memcpy(svcData + 1, tempMsg, BLE4_ODID_MSG_SIZE);

    // Raw 31-byte legacy advertisement payload, hand-assembled per
    // Bluetooth Core Vol 3 Part C 11. No NimBLE wrapper involvement.
    uint8_t adv[31];
    uint8_t pos = 0;

    // Flags AD (3 bytes total: len, type, value)
    adv[pos++] = 0x02;   // length
    adv[pos++] = 0x01;   // AD type: Flags
    adv[pos++] = 0x06;   // LE General Disc + BR/EDR Not Supported

    // Service Data 16-bit UUID AD (28 bytes total: len, type, UUID lo/hi, payload)
    // length = 1 (type) + 2 (UUID) + 24 (svcData) = 27
    adv[pos++] = (uint8_t)(1 + 2 + sizeof(svcData));
    adv[pos++] = 0x16;   // AD type: Service Data 16-bit UUID
    adv[pos++] = 0xFA;   // UUID 0xFFFA low byte (ASTM F3411 ODID)
    adv[pos++] = 0xFF;   // UUID 0xFFFA high byte
    memcpy(&adv[pos], svcData, sizeof(svcData));
    pos += sizeof(svcData);
    // pos == 31 exactly

    // Push raw bytes straight to the controller. Per Bluetooth Core HCI
    // LE Set Advertising Data is permitted during active advertising;
    // the controller swaps in the new payload on the next adv event.
    int rc = ble_gap_adv_set_data(adv, pos);
    if (rc != 0) {
        Serial.printf("[RID] ble_gap_adv_set_data FAILED rc=%d\n", rc);
        return;
    }

    // Start advertising once. Repeated start() on an already-active
    // advertiser wedges NimBLE's state machine after ~60s (observed:
    // 675 events in the first 60s, then silent thereafter).
    if (!_advStarted) {
        // First-call diagnostic: dump the exact 31 bytes we just handed
        // to the controller. If nRF Connect still reports an empty PDU
        // and these bytes look correct, the bug is past our code.
        Serial.print("[RID-VERIFY] adv bytes:");
        for (uint8_t i = 0; i < pos; i++) {
            Serial.printf(" %02X", adv[i]);
        }
        Serial.println();

        struct ble_gap_adv_params adv_params = {};
        adv_params.conn_mode = BLE_GAP_CONN_MODE_NON;
        adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
        adv_params.itvl_min  = 0x00A0;  // 100 ms in 0.625 ms units
        adv_params.itvl_max  = 0x0140;  // 200 ms

        rc = ble_gap_adv_start(BLE_OWN_ADDR_RANDOM, NULL,
                               BLE_HS_FOREVER, &adv_params,
                               NULL, NULL);
        if (rc != 0) {
            Serial.printf("[RID] ble_gap_adv_start FAILED rc=%d\n", rc);
            return;
        }
        _advStarted = true;
        Serial.println("[RID] BLE advertising started (direct HCI)");

        // Confirm controller is actually keying after start. If
        // ctrl_adv=0 here despite start returning rc=0, NimBLE took
        // the call but the controller dropped advertising.
        vTaskDelay(pdMS_TO_TICKS(100));
        Serial.printf("[RID-VERIFY] ctrl_adv=%d after start+100ms\n",
                      ble_gap_adv_active());
    }

    _bleCount++;
}

void ridInit() {
    _ridRunning = false;
    _wifiCount = 0;
    _bleCount  = 0;
    initDroneDefaults();
    Serial.println("[RID] T3-S3 BLE RID ready (NimBLE direct HCI backend)");
    Serial.println("[RID] WiFi ODID + DJI IE 221 remain on the XR1.");
}

void ridStart() {
    if (!bleInit()) {
        Serial.println("[RID] BLE init failed -- cannot start RID");
        return;
    }
    _bleCount = 0;
    _advStarted = false;
    memset(_bleAdCounters, 0, sizeof(_bleAdCounters));
    _bleRotation = 0;
    _ridRunning = true;
    _lastTxMs = 0;  // fire on next ridUpdate() tick
    Serial.printf("[RID] BLE ASTM F3411 advertising arming: %s @ %.4f,%.4f\n",
                  _drone.serialNumber, _drone.latitude, _drone.longitude);
}

void ridStop() {
    if (_bleReady) {
        ble_gap_adv_stop();
    }
    _advStarted = false;
    _ridRunning = false;
    Serial.printf("[RID] BLE advertising stopped (%lu adverts)\n",
                  (unsigned long)_bleCount);
}

void ridUpdate() {
    if (!_ridRunning) return;
    const unsigned long now = millis();
    if ((now - _lastTxMs) < RID_TX_INTERVAL_MS) return;
    _lastTxMs = now;
    bleTransmitOdid();

    // Periodic health log (every 30 s). ble_gap_adv_active() is the
    // controller-side ground truth -- a started=1 ctrl_adv=0 split means
    // NimBLE accepted start() but the controller has dropped advertising,
    // the failure mode that surfaced after the EXT_ADV swap.
    static uint32_t lastHealthMs = 0;
    if ((now - lastHealthMs) >= 30000) {
        lastHealthMs = now;
        Serial.printf("[RID-HEALTH] BLE count=%lu started=%d ctrl_adv=%d\n",
                      (unsigned long)_bleCount,
                      (int)_advStarted,
                      (int)ble_gap_adv_active());
    }
}

RidParams ridGetParams() {
    return RidParams{ _wifiCount, _bleCount, _drone.latitude, _drone.longitude,
                      _drone.altGeo, _ridRunning };
}

#endif  // ESP_IDF_VERSION_MAJOR < 5
