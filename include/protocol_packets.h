#ifndef PROTOCOL_PACKETS_H
#define PROTOCOL_PACKETS_H

// ============================================================================
// Wire-authentic packet builders for the protocols JJ emulates.
//
// Everything here writes well-formed framing + CRCs so a detector that
// demodulates the bytes (as opposed to one that only looks at RF envelope)
// sees valid-looking drone traffic. Sources cited inline — if you change
// any constant here, chase the citation first.
// ============================================================================

#include <Arduino.h>
#include <stdint.h>
#include <stddef.h>

// --- CRC primitives --------------------------------------------------------
// CRC-8/DVB-S2, poly 0xD5 — used by TBS Crossfire (CRSF) framing.
uint8_t  crc8_dvbs2(const uint8_t *data, size_t len);

// MAVLink v2 X.25 CRC with a CRC_EXTRA byte mixed in at the end per
// the MAVLink reference implementation (crc_accumulate / crc_extra).
uint16_t mavlink_crc_x25(const uint8_t *data, size_t len, uint8_t crc_extra);

// ExpressLRS OTA CRC-14 — polynomial 0x2B5, seeded from the binding UID.
// Returns a 14-bit value (upper two bits of the uint16 are zero). Source:
// ExpressLRS src/lib/OTA/OTA.cpp GeneratePacketCRC() / Crc2Byte table.
uint16_t elrs_crc14(const uint8_t *data, size_t len, uint16_t seed);

// Derive the CRC-14 seed from a 6-byte binding UID (ELRS uses the last
// four bytes of the UID to seed the CRC init). Source: ExpressLRS
// OtaUpdateCrcInitFromUid().
uint16_t elrs_crc14_seed_from_uid(const uint8_t uid[6]);

// --- Packet builders -------------------------------------------------------
// Build a CRSF "RC Channels Packed" (type 0x16) frame into `out`. Channels
// are 16 × 11-bit values in CRSF units (172=min, 992=center, 1811=max).
// Returns the total frame length (always 26 bytes: 2-byte header + 24-byte
// body including CRC). Output must be at least 26 bytes.
size_t build_crsf_rc_channels_packed(uint8_t *out, const uint16_t channels[16]);

// Build a MAVLink v2 HEARTBEAT (msgid 0) frame for a quadrotor + ArduPilot
// + armed + active. Increments `seq` (uint8 wraparound) and writes an
// MAVLINK_STX2-framed packet into `out`. Returns total length (21 bytes).
size_t build_mavlink_heartbeat_v2(uint8_t *out, uint8_t &seq,
                                  uint8_t sysid = 1, uint8_t compid = 1);

// MAVLink v2 SYS_STATUS (msgid 1) — battery voltage (mV, default 12.6 V),
// current draw (-1 = unknown), remaining percent (default 75), and
// sensor presence/health bitmasks. Lets a SiK-aware receiver see the
// protocol's typical mixed-message traffic instead of only HEARTBEAT.
// Returns the frame length (42 bytes for a 31-byte payload).
size_t build_mavlink_sys_status_v2(uint8_t *out, uint8_t &seq,
                                   uint8_t sysid = 1, uint8_t compid = 1);

// Build an ExpressLRS OTA LoRa packet matching the 8-byte or 10-byte air
// rate modes. `payloadLen` must be 8 or 10. The first byte encodes the
// FHSS sync phase (upper 2 bits) + nonce (lower 6 bits); channel data
// fills the middle bytes; the last 2 bytes are the CRC-14 (upper 14 bits
// of the 16-bit output go in bytes n-2..n-1, with the top 2 bits cleared).
// `nonceCounter` increments each call (wraps at 64). `uid` is the binding
// UID used to seed the CRC.
size_t build_elrs_ota_packet(uint8_t *out, uint8_t payloadLen,
                             uint8_t &nonceCounter,
                             const uint8_t uid[6]);

// --- Shared defaults -------------------------------------------------------
// JJ test binding UID — fixed so all packets from a single JJ unit carry the
// same CRC seed. Not a real ELRS TX UID; chosen to be visibly "JJ test".
extern const uint8_t JJ_ELRS_TEST_UID[6];

#endif // PROTOCOL_PACKETS_H
