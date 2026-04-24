// ============================================================================
// DJI DroneID WiFi beacon emission on the XR1's ESP32C3 WiFi radio.
// Proprietary layout (OUI 26:37:12, subcommand 0x10 = Flight Telemetry) per
// RUB-SysSec DroneSecurity NDSS 2023 + Kismet's Kaitai definition.
//
// CRITICAL DELTA from ODID: DJI encodes lat/lon as int32(degrees × 174533),
// NOT ODID's int32(degrees × 1e7). Using the wrong scale yields positions
// off by a factor of ~57.
//
// Sends at ~200 ms intervals, rotating WiFi channels 1/6/11, sharing the
// ESP32C3 WiFi stack with remoteIdWifi via the ridWifiStack* extern API.
// ============================================================================

#include "remote_id.h"

#include <Arduino.h>
#include <string.h>

#include "esp_wifi.h"

extern RemoteIdStatus g_ridStatus;

extern "C" bool ridWifiStackUp();
extern "C" bool ridWifiStackBringUp();
extern "C" void ridWifiRefUp();
extern "C" void ridWifiRefDown();

// ----- DJI payload layout --------------------------------------------------
// Byte layout of the vendor-IE payload after OUI and type bytes. Offsets
// below are all relative to the start of the payload (not the beacon frame)
// and match the DroneSecurity NDSS 2023 reversing table.
//
//   [0] = OUI type (proprietary DJI value — 0x58 for v2 DroneID frames)
//   [1..2]  padding / version
//   [3] = subcommand = 0x10 (Flight Telemetry)
//   [4]     padding
//   [5..6]  sequence (int16 LE)
//   [7..8]  state_info (uint16 LE bitfield)
//   [9..24] serial_number ASCII[16] null-padded
//   [25..28] longitude int32 LE (degrees × 174533)
//   [29..32] latitude  int32 LE (degrees × 174533)
//   [33..34] altitude (m, int16 LE)
//   [35..36] height AGL (m, int16 LE)
//   [37..42] v_north / v_east / v_up int16 LE × 3 (m/s)
//   [43..48] pitch / roll / yaw int16 LE × 3 (raw/100/57.296 = radians)
//   [49..52] home_longitude int32 LE
//   [53..56] home_latitude  int32 LE
//   [57]     product_type (0x03=Mavic2, 0x0A=Mini2, 0x11=Air2S)
//   [58]     reserved
//   [59..78] uuid[20]
// Total payload length ≈ 79 bytes. The vendor IE element adds 2 + 3 (OUI) + 1
// (OUI type) header bytes; the beacon MAC header + fixed beacon body adds 38
// more. Total frame ≈ 123 bytes.

static constexpr uint8_t DJI_OUI[3]     = { 0x26, 0x37, 0x12 };
static constexpr uint8_t DJI_OUI_TYPE   = 0x58;   // DroneID v2 frames

// double (not float) so the lat/lon multiplication retains double precision
// before the int32 cast. At extreme latitudes the float mantissa (~7 sig
// figs) would truncate below the ~1-meter precision DJI's int32 encoding
// preserves.
static constexpr double DJI_LL_SCALE = 174533.0;  // degrees × this = int32

static RemoteIdState s_state = {};
static uint16_t      s_modelCode = 0x0A;
static uint16_t      s_seq = 0;

static inline void put16le(uint8_t *p, int16_t v)   { p[0] = v & 0xFF; p[1] = (v >> 8) & 0xFF; }
static inline void putU16le(uint8_t *p, uint16_t v) { p[0] = v & 0xFF; p[1] = (v >> 8) & 0xFF; }
static inline void put32le(uint8_t *p, int32_t v)   {
    p[0] = v & 0xFF; p[1] = (v >> 8) & 0xFF;
    p[2] = (v >> 16) & 0xFF; p[3] = (v >> 24) & 0xFF;
}

// ----- beacon frame assembly (shares header with ODID WiFi) ----------------
static constexpr uint8_t DJI_BEACON_HEADER[] = {
    0x80, 0x00,                          // Beacon
    0x00, 0x00,                          // Duration
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // Destination broadcast
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Source (filled at start)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // BSSID (same as source)
    0x00, 0x00,                          // Sequence (HW fills)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Timestamp
    0xC8, 0x00,                          // Beacon interval 200 TU (~204.8 ms)
    0x01, 0x00,                          // Capability: ESS
    0x00, 0x00,                          // SSID IE length=0
};
static constexpr size_t DJI_BEACON_HDR_SZ = sizeof(DJI_BEACON_HEADER);

static uint8_t s_mac[6]    = { 0x02, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD };
static uint8_t s_frame[256];
static size_t  s_frameLen = 0;

static void buildDjiPayload(uint8_t *p /*79 bytes*/) {
    memset(p, 0, 79);
    p[0] = DJI_OUI_TYPE;
    // [1..2] version / padding left zero
    p[3] = 0x10;                         // subcommand: flight telemetry
    // [4] pad
    putU16le(p + 5, s_seq++);            // sequence
    putU16le(p + 7, 0x0FFF);             // state_info: all valid flags

    strncpy((char *)(p + 9), s_state.serial, 16);

    put32le(p + 25, (int32_t)(s_state.longitude * DJI_LL_SCALE));
    put32le(p + 29, (int32_t)(s_state.latitude  * DJI_LL_SCALE));

    put16le(p + 33, (int16_t)s_state.altitudeMeters);
    put16le(p + 35, 50);                 // height AGL = 50 m

    // Velocity components: v_north, v_east, v_up. Approximate from speed+hdg.
    const float radHdg = s_state.headingDeg * 0.0174533f;
    put16le(p + 37, (int16_t)(s_state.speedMps * cosf(radHdg)));  // v_north
    put16le(p + 39, (int16_t)(s_state.speedMps * sinf(radHdg)));  // v_east
    put16le(p + 41, 0);                                           // v_up

    // Pitch / roll / yaw — yaw matches heading expressed in DJI's raw units.
    put16le(p + 43, 0);                                // pitch
    put16le(p + 45, 0);                                // roll
    put16le(p + 47, (int16_t)(s_state.headingDeg * 100));  // yaw (raw/100)

    // Home = current position for a simulated stationary takeoff point.
    put32le(p + 49, (int32_t)(s_state.longitude * DJI_LL_SCALE));
    put32le(p + 53, (int32_t)(s_state.latitude  * DJI_LL_SCALE));

    p[57] = (uint8_t)(s_modelCode & 0xFF);
    p[58] = 0;

    // UUID[20]: derive from serial hash so replays stay consistent.
    uint8_t *u = p + 59;
    uint32_t h = 0xABCD1234u;
    for (const char *c = s_state.serial; *c; ++c) {
        h = (h ^ (uint8_t)*c) * 0x01000193u;
        for (int b = 0; b < 20; ++b) u[b] ^= (uint8_t)(h >> (b & 24));
    }
}

static void rebuildFrame() {
    memcpy(s_frame, DJI_BEACON_HEADER, DJI_BEACON_HDR_SZ);
    memcpy(s_frame + 10, s_mac, 6);
    memcpy(s_frame + 16, s_mac, 6);

    uint8_t payload[79];
    buildDjiPayload(payload);

    size_t pos = DJI_BEACON_HDR_SZ;
    s_frame[pos++] = 0xDD;                        // vendor IE
    s_frame[pos++] = (uint8_t)(3 + sizeof(payload));
    s_frame[pos++] = DJI_OUI[0];
    s_frame[pos++] = DJI_OUI[1];
    s_frame[pos++] = DJI_OUI[2];
    // NOTE: DJI's OUI type byte is part of the payload at offset 0 (we write
    // DJI_OUI_TYPE into payload[0]), not a separate byte. So copy payload
    // directly after the 3-byte OUI.
    memcpy(s_frame + pos, payload, sizeof(payload));
    pos += sizeof(payload);

    s_frameLen = pos;
}

// ----- 200 ms scheduler ----------------------------------------------------
// Channel rotation is owned by the unified controller in remote_id_common.cpp
// so ODID and DJI don't race each other on esp_wifi_set_channel.
static uint32_t s_lastTxMs = 0;

extern "C" void ridDjiTick() {
    if (!g_ridStatus.djiActive) return;
    const uint32_t now = millis();
    if (now - s_lastTxMs < 200) return;
    s_lastTxMs = now;

    rebuildFrame();
    if (s_frameLen == 0) return;
    if (esp_wifi_80211_tx(WIFI_IF_STA, s_frame, s_frameLen, true) == ESP_OK) {
        ++g_ridStatus.djiFrameCount;
    }
}

// ----- public API ----------------------------------------------------------
bool djiDroneIdStart(const RemoteIdState &state, uint16_t modelCode) {
    if (g_ridStatus.djiActive) return true;
    s_state     = state;
    s_modelCode = modelCode ? modelCode : 0x0A;   // default Mini 2
    s_seq       = 0;

    if (!ridWifiStackBringUp()) {
        Serial.println("[XR1-RID] DJI WiFi stack init failed");
        return false;
    }
    ridWifiRefUp();

    // Distinct MAC per DJI source so WiFi ODID and DJI don't collide at L2.
    uint32_t h = 0xC0C0C0C0u;
    for (const char *p = s_state.serial; *p; ++p) {
        h = (h ^ (uint8_t)*p) * 0x01000193u;
    }
    s_mac[0] = 0x02;
    s_mac[1] = 0xDC;   // "DJ"-distinct second byte
    s_mac[2] = (h >> 16) & 0xFF;
    s_mac[3] = (h >>  8) & 0xFF;
    s_mac[4] = (h      ) & 0xFF;
    s_mac[5] = 0x0D;

    // Channel selection is owned by the unified controller in
    // remote_id_common.cpp; no set_channel call here (see H1 fix).
    s_lastTxMs = 0;

    g_ridStatus.djiActive     = true;
    g_ridStatus.djiFrameCount = 0;
    Serial.printf("[XR1-RID] DJI DroneID ON: serial=%s model=0x%02X\n",
                  s_state.serial, (unsigned)s_modelCode);
    return true;
}

void djiDroneIdStop() {
    if (!g_ridStatus.djiActive) return;
    g_ridStatus.djiActive = false;
    ridWifiRefDown();
    Serial.printf("[XR1-RID] DJI DroneID OFF (%u frames sent)\n",
                  (unsigned)g_ridStatus.djiFrameCount);
}
