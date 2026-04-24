// ============================================================================
// XR1-side ExpressLRS CRC-14 (see header). Identical poly + behaviour to the
// T3S3 implementation in src/protocol_packets.cpp — kept as a small standalone
// ~30-line unit so the HOP engine can recompute per nonce without pulling
// the full protocol_packets module into the xr1-firmware build.
// ============================================================================

#include "elrs_crc14.h"

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
