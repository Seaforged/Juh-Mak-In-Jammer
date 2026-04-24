// ============================================================================
// 2.4 GHz protocol profiles driven through the XR1 (LR1121) over the T3S3's
// UART link. Each start function builds a channel list + modulation config
// from the parameters in docs/JJ_Protocol_Emulation_Reference_v2.md (or the
// best-known approximation where the protocol is proprietary), then uses
// xr1_driver to push it to the XR1. Hopping runs autonomously on the XR1's
// side until xr1_modes.h's stop is called.
// ============================================================================

#include "xr1_modes.h"
#include "xr1_driver.h"
#include "protocol_packets.h"

#include <Arduino.h>
#include <string.h>

// ----- shared state --------------------------------------------------------
static Xr1ModesStatus s_status = {};

static void clearStatus() {
    memset(&s_status, 0, sizeof(s_status));
    s_status.modulation = "";
    s_status.label[0] = '\0';
}

// ----- ELRS 2.4 GHz channel plan (ISM2G4, v2 §3.1.1) -----------------------
// 80 channels, 1.0 MHz spacing, 2400.4 .. 2479.4 MHz, sync channel 40.
static constexpr float    ELRS2G4_FREQ_START = 2400.4f;
static constexpr float    ELRS2G4_FREQ_STEP  = 1.0f;
static constexpr uint8_t  ELRS2G4_CHANNELS   = 80;
static constexpr uint8_t  ELRS2G4_SYNC_CH    = 40;

static constexpr uint32_t ELRS_LCG_MULT = 0x343FDu;
static constexpr uint32_t ELRS_LCG_INC  = 0x269EC3u;
static constexpr uint32_t ELRS_HOP_SEED = 0xDEADBEEFu;

static inline float elrs2g4ChanFreq(uint8_t ch) {
    return ELRS2G4_FREQ_START + (float)ch * ELRS2G4_FREQ_STEP;
}

// Fisher-Yates shuffle using the real ELRS LCG constants, matching the
// sub-GHz elrs implementation in src/rf_modes.cpp. The sync channel is
// placed at slot 0; remaining slots are a shuffle of the 79 non-sync
// channels. The XR1's HOP command visits slots in order, so sync sees
// the air ~1/80 of the time — matches the real-ELRS hop_interval=1 case
// and is a close enough approximation for hop_interval=4 given detection
// tests.
static void elrsBuildHopList(float *out, uint8_t totalCh, uint8_t syncCh, uint32_t seed) {
    uint8_t nonSync[80];
    uint8_t cnt = 0;
    for (uint8_t i = 0; i < totalCh; i++) {
        if (i != syncCh) nonSync[cnt++] = i;
    }
    uint32_t rng = seed;
    for (uint8_t i = cnt - 1; i > 0; i--) {
        rng = rng * ELRS_LCG_MULT + ELRS_LCG_INC;
        uint8_t j = (rng >> 16) % (i + 1);
        uint8_t tmp = nonSync[i]; nonSync[i] = nonSync[j]; nonSync[j] = tmp;
    }
    out[0] = elrs2g4ChanFreq(syncCh);
    for (uint8_t i = 0; i < cnt; i++) {
        out[i + 1] = elrs2g4ChanFreq(nonSync[i]);
    }
}

// ----- ELRS 2.4 GHz air rates (v2 §3.1.2 port for ISM2G4) ------------------
struct Elrs2g4Rate {
    const char *label;        // "500Hz"
    uint8_t     sf;
    uint8_t     cr;           // denominator (5..8)
    uint8_t     preamble;
    uint8_t     payloadLen;
    uint16_t    rateHz;
    uint8_t     hopInterval;  // packets per channel
    uint32_t    pktIntervalUs;
};
// ExpressLRS upstream (src/src/common.cpp, SX1280/LR11xx 2.4 GHz table) uses:
//   500Hz = SF5 CR4/6 preamble12 payload8 hop4 interval2000us
//   250Hz = SF6 CR4/8 preamble14 payload8 hop4 interval4000us
//   150Hz = SF7 CR4/8 preamble12 payload8 hop4 interval6666us
//   50Hz  = SF8 CR4/8 preamble12 payload8 hop2 interval20000us
// The local docs had drifted to SF6/7/8/9 + preamble 6; this table restores
// the XR1 path to the actual 2.4 GHz ELRS LoRa profile family.
static const Elrs2g4Rate ELRS_2G4_RATES[] = {
    { "500Hz",  5, 6, 12, 8, 500, 4,  2000 },
    { "250Hz",  6, 8, 14, 8, 250, 4,  4000 },
    { "150Hz",  7, 8, 12, 8, 150, 4,  6666 },
    { "50Hz",   8, 8, 12, 8,  50, 2, 20000 },
};
static constexpr uint8_t ELRS_2G4_RATE_COUNT =
    sizeof(ELRS_2G4_RATES) / sizeof(ELRS_2G4_RATES[0]);

// Power default for all 2.4 GHz LoRa protocols (matches XR1_POWER_2G4_DBM[0]
// in the XR1 firmware — index 0 = 12 dBm).
static constexpr int8_t XR1_DEFAULT_PWR_DBM = 12;

// ----- common plumbing -----------------------------------------------------
// Sends the XR1 a fresh STOP then a fresh config. Returns false and stops
// the XR1 if any step fails so we never leave the radio half-configured.
static bool pushFreqPowerAndHop(float *channels, uint8_t count, uint16_t dwellMs,
                                float startFreqForSetup, int8_t pwrDbm) {
    if (!xr1Stop())                                  return false;
    if (!xr1SetFreq(startFreqForSetup))              return false;
    if (!xr1SetPower(pwrDbm))                        return false;
    if (!xr1StartHop(channels, count, dwellMs))      return false;
    return true;
}

// ----- public: ELRS 2.4 GHz ------------------------------------------------
bool xr1ModeElrs2g4Start(uint8_t rateIdx) {
    if (rateIdx >= ELRS_2G4_RATE_COUNT) return false;
    const Elrs2g4Rate &r = ELRS_2G4_RATES[rateIdx];

    // Build the 80-channel list: slot 0 = sync, slots 1..79 = shuffle.
    float channels[ELRS2G4_CHANNELS];
    elrsBuildHopList(channels, ELRS2G4_CHANNELS, ELRS2G4_SYNC_CH, ELRS_HOP_SEED);

    // Dwell: hop_interval packets spent on each channel -> dwell_ms.
    uint16_t dwellMs = (uint16_t)((r.pktIntervalUs * r.hopInterval + 999UL) / 1000UL);
    if (dwellMs == 0) dwellMs = 1;

    // Configure modulation first (LoRa on the start frequency), then hop.
    const float startFreq = channels[0];
    if (!xr1Stop())                                                 return false;
    if (!xr1SetFreq(startFreq))                                     return false;
    if (!xr1SetLoRaEx(r.sf, 812.5f, r.cr, r.preamble, true, r.payloadLen))    return false;
    if (!xr1SetPower(XR1_DEFAULT_PWR_DBM))                          return false;

    // Push a wire-authentic ELRS OTA template the XR1 will roll the nonce
    // byte on for each TX. CRC-14 is computed here with the JJ test UID;
    // byte 0 still rolls on the XR1 so the header-byte nonce matches what
    // a real ELRS TX produces per packet.
    uint8_t elrsTemplate[10];
    uint8_t nonce = 0;
    const size_t tmplLen = build_elrs_ota_packet(elrsTemplate, r.payloadLen,
                                                 nonce, JJ_ELRS_TEST_UID);
    if (tmplLen == r.payloadLen) {
        xr1SetPayload(elrsTemplate, (uint8_t)tmplLen);
    }

    if (!xr1StartHopEx(channels, ELRS2G4_CHANNELS, dwellMs,
                       r.pktIntervalUs, r.hopInterval, r.payloadLen)) return false;

    clearStatus();
    s_status.running   = true;
    s_status.startMhz  = elrs2g4ChanFreq(0);
    s_status.stopMhz   = elrs2g4ChanFreq(ELRS2G4_CHANNELS - 1);
    s_status.channels  = ELRS2G4_CHANNELS;
    s_status.dwellMs   = dwellMs;
    s_status.powerDbm  = XR1_DEFAULT_PWR_DBM;
    s_status.modulation = "LoRa BW812";
    snprintf(s_status.label, sizeof(s_status.label),
             "ELRS-2G4 %s SF%u", r.label, (unsigned)r.sf);

    Serial.printf("[XR1-MODE] ELRS-2G4 %s: SF%u/BW812.5/CR4-%u preamble=%u implicit len=%u, %uch %.1f-%.1fMHz, dwell=%ums, hopEvery=%u, pwr=%d dBm\n",
                  r.label, (unsigned)r.sf, (unsigned)r.cr, (unsigned)r.preamble,
                  (unsigned)r.payloadLen,
                  (unsigned)ELRS2G4_CHANNELS, s_status.startMhz, s_status.stopMhz,
                  (unsigned)dwellMs, (unsigned)r.hopInterval, (int)XR1_DEFAULT_PWR_DBM);
    return true;
}

// ----- public: ImmersionRC Ghost (approximate) -----------------------------
bool xr1ModeGhostStart() {
    // Ghost is proprietary; use LoRa SF7/BW812.5, 80 channels spanning the
    // 2.4 GHz ISM band, pseudo-random hop (same LCG seed = distinct from
    // ELRS because no sync channel is carved out). ~250 Hz packet rate,
    // hop every packet -> dwell ~4 ms.
    constexpr uint8_t  CH_COUNT = 80;
    constexpr uint16_t DWELL_MS = 4;

    // Shuffle all 80 channels using the same LCG (no sync channel here).
    uint8_t idx[CH_COUNT];
    for (uint8_t i = 0; i < CH_COUNT; i++) idx[i] = i;
    uint32_t rng = 0xC0FFEE5Au;   // distinct seed from ELRS
    for (uint8_t i = CH_COUNT - 1; i > 0; i--) {
        rng = rng * ELRS_LCG_MULT + ELRS_LCG_INC;
        uint8_t j = (rng >> 16) % (i + 1);
        uint8_t tmp = idx[i]; idx[i] = idx[j]; idx[j] = tmp;
    }
    float channels[CH_COUNT];
    for (uint8_t i = 0; i < CH_COUNT; i++) channels[i] = elrs2g4ChanFreq(idx[i]);

    if (!xr1Stop())                                         return false;
    if (!xr1SetFreq(channels[0]))                           return false;
    if (!xr1SetLoRa(7, 812.5f, 5))                          return false;
    if (!xr1SetPower(XR1_DEFAULT_PWR_DBM))                  return false;
    if (!xr1StartHop(channels, CH_COUNT, DWELL_MS))         return false;

    clearStatus();
    s_status.running    = true;
    s_status.startMhz   = 2400.4f;
    s_status.stopMhz    = 2479.4f;
    s_status.channels   = CH_COUNT;
    s_status.dwellMs    = DWELL_MS;
    s_status.powerDbm   = XR1_DEFAULT_PWR_DBM;
    s_status.modulation = "LoRa SF7";
    strncpy(s_status.label, "Ghost 2G4 (approx)", sizeof(s_status.label));

    Serial.printf("[XR1-MODE] Ghost (approx): LoRa SF7/BW812.5, %uch, dwell=%ums [APPROXIMATE]\n",
                  (unsigned)CH_COUNT, (unsigned)DWELL_MS);
    return true;
}

// ----- GFSK helpers --------------------------------------------------------
// The T3S3's XR1 driver sends FSK commands after LoRa ones would have been.
// We funnel the "set up GFSK on an N-channel span" pattern through one
// helper to keep FrSky/FlySky/DJI short and consistent.
static bool startGfskFhss(const char *label, float startMhz, float stopMhz,
                          uint8_t chCount, uint16_t dwellMs,
                          float brKbps, float devKhz,
                          bool shuffle, const char *modLabel) {
    if (chCount == 0 || chCount > 80) return false;

    float spacing = (chCount > 1) ? ((stopMhz - startMhz) / (float)(chCount - 1)) : 0.0f;
    uint8_t idx[80];
    for (uint8_t i = 0; i < chCount; i++) idx[i] = i;
    if (shuffle) {
        uint32_t rng = 0xA5A5A5A5u ^ (uint32_t)chCount;
        for (uint8_t i = chCount - 1; i > 0; i--) {
            rng = rng * ELRS_LCG_MULT + ELRS_LCG_INC;
            uint8_t j = (rng >> 16) % (i + 1);
            uint8_t tmp = idx[i]; idx[i] = idx[j]; idx[j] = tmp;
        }
    }
    float channels[80];
    for (uint8_t i = 0; i < chCount; i++) channels[i] = startMhz + idx[i] * spacing;

    if (!xr1Stop())                                 return false;
    if (!xr1SetFreq(channels[0]))                   return false;
    if (!xr1SetFSK(brKbps, devKhz))                 return false;
    if (!xr1SetPower(XR1_DEFAULT_PWR_DBM))          return false;
    if (!xr1StartHop(channels, chCount, dwellMs))   return false;

    clearStatus();
    s_status.running    = true;
    s_status.startMhz   = startMhz;
    s_status.stopMhz    = stopMhz;
    s_status.channels   = chCount;
    s_status.dwellMs    = dwellMs;
    s_status.powerDbm   = XR1_DEFAULT_PWR_DBM;
    s_status.modulation = modLabel;
    strncpy(s_status.label, label, sizeof(s_status.label) - 1);
    s_status.label[sizeof(s_status.label) - 1] = '\0';

    Serial.printf("[XR1-MODE] %s: GFSK %.0fkbps/%.0fkHz, %uch %.1f-%.1fMHz, dwell=%ums, pwr=%d dBm\n",
                  label, brKbps, devKhz,
                  (unsigned)chCount, startMhz, stopMhz,
                  (unsigned)dwellMs, (int)XR1_DEFAULT_PWR_DBM);
    return true;
}

// ----- public: FrSky ACCST D16 footprint -----------------------------------
bool xr1ModeFrskyStart() {
    // FrSky ACCST D16 16-ch mode: GFSK 250 kbps / 50 kHz dev, 47 channels
    // over 2.4 GHz ISM band, 18 ms hop interval. Source:
    // DIY-Multiprotocol-TX-Module Pktfrsky_ACCST branch.
    return startGfskFhss("FrSky D16 footprint",
                         2403.5f, 2479.5f,  // ~47ch × ~1.65 MHz spacing ≈ 76 MHz span
                         47, 18,
                         250.0f, 50.0f,
                         true,              // shuffled hop
                         "GFSK 250k/50k");
}

// ----- public: FlySky AFHDS 2A footprint -----------------------------------
bool xr1ModeFlyskyStart() {
    // AFHDS 2A: GFSK 250 kbps, 16 channels over 2.4 GHz, ~1.5 ms hop.
    // 1.5 ms is too fast for UART round-trip (PING alone takes ~10 ms),
    // so we clamp to 5 ms and label [APPROXIMATE].
    const bool ok = startGfskFhss("FlySky AFHDS-2A footprint [APPROX]",
                                  2408.0f, 2473.0f,  // 16ch × 4.33 MHz spacing
                                  16, 5,
                                  250.0f, 50.0f,
                                  true,
                                  "GFSK 250k/50k");
    if (ok) {
        Serial.println("[XR1-MODE] note: real AFHDS-2A hops every 1.5 ms; clamped to 5 ms here");
    }
    return ok;
}

// ----- public: DJI OcuSync energy approximation ----------------------------
bool xr1ModeDjiEnergyStart() {
    // NOT real OcuSync — that's OFDM. This only approximates the energy
    // signature: GFSK bursts hopping across 2.4 GHz on ~20 channels at 50 ms
    // dwell, random order. Purpose is to trigger energy-based detectors.
    const bool ok = startGfskFhss("DJI OcuSync energy [APPROX]",
                                  2400.5f, 2481.5f,
                                  20, 50,
                                  250.0f, 50.0f,
                                  true,
                                  "GFSK 250k/50k");
    if (ok) {
        Serial.println("[XR1-MODE] note: not real OcuSync (OFDM); energy-signature approximation only");
    }
    return ok;
}

// ----- public: Generic configurable ----------------------------------------
bool xr1ModeGenericStart(const Xr1GenericCfg &cfg) {
    if (cfg.channelCount == 0 || cfg.channelCount > 80) return false;

    float channels[80];
    for (uint8_t i = 0; i < cfg.channelCount; i++) {
        channels[i] = cfg.startMhz + i * cfg.spacingMhz;
    }
    const float startFreq = channels[0];
    const float endFreq   = channels[cfg.channelCount - 1];

    if (!xr1Stop())                              return false;
    if (!xr1SetFreq(startFreq))                  return false;
    if (cfg.isLora) {
        if (!xr1SetLoRa(cfg.sf, cfg.bwKhz, cfg.cr)) return false;
    } else {
        if (!xr1SetFSK(cfg.brKbps, cfg.devKhz))   return false;
    }
    if (!xr1SetPower(cfg.powerDbm))              return false;
    if (!xr1StartHop(channels, cfg.channelCount, cfg.dwellMs)) return false;

    clearStatus();
    s_status.running   = true;
    s_status.startMhz  = startFreq;
    s_status.stopMhz   = endFreq;
    s_status.channels  = cfg.channelCount;
    s_status.dwellMs   = cfg.dwellMs;
    s_status.powerDbm  = cfg.powerDbm;
    if (cfg.isLora) {
        static char modBuf[24];
        snprintf(modBuf, sizeof(modBuf), "LoRa SF%u/BW%u",
                 (unsigned)cfg.sf, (unsigned)cfg.bwKhz);
        s_status.modulation = modBuf;
    } else {
        static char modBuf[24];
        snprintf(modBuf, sizeof(modBuf), "GFSK %.0fk/%.0fk",
                 cfg.brKbps, cfg.devKhz);
        s_status.modulation = modBuf;
    }
    strncpy(s_status.label, "Generic 2G4 FHSS", sizeof(s_status.label));

    Serial.printf("[XR1-MODE] Generic: %s %.1f-%.1fMHz (%u ch x %.2fMHz) dwell=%ums pwr=%d\n",
                  s_status.modulation, startFreq, endFreq,
                  (unsigned)cfg.channelCount, cfg.spacingMhz,
                  (unsigned)cfg.dwellMs, (int)cfg.powerDbm);
    return true;
}

// ----- public: stop --------------------------------------------------------
void xr1ModesStop() {
    xr1Stop();
    if (s_status.running) {
        Serial.printf("[XR1-MODE] stopped (%s)\n", s_status.label);
    }
    clearStatus();
}

Xr1ModesStatus xr1ModesGetStatus() {
    return s_status;
}
