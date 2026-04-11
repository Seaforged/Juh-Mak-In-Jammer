#pragma once

// ============================================================================
// Protocol-specific RF profiles (Phase 3).
//
// Channel plans, modulation parameters, hop sequences, and packet-rate tables
// for each emulated drone protocol. Every value in this file (eventually)
// must cite docs/JJ_Protocol_Emulation_Reference_v2.md or a primary source —
// no approximations.
//
// Phase 1 only declares the struct shapes so downstream .cpp files compile.
// ============================================================================

#include <Arduino.h>

enum class RfModulation : uint8_t {
    LORA,
    GFSK,
    LR_FHSS,
};

struct RfLoraParams {
    uint8_t  spreadingFactor;
    float    bandwidthKhz;
    uint8_t  codingRate;
    uint16_t preambleSymbols;
    uint8_t  syncWord;
};

struct RfFskParams {
    float    bitrateKbps;
    float    deviationKhz;
    float    rxBandwidthKhz;
    uint16_t preambleBits;
};

struct RfProfile {
    const char     *name;
    RfModulation    modulation;
    const float    *channelsMhz;
    uint8_t         numChannels;
    uint16_t        dwellMicros;
    int8_t          powerDbm;
    RfLoraParams    lora;
    RfFskParams     fsk;
};

// Lookup and activation stubs (implemented in Phase 3).
const RfProfile *rfProfileByName(const char *name);
