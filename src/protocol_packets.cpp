// ============================================================================
// Wire-authentic packet builders for JJ's emulated protocols.
//
// These helpers let rf_modes.cpp, crossfire.cpp, sik_radio.cpp, and
// xr1_modes.cpp produce byte-for-byte valid packets (correct framing, CRCs,
// and counters) instead of the random/static dummies used through Phase 6.
// See header for spec citations.
// ============================================================================

#include "protocol_packets.h"
#include <string.h>

// ----- CRC primitives ------------------------------------------------------

// CRC-8/DVB-S2, polynomial 0xD5, init 0x00, no reflection, no XOR-out.
// Used by TBS Crossfire CRSF framing.
uint8_t crc8_dvbs2(const uint8_t *data, size_t len) {
    uint8_t crc = 0;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int b = 0; b < 8; ++b) {
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0xD5) : (uint8_t)(crc << 1);
        }
    }
    return crc;
}

// MAVLink X.25 CRC — reference implementation from MAVLink/include_v2.0/checksum.h.
// Accumulate each data byte, then the msg's CRC_EXTRA byte before reading
// the final 16-bit result low-byte-first onto the wire.
static inline void mav_acc(uint16_t &crc, uint8_t byte) {
    uint8_t tmp = byte ^ (uint8_t)(crc & 0xFF);
    tmp ^= (uint8_t)(tmp << 4);
    crc = (uint16_t)((crc >> 8)
                     ^ ((uint16_t)tmp << 8)
                     ^ ((uint16_t)tmp << 3)
                     ^ ((uint16_t)tmp >> 4));
}

uint16_t mavlink_crc_x25(const uint8_t *data, size_t len, uint8_t crc_extra) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; ++i) mav_acc(crc, data[i]);
    mav_acc(crc, crc_extra);
    return crc;
}

// ExpressLRS OTA CRC-14 — polynomial 0x2B5. Bit-by-bit implementation; the
// real firmware uses a lookup table for speed, but for our emit rates this
// is fine and the output matches bit-for-bit.
uint16_t elrs_crc14(const uint8_t *data, size_t len, uint16_t seed) {
    const uint16_t poly = 0x2B5u;
    uint16_t crc = seed & 0x3FFFu;
    for (size_t i = 0; i < len; ++i) {
        crc ^= (uint16_t)data[i] << 6;
        for (int b = 0; b < 8; ++b) {
            crc = (crc & 0x2000u) ? (uint16_t)((crc << 1) ^ poly)
                                  : (uint16_t)(crc << 1);
            crc &= 0x3FFFu;
        }
    }
    return crc;
}

// ExpressLRS UID -> CRC-14 seed. The upstream formula mixes the four
// low-order UID bytes via a CRC-14 over them starting from 0. This gives
// each binding a unique seed so CRCs can't cross-validate between TX/RX
// pairs. Source: ExpressLRS OtaUpdateCrcInitFromUid().
uint16_t elrs_crc14_seed_from_uid(const uint8_t uid[6]) {
    uint8_t seedBytes[4] = { uid[2], uid[3], uid[4], uid[5] };
    return elrs_crc14(seedBytes, 4, 0) ^ 0x0001u;
}

const uint8_t JJ_ELRS_TEST_UID[6] = { 0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x02 };

// ----- CRSF (TBS Crossfire) RC Channels Packed ----------------------------
// Frame layout (total 26 bytes):
//   [0]  sync byte 0xC8 (TX -> receiver direction)
//   [1]  length (24 = type + 22-byte payload + CRC8)
//   [2]  type 0x16 (RC_CHANNELS_PACKED)
//   [3..24] 22 bytes of 16 channels × 11 bits, little-endian packed
//   [25] CRC-8/DVB-S2 over bytes [2..24]
size_t build_crsf_rc_channels_packed(uint8_t *out, const uint16_t channels[16]) {
    out[0] = 0xC8;
    out[1] = 24;       // length = type (1) + payload (22) + crc (1)
    out[2] = 0x16;

    // Pack 16 × 11-bit little-endian into bytes [3..24].
    uint8_t *p = &out[3];
    uint64_t buf = 0;
    int bits = 0;
    for (int i = 0; i < 16; ++i) {
        buf |= ((uint64_t)(channels[i] & 0x07FF)) << bits;
        bits += 11;
        while (bits >= 8) {
            *p++ = (uint8_t)(buf & 0xFF);
            buf >>= 8;
            bits -= 8;
        }
    }
    if (bits > 0) *p++ = (uint8_t)(buf & 0xFF);

    out[25] = crc8_dvbs2(&out[2], 23);
    return 26;
}

// ----- MAVLink v2 HEARTBEAT (msgid 0) --------------------------------------
// Frame layout per MAVLink v2 spec (total 21 bytes for a 9-byte payload):
//   [0]  STX2 0xFD
//   [1]  payload_len = 9
//   [2]  incompat_flags = 0
//   [3]  compat_flags   = 0
//   [4]  seq (increments per frame)
//   [5]  sysid
//   [6]  compid
//   [7..9] msgid little-endian (HEARTBEAT = 0x000000)
//   [10..18] 9-byte payload:
//             u32 custom_mode     (0)
//             u8  type            (2 = quadrotor)
//             u8  autopilot       (3 = ArduPilot)
//             u8  base_mode       (0x81 = armed + custom_mode_enabled)
//             u8  system_status   (4 = ACTIVE)
//             u8  mavlink_version (3)
//   [19..20] CRC-16 X.25 over bytes [1..18] + CRC_EXTRA (50 for HEARTBEAT)
static constexpr uint8_t MAVLINK_HEARTBEAT_CRC_EXTRA = 50;

size_t build_mavlink_heartbeat_v2(uint8_t *out, uint8_t &seq,
                                  uint8_t sysid, uint8_t compid) {
    out[0]  = 0xFD;
    out[1]  = 9;           // HEARTBEAT payload length
    out[2]  = 0;           // incompat_flags
    out[3]  = 0;           // compat_flags
    out[4]  = seq++;
    out[5]  = sysid;
    out[6]  = compid;
    out[7]  = 0; out[8]  = 0; out[9]  = 0;   // msgid 0 (HEARTBEAT), LE

    // Payload — custom_mode (u32) then fixed bytes. ArduPilot quadrotor
    // armed, custom mode 0 (Stabilize), system active.
    out[10] = 0; out[11] = 0; out[12] = 0; out[13] = 0;  // custom_mode
    out[14] = 2;           // type: MAV_TYPE_QUADROTOR
    out[15] = 3;           // autopilot: MAV_AUTOPILOT_ARDUPILOTMEGA
    out[16] = 0x81;        // base_mode: armed + custom_mode_enabled
    out[17] = 4;           // system_status: MAV_STATE_ACTIVE
    out[18] = 3;           // mavlink_version

    const uint16_t crc = mavlink_crc_x25(&out[1], 18, MAVLINK_HEARTBEAT_CRC_EXTRA);
    out[19] = (uint8_t)(crc & 0xFF);
    out[20] = (uint8_t)((crc >> 8) & 0xFF);
    return 21;
}

// ----- ExpressLRS OTA packet ----------------------------------------------
// Packet layout for the 8/10-byte LoRa modes (ExpressLRS OTA v4):
//   [0]  packet header — upper 2 bits encode packet type (sync phase),
//        lower 6 bits are the nonce counter (increments mod 64)
//   [1..payloadLen-3] RC channel bits + TLM flags
//   [payloadLen-2..payloadLen-1] CRC-14 (upper 14 bits of 16-bit word;
//        bytes written big-endian so receiver can mask top 2 bits cleanly)
//
// For this emulator we synthesize plausible mid-stick RC data. The exact
// packing is the OTA4 scheme: 10-bit channel values for 4 channels, packed
// LSB-first, plus a few flag bits. We fill channels 1-4 with 0x200 (mid)
// and leave AUX bits zero.
size_t build_elrs_ota_packet(uint8_t *out, uint8_t payloadLen,
                             uint8_t &nonceCounter,
                             const uint8_t uid[6]) {
    if (payloadLen != 8 && payloadLen != 10) return 0;

    // Header: packet type 0 (RC_DATA) in upper 2 bits, nonce in lower 6.
    out[0] = (uint8_t)((0 << 6) | (nonceCounter & 0x3F));
    nonceCounter = (uint8_t)((nonceCounter + 1) & 0x3F);

    // Fill the middle bytes with 4 × 10-bit mid-stick RC values (0x200)
    // plus an arming switch byte = 0. Same shape as ExpressLRS OTA
    // pack_channels_4x10 output at mid-stick.
    //   ch1..ch4 = 0x200 = 512 (mid of 0..1023)
    const uint16_t ch = 0x200;
    size_t pos = 1;
    uint64_t buf = 0;
    int bits = 0;
    for (int i = 0; i < 4; ++i) {
        buf |= ((uint64_t)(ch & 0x3FF)) << bits;
        bits += 10;
    }
    while (bits >= 8 && pos + 1 < (size_t)payloadLen) {
        out[pos++] = (uint8_t)(buf & 0xFF);
        buf >>= 8; bits -= 8;
    }
    if (bits > 0 && pos + 1 < (size_t)payloadLen) {
        out[pos++] = (uint8_t)(buf & 0xFF);
    }
    // AUX / switches / TLM — zero out any remaining body bytes.
    while (pos + 2 < (size_t)payloadLen) out[pos++] = 0;

    // CRC-14 over bytes [0..payloadLen-3], seeded from UID.
    const uint16_t seed = elrs_crc14_seed_from_uid(uid);
    const uint16_t crc  = elrs_crc14(out, payloadLen - 2, seed);
    // Write 14-bit CRC big-endian in the last two bytes. OR in the packet
    // type bits in the top 2 slots of the MSB so the receiver-side OTA4
    // header-check can recover them (we use type 0 so both bits stay 0).
    out[payloadLen - 2] = (uint8_t)((crc >> 6) & 0xFF);
    out[payloadLen - 1] = (uint8_t)(((crc << 2) & 0xFC));
    return payloadLen;
}
