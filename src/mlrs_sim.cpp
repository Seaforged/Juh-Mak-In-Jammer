#include <Arduino.h>
#include "mlrs_sim.h"
#include "rf_modes.h"       // for rfGetPower()
#include "protocol_params.h"

// ============================================================
// mLRS Simulation — v2 ref §3.4 [Ref P12, P14]
// ============================================================
// mLRS uses symmetric TX/RX alternation: each frame slot contains
// either a TX or RX burst, and both radios hop in lockstep.
// Effective hop rate = packet_rate / 2 (since TX and RX alternate).
//
// NOTE: Many parameters are [VERIFY] — marked with TODO comments.
// These will be corrected when the mLRS repo is cloned and read.

static SX1262 *_radio = nullptr;
static uint8_t _modeIdx = 0;  // default: 19 Hz LoRa

// TODO [VERIFY]: mLRS 915 band channel count — estimated 20 — v2 §3.4
static const uint8_t  MLRS_NUM_CHANNELS  = 20;
static const float    MLRS_BAND_START    = 902.0f;   // TODO [VERIFY] exact start freq
static const float    MLRS_BAND_END      = 928.0f;   // TODO [VERIFY] exact end freq

// Per-mode radio parameters — estimated, all marked [VERIFY]
struct MlrsModeParams {
    uint8_t  sf;           // TODO [VERIFY] per mode
    uint32_t bwHz;         // TODO [VERIFY] per mode
    float    fskBitrate;   // 0 = LoRa mode, >0 = FSK mode
    uint8_t  payloadLen;
};

// TODO [VERIFY]: SF and BW for each mode — estimated to fit frame timing
static const MlrsModeParams MLRS_MODE_PARAMS[] = {
    { 8, 500000,  0.0f, 10 },  // 19 Hz LoRa — ~11ms ToA fits in 52ms frame
    { 7, 500000,  0.0f, 10 },  // 31 Hz LoRa — ~6.7ms ToA fits in 32ms frame
    { 0, 0,      64.0f, 32 },  // 50 Hz FSK  — GFSK 64 kbps, similar to SiK
};

// Channel table
static float _mlrsChannels[MLRS_NUM_CHANNELS];
static uint8_t _hopSeq[MLRS_NUM_CHANNELS];
static uint8_t _hopIdx = 0;

// State
static bool     _running     = false;
static uint32_t _packetCount = 0;
static uint32_t _hopCount    = 0;
static float    _currentMHz  = 0;
static bool     _isTxPhase   = true;   // alternates TX/RX
static unsigned long _lastFrameUs = 0;

// Dummy payloads
static const uint8_t MLRS_LORA_PAYLOAD[] = { 0x4D, 0x4C, 0x52, 0x53, 0x00, 0x00, 0x00, 0x00, 0x55, 0xAA };
static uint8_t _mlrsFskPayload[32];

static void mlrsBuildChannelTable() {
    // Simple linear spacing across the band — TODO [VERIFY] actual mLRS channel plan
    for (uint8_t i = 0; i < MLRS_NUM_CHANNELS; i++) {
        if (MLRS_NUM_CHANNELS > 1) {
            _mlrsChannels[i] = MLRS_BAND_START +
                (i * (MLRS_BAND_END - MLRS_BAND_START) / (MLRS_NUM_CHANNELS - 1));
        } else {
            _mlrsChannels[i] = (MLRS_BAND_START + MLRS_BAND_END) / 2.0f;
        }
    }
}

static void mlrsBuildHopSequence(uint32_t seed) {
    for (uint8_t i = 0; i < MLRS_NUM_CHANNELS; i++) _hopSeq[i] = i;
    uint32_t rng = seed;
    for (uint8_t i = MLRS_NUM_CHANNELS - 1; i > 0; i--) {
        rng = rng * 1664525UL + 1013904223UL;
        uint8_t j = rng % (i + 1);
        uint8_t tmp = _hopSeq[i];
        _hopSeq[i] = _hopSeq[j];
        _hopSeq[j] = tmp;
    }
}

// ============================================================
// Public API
// ============================================================

void mlrsInit(SX1262 *radio) {
    _radio = radio;
    _running = false;
}

void mlrsSetMode(uint8_t modeIndex) {
    if (modeIndex < MLRS_MODE_COUNT) {
        _modeIdx = modeIndex;
    }
}

void mlrsStart() {
    if (!_radio) return;

    const MlrsMode& mode = MLRS_MODES[_modeIdx];
    const MlrsModeParams& params = MLRS_MODE_PARAMS[_modeIdx];

    mlrsBuildChannelTable();
    mlrsBuildHopSequence(0x6D4C5253);  // "mLRS" as seed
    _hopIdx = 0;
    _packetCount = 0;
    _hopCount = 0;
    _isTxPhase = true;

    int8_t pwr = rfGetPower();

    _radio->reset();
    delay(100);

    int state;
    if (mode.isLoRa) {
        // LoRa mode — CAD-detectable by SENTRY-RF
        state = _radio->begin(
            _mlrsChannels[_hopSeq[0]],
            (float)(params.bwHz / 1000),
            params.sf,
            7,                      // CR 4/7 (estimated, TODO [VERIFY])
            SYNC_WORD_ELRS,         // TODO [VERIFY] mLRS sync word — using 0x12 as placeholder
            pwr,
            6,                      // preamble (TODO [VERIFY])
            1.8, false
        );
        if (state == RADIOLIB_ERR_NONE) _radio->explicitHeader();
    } else {
        // FSK mode — not CAD-detectable
        state = _radio->beginFSK(
            _mlrsChannels[_hopSeq[0]],
            params.fskBitrate,
            params.fskBitrate / 4.0f,  // deviation ~bitrate/4
            156.2,
            pwr, 32, 1.8, false
        );
        // Generate random FSK payload
        for (uint8_t i = 0; i < sizeof(_mlrsFskPayload); i++) {
            _mlrsFskPayload[i] = (uint8_t)(esp_random() & 0xFF);
        }
    }

    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("[mLRS] radio config FAILED (error %d)\n", state);
        _running = false;
        return;
    }

    _currentMHz = _mlrsChannels[_hopSeq[0]];
    _running = true;
    _lastFrameUs = micros();

    // Transmit first packet
    int16_t txRc;
    if (mode.isLoRa) {
        txRc = _radio->startTransmit(MLRS_LORA_PAYLOAD, params.payloadLen);
    } else {
        txRc = _radio->transmit(_mlrsFskPayload, params.payloadLen);
    }
    if (txRc == RADIOLIB_ERR_NONE) _packetCount++;

    // Protocol info output — v2 §7.2
    // Symmetric hopping: effective hop rate = packet_rate / 2
    uint32_t frameUs = 1000000UL / mode.rateHz;
    float effectiveHopsPerSec = mode.rateHz / 2.0f;
    uint32_t dwellMs = (2 * frameUs) / 1000;  // TX + RX frame before hop
    Serial.printf("[mLRS-915] %uch %.0f-%.0fMHz %s %uHz symmetric\n",
                  MLRS_NUM_CHANNELS, MLRS_BAND_START, MLRS_BAND_END,
                  mode.isLoRa ? "LoRa" : "FSK", mode.rateHz);
    if (mode.isLoRa) {
        Serial.printf("  SF%u/BW%lu  ", params.sf, params.bwHz / 1000);
    } else {
        Serial.printf("  GFSK@%.0fkbps  ", params.fskBitrate);
    }
    Serial.printf("Dwell: %lums/freq  Hops: %.1f/s  Power: %d dBm\n",
                  (unsigned long)dwellMs, effectiveHopsPerSec, pwr);
    Serial.println("  NOTE: parameters estimated [VERIFY against mLRS source]");
}

void mlrsStop() {
    if (!_radio) return;
    _radio->standby();
    _running = false;
    Serial.printf("[mLRS] TX OFF: %lu packets, %lu hops\n",
                  (unsigned long)_packetCount, (unsigned long)_hopCount);
}

void mlrsUpdate() {
    if (!_running || !_radio) return;

    const MlrsMode& mode = MLRS_MODES[_modeIdx];
    const MlrsModeParams& params = MLRS_MODE_PARAMS[_modeIdx];

    // Frame interval = 1/rate (each frame is either TX or RX)
    uint32_t frameUs = 1000000UL / mode.rateHz;
    unsigned long nowUs = micros();
    if ((nowUs - _lastFrameUs) < frameUs) return;
    _lastFrameUs += frameUs;

    if (_isTxPhase) {
        // TX phase: transmit on current channel
        int16_t txRc;
        if (mode.isLoRa) {
            txRc = _radio->startTransmit(MLRS_LORA_PAYLOAD, params.payloadLen);
        } else {
            txRc = _radio->transmit(_mlrsFskPayload, params.payloadLen);
        }
        if (txRc == RADIOLIB_ERR_NONE) _packetCount++;
        _isTxPhase = false;  // next frame is RX (silence)
    } else {
        // RX phase: silence (simulate receiver listening), then hop
        _isTxPhase = true;
        _hopIdx = (_hopIdx + 1) % MLRS_NUM_CHANNELS;
        _hopCount++;

        float nextFreq = _mlrsChannels[_hopSeq[_hopIdx]];
        _currentMHz = nextFreq;
        _radio->standby();
        _radio->setFrequency(nextFreq);
    }
}

MlrsParams mlrsGetParams() {
    const MlrsMode& mode = MLRS_MODES[_modeIdx];
    return MlrsParams{
        _currentMHz,
        _hopSeq[_hopIdx],
        _packetCount,
        _hopCount,
        mode.rateHz,
        mode.isLoRa,
        rfGetPower(),
        _running,
    };
}
