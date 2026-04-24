#pragma once

// ============================================================================
// XR1-side ExpressLRS OTA CRC-14 (polynomial 0x2B5) for recomputing the CRC
// after nonce rolling in the HOP engine. The T3S3 builds the initial wire-
// authentic packet with CRC for nonce=0; this function lets the XR1 update
// the CRC for each subsequent nonce so every transmitted packet carries a
// valid CRC and a content-parsing detector accepts them all.
//
// Same implementation as src/protocol_packets.cpp elrs_crc14; kept standalone
// on the XR1 side because the T3S3 source tree isn't pulled into xr1-firmware.
// ============================================================================

#include <stdint.h>
#include <stddef.h>

uint16_t elrs_crc14(const uint8_t *data, size_t len, uint16_t seed);
