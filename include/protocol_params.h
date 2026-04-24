#ifndef PROTOCOL_PARAMS_H
#define PROTOCOL_PARAMS_H

#include <cstdint>

// ============================================================
// JJ Protocol Emulation Parameters — Single Source of Truth
// ============================================================
// All constants sourced from JJ_Protocol_Emulation_Reference_v2.md
// Every value has an inline citation to the v2 reference section.
// DO NOT duplicate these values in source files — #include this header.

// ============================================================
// 1. ELRS Channel Plans — v2 ref §3.1.1 [Ref P1]
// ============================================================
// Source: ExpressLRS FHSS.cpp lines 17-25
// Channel formula: freq = freqStart + index * (freqStop - freqStart) / (channels - 1)
// NOTE: divisor is (channels - 1), NOT channels — v2 §3.1.1 correction

struct ElrsDomain {
    const char* name;
    float       freqStartMHz;   // First channel center frequency
    float       freqStopMHz;    // Last channel center frequency
    uint8_t     channels;       // Number of hopping channels
    uint8_t     syncChannel;    // Midpoint sync channel index
    uint8_t     hopInterval;    // Standard hop interval (packets per hop)
};

// All 9 ELRS domains — v2 ref §3.1.1, §3.1.3 [Ref P1]
static const ElrsDomain ELRS_DOMAINS[] = {
    // name       start     stop     ch  sync  hopInt
    { "FCC915",  903.500f, 926.900f, 40,  20,   4 },  // v2 §3.1.1 — US FCC §15.247
    { "AU915",   915.500f, 926.900f, 20,  10,   8 },  // v2 §3.1.1 — ACMA AS/NZS 4268
    { "EU868",   863.275f, 869.575f, 13,   6,   8 },  // v2 §3.1.1 — ETSI EN 300 220
    { "IN866",   865.375f, 866.950f,  4,   2,   8 },  // v2 §3.1.1 — WPC India; hop interval 8 (ELRS rx_main.cpp groups IN866 with EU868)
    { "AU433",   433.420f, 434.420f,  3,   1,  36 },  // v2 §3.1.1, §3.1.3
    { "EU433",   433.100f, 434.450f,  3,   1,  36 },  // v2 §3.1.1, §3.1.3
    { "US433",   433.250f, 438.000f,  8,   4,  36 },  // v2 §3.1.1, §3.1.3
    { "US433W",  423.500f, 438.000f, 20,  10,  36 },  // v2 §3.1.1, §3.1.3
    { "ISM2G4", 2400.400f,2479.400f, 80,  40,   4 },  // v2 §3.1.1 — 2.4 GHz (not usable on SX1262)
};

static const uint8_t ELRS_DOMAIN_COUNT = sizeof(ELRS_DOMAINS) / sizeof(ELRS_DOMAINS[0]);

// Domain indices for direct access
enum ElrsDomainId {
    ELRS_DOMAIN_FCC915 = 0,
    ELRS_DOMAIN_AU915  = 1,
    ELRS_DOMAIN_EU868  = 2,
    ELRS_DOMAIN_IN866  = 3,
    ELRS_DOMAIN_AU433  = 4,
    ELRS_DOMAIN_EU433  = 5,
    ELRS_DOMAIN_US433  = 6,
    ELRS_DOMAIN_US433W = 7,
    ELRS_DOMAIN_ISM2G4 = 8,
};

// Channel frequency calculation — v2 ref §3.1.1
// freq = freqStart + index * (freqStop - freqStart) / (channels - 1)
static inline float elrsChanFreq(const ElrsDomain& d, uint8_t chanIndex) {
    if (d.channels <= 1) return d.freqStartMHz;
    return d.freqStartMHz + (chanIndex * (d.freqStopMHz - d.freqStartMHz) / (d.channels - 1));
}

// ============================================================
// 2. ELRS Air Rate Modes (900 MHz) — v2 ref §3.1.2 [Ref P2, P3]
// ============================================================
// Source: ELRS Lua docs, Betaflight expresslrs_common.c

struct ElrsAirRate {
    const char* name;
    uint16_t    rateHz;         // Packet rate in Hz
    uint8_t     sf;             // Spreading factor (5-12)
    uint32_t    bwHz;           // Bandwidth in Hz (500000 = BW500)
    uint8_t     cr;             // Coding rate denominator (7 = CR 4/7) — v2 §3.1.2
    uint8_t     preambleLen;    // Preamble symbols — v2 §3.1.2: ELRS uses 6
    uint8_t     payloadLen;     // Payload bytes
    uint8_t     hopInterval;    // Packets per hop (standard); DVDA overrides to 2
    bool        isDvda;         // DVDA mode flag — v2 §3.1.2 [Ref P19]
};

// All 6 ELRS 900 MHz air rate modes — v2 ref §3.1.2
static const ElrsAirRate ELRS_AIR_RATES[] = {
    // name    rate  SF  BW       CR pream  payload hopInt dvda
    { "200Hz",  200,  6, 500000,  7,   6,     8,     4,  false },  // v2 §3.1.2 — racer
    { "100Hz",  100,  7, 500000,  7,   6,    10,     4,  false },  // v2 §3.1.2 — balanced
    { "50Hz",    50,  8, 500000,  7,   6,    10,     4,  false },  // v2 §3.1.2 — long range
    { "25Hz",    25,  9, 500000,  7,   6,    10,     4,  false },  // v2 §3.1.2 — ultra LR
    { "D250",   250,  6, 500000,  7,   6,     8,     2,  true  },  // v2 §3.1.2 — DVDA [Ref P19]
    { "D500",   500,  5, 500000,  7,   6,     8,     2,  true  },  // v2 §3.1.2 — DVDA [Ref P19]
};

static const uint8_t ELRS_AIR_RATE_COUNT = sizeof(ELRS_AIR_RATES) / sizeof(ELRS_AIR_RATES[0]);

// Air rate indices for direct access
enum ElrsAirRateId {
    ELRS_RATE_200HZ = 0,
    ELRS_RATE_100HZ = 1,
    ELRS_RATE_50HZ  = 2,
    ELRS_RATE_25HZ  = 3,
    ELRS_RATE_D250  = 4,
    ELRS_RATE_D500  = 5,
};

// ============================================================
// 3. ELRS FHSS Sequence Generation — v2 ref §3.1.6 [Ref P4, P5]
// ============================================================
// Real ELRS LCG constants confirmed by NCC Group and GNU Radio research.
// NOT the Knuth constants (1664525, 1013904223) used in JJ v1.

static const uint32_t ELRS_LCG_MULTIPLIER = 0x343FD;     // v2 §3.1.6 [Ref P4, P5]
static const uint32_t ELRS_LCG_INCREMENT  = 0x269EC3;    // v2 §3.1.6 [Ref P4, P5]

// ============================================================
// 4. LoRa Sync Words — v2 ref §6.3 [Ref S1, §6.1.6]
// ============================================================

static const uint8_t SYNC_WORD_ELRS       = 0x12;  // v2 §6.3 — private LoRa network
static const uint8_t SYNC_WORD_LORAWAN    = 0x34;  // v2 §6.3 — public LoRa network
static const uint8_t SYNC_WORD_MESHTASTIC = 0x2B;  // v2 §6.3 — Meshtastic private

// ============================================================
// 5. TBS Crossfire Parameters — v2 ref §3.2 [Ref P6, P7]
// ============================================================

struct CrossfireBand {
    const char* name;
    float       freqStartMHz;
    float       freqStopMHz;
    uint8_t     channels;       // Estimated
    float       chanSpacingMHz;
};

// Crossfire bands — v2 ref §3.2.1 [Ref P7]
// NOTE: EU868 is 863-870, NOT 850-870 — corrected per ETSI limits (v2 §3.2.1)
static const CrossfireBand CRSF_BANDS[] = {
    { "915", 902.0f, 928.0f, 100, 0.260f },  // v2 §3.2.1 — US FCC
    { "868", 863.0f, 870.0f,  27, 0.260f },  // v2 §3.2.1 — EU ETSI (corrected from 850)
};

enum CrsfBandId {
    CRSF_BAND_915 = 0,
    CRSF_BAND_868 = 1,
};

// Crossfire modulation modes — v2 ref §3.2.2, §3.2.3
static const float    CRSF_FSK_BITRATE_KBPS    = 85.1f;   // v2 §3.2.3 [Ref P6]
static const float    CRSF_FSK_DEVIATION_KHZ    = 42.48f;  // g3gg0.de SPI sniffing of SX1272 register writes (TBS Crossfire 2021 RE)
static const uint16_t CRSF_FSK_RATE_HZ          = 150;     // v2 §3.2.3 — 150 Hz packet rate
static const uint32_t CRSF_FSK_PACKET_INTERVAL_US = 6667;  // 1000000 / 150 Hz
static const uint16_t CRSF_LORA_RATE_HZ         = 50;      // v2 §3.2.3 — 50 Hz LoRa mode
static const uint32_t CRSF_LORA_PACKET_INTERVAL_US = 20000; // 1000000 / 50 Hz

// ============================================================
// 6. SiK Radio Parameters — v2 ref §3.3 [Ref P9, P10, P11]
// ============================================================

struct SikBand {
    const char* name;
    float       minFreqMHz;
    float       maxFreqMHz;
    uint8_t     defaultChannels;
};

static const SikBand SIK_BANDS[] = {
    { "US915", 915.0f, 928.0f, 50 },    // v2 §3.3.1 [Ref P9]
    { "EU868", 868.0f, 869.0f,  7 },    // v2 §3.3.1 — 1 MHz band / ~111 kHz per channel at 64 kbps GFSK (1000/(7+2))
};

enum SikBandId {
    SIK_BAND_US915 = 0,
    SIK_BAND_EU868 = 1,
};

// SiK channel calculation: width = (MAX-MIN) / (NUM+2), with guard bands
// channel[n] = MIN + guardDelta + n * width + netidSkew
// v2 ref §3.3.2 [Ref P10]
static inline float sikChanFreq(const SikBand& b, uint8_t numChannels,
                                uint8_t chanIndex, uint8_t netId) {
    if (numChannels == 0) return b.minFreqMHz;
    float widthMHz = (b.maxFreqMHz - b.minFreqMHz) / (numChannels + 2);
    float guardDelta = widthMHz / 2.0f;
    float netidSkew = (float)(netId % 256) * widthMHz / 256.0f;
    return b.minFreqMHz + guardDelta + (chanIndex * widthMHz) + netidSkew;
}

// SiK default parameters — v2 ref §3.3.1 [Ref P9, P11]
static const float    SIK_DEFAULT_AIR_SPEED_KBPS = 64.0f;    // v2 §3.3.1
static const uint8_t  SIK_DEFAULT_NETID          = 25;        // v2 §3.3.1
static const int8_t   SIK_DEFAULT_POWER_DBM      = 20;        // v2 §3.3.1

// SiK air speed presets — v2 ref §3.3.1
static const float SIK_AIR_SPEEDS_KBPS[] = { 4.0f, 64.0f, 125.0f, 250.0f };
static const uint8_t SIK_AIR_SPEED_COUNT = sizeof(SIK_AIR_SPEEDS_KBPS) / sizeof(SIK_AIR_SPEEDS_KBPS[0]);

// ============================================================
// 7. LoRaWAN US915 — v2 ref §4.1 [Ref R6, R7]
// ============================================================

// Sub-Band 2 uplink frequencies (TTN/Helium default) — v2 ref §4.1
static const float LORAWAN_US915_SB2[] = {
    903.9f, 904.1f, 904.3f, 904.5f,    // v2 §4.1 [Ref R7]
    904.7f, 904.9f, 905.1f, 905.3f,
};
static const uint8_t LORAWAN_US915_SB2_COUNT = 8;

// LoRaWAN fixed RX2 downlink — v2 ref §4.1
static const float LORAWAN_US915_RX2_FREQ = 923.3f;  // v2 §4.1

// LoRaWAN radio parameters
static const uint8_t  LORAWAN_PREAMBLE_LEN    = 8;    // v2 §6.2 [Ref R6, §4.1.1]
static const uint32_t LORAWAN_BW_HZ           = 125000; // Standard BW125
static const uint32_t LORAWAN_BW500_HZ        = 500000; // DR4 mode BW500

// ============================================================
// 8. LoRaWAN EU868 — v2 ref §4.2 [Ref R6, R2]
// ============================================================

// Mandatory EU868 channels — v2 ref §4.2
static const float LORAWAN_EU868_CHANNELS[] = {
    868.1f, 868.3f, 868.5f,            // v2 §4.2 — mandatory
};
static const uint8_t LORAWAN_EU868_CH_COUNT = 3;

// EU868 downlink RX2 — v2 ref §4.2
static const float   LORAWAN_EU868_RX2_FREQ = 869.525f;
static const uint8_t LORAWAN_EU868_RX2_SF   = 9;

// ============================================================
// 9. Meshtastic Parameters — v2 ref §4.3 [Ref R8]
// ============================================================

static const uint8_t  MESHTASTIC_PREAMBLE_LEN = 16;   // v2 §4.3 — 16 symbols (longer than ELRS/LoRaWAN)
// Sync word defined above as SYNC_WORD_MESHTASTIC = 0x2B
// US frequency range: 902.0-928.0 MHz, ~104 channels — v2 §4.3
static const float    MESHTASTIC_US_FREQ_START = 902.0f;
static const float    MESHTASTIC_US_FREQ_END   = 928.0f;
static const uint8_t  MESHTASTIC_US_CHANNELS   = 104;  // v2 §4.3

// ============================================================
// 10. mLRS Parameters — grounded from olliw42/mLRS (fhss.h + common_conf.h)
// ============================================================
// mLRS 915 MHz FCC: 43 channels, 902-928 MHz, LFSR hop sequence.
//   19 Hz: SF7/BW500 (-112 dBm sensitivity)
//   31 Hz: SF6/BW500 (-108 dBm sensitivity)
//   50 Hz: GFSK ~64 kbps (bitrate/deviation undocumented upstream —
//          SiK-like approximation in mlrs_sim.cpp)
// Frame length: 91 bytes (FRAME_TX_RX_LEN)
// Sync word: bind-phrase derived; we use 0x12 (private LoRa) as proxy.
// Preamble: 8 symbols (mLRS default).

struct MlrsMode {
    const char* name;
    uint16_t    rateHz;
    bool        isLoRa;
    uint8_t     sf;             // LoRa modes only (0 for FSK)
};

// Effective SF values live in mlrs_sim.cpp's MLRS_MODE_PARAMS table; this
// enum array keeps name/rate/modulation-kind for the menu dispatcher.
static const MlrsMode MLRS_MODES[] = {
    { "19Hz",   19, true,  7 },    // SF7/BW500 (grounded)
    { "31Hz",   31, true,  6 },    // SF6/BW500 (grounded)
    { "50Hz",   50, false, 0 },    // GFSK ~64 kbps (approximated)
};

static const uint8_t MLRS_MODE_COUNT = sizeof(MLRS_MODES) / sizeof(MLRS_MODES[0]);

// ============================================================
// Common Constants
// ============================================================

// Maximum hop sequence length — v2 ref §3.1.6 [Ref P20]
// Real ELRS: (256 / freq_count) * freq_count, prevents uint8_t overflow
static inline uint8_t elrsSeqLength(uint8_t freqCount) {
    return (256 / freqCount) * freqCount;
}

#endif // PROTOCOL_PARAMS_H
