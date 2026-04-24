#include <Arduino.h>
#include "mlrs_sim.h"
#include "rf_modes.h"       // for rfGetPower()
#include "protocol_params.h"
#include "system_health.h"

// ============================================================
// mLRS Simulation — v2 ref §3.4 [Ref P12, P14]
// ============================================================
// mLRS uses symmetric TX/RX alternation: each frame slot contains
// either a TX or RX burst, and both radios hop in lockstep.
// Effective hop rate = packet_rate / 2 (since TX and RX alternate).
//
// Ground-truth parameters from olliw42/mLRS (Common/fhss.h and
// Common/common_conf.h, per the JJ fix3 research pass):
//   - 915 MHz FCC:    43 channels, 902.0 - 928.0 MHz
//   - 868 MHz EU:     10 channels, 863.275 - 869.575 MHz (not emulated here)
//   - 433 MHz:         3 channels (not emulated here)
// Air-rate sensitivity figures in the mLRS docs point to SF7/BW500 for the
// 19 Hz mode (-112 dBm) and SF6/BW500 for the 31 Hz mode (-108 dBm). The
// 50 Hz FSK mode's bitrate / deviation are not documented publicly —
// approximated against SiK-style 64 kbps / 25 kHz as the best defensible
// placeholder. Frame length is FRAME_TX_RX_LEN = 91 in mLRS sources.
//
// Hop sequence in real mLRS is LFSR-based with a bind-phrase-derived
// sync word. We keep the simpler LCG shuffle here — the spectral
// footprint (channel occupancy) is identical; only the hop order differs.

static SX1262 *_radio = nullptr;
static uint8_t _modeIdx = 0;  // default: 19 Hz LoRa

static const uint8_t  MLRS_NUM_CHANNELS  = 43;        // mLRS 915 FCC (fhss.h)
static const float    MLRS_BAND_START    = 902.0f;    // FCC ISM start
static const float    MLRS_BAND_END      = 928.0f;    // FCC ISM end
static const uint8_t  MLRS_FRAME_LEN     = 91;        // FRAME_TX_RX_LEN

struct MlrsModeParams {
    uint8_t  sf;           // per mode (from sensitivity tables)
    uint32_t bwHz;         // per mode
    float    fskBitrate;   // 0 = LoRa mode, >0 = FSK mode (kbps)
    uint8_t  payloadLen;   // mLRS uses fixed 91-byte frames
};

// Grounded values:
//   19 Hz: SF7/BW500 (sensitivity -112 dBm suggests SF7)
//   31 Hz: SF6/BW500 (sensitivity -108 dBm suggests SF6)
//   50 Hz FSK: bitrate / deviation undocumented publicly; SiK-like
//              64 kbps / 16 kHz is the closest defensible approximation.
static const MlrsModeParams MLRS_MODE_PARAMS[] = {
    { 7, 500000,  0.0f, MLRS_FRAME_LEN },  // 19 Hz LoRa SF7/BW500
    { 6, 500000,  0.0f, MLRS_FRAME_LEN },  // 31 Hz LoRa SF6/BW500
    { 0, 0,      64.0f, MLRS_FRAME_LEN },  // 50 Hz GFSK ~64 kbps (approximation)
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
// mLRS transmits a fixed 91-byte frame (FRAME_TX_RX_LEN). We fill it with
// a recognisable "MLRS" prefix then zero padding — plausible at the byte
// level for a receiver doing length-based filtering.
static uint8_t _mlrsLoraPayload[MLRS_FRAME_LEN];
static uint8_t _mlrsFskPayload[MLRS_FRAME_LEN];
static bool    _mlrsPayloadsInit = false;

static void mlrsEnsurePayloads() {
    if (_mlrsPayloadsInit) return;
    memset(_mlrsLoraPayload, 0, sizeof(_mlrsLoraPayload));
    _mlrsLoraPayload[0] = 'M'; _mlrsLoraPayload[1] = 'L';
    _mlrsLoraPayload[2] = 'R'; _mlrsLoraPayload[3] = 'S';
    _mlrsPayloadsInit = true;
}

static void mlrsBuildChannelTable() {
    // Linear spacing across the FCC band. Real mLRS channel centres come
    // from fhss.h's per-band tables; the spectral envelope is the same
    // whether the centres line up on a uniform grid or the mLRS grid
    // (both span 902-928 MHz with ~43 channels).
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
    if (!sx1262ModeAvailable()) return;

    const MlrsMode& mode = MLRS_MODES[_modeIdx];
    const MlrsModeParams& params = MLRS_MODE_PARAMS[_modeIdx];

    mlrsBuildChannelTable();
    mlrsBuildHopSequence(0x6D4C5253);  // "mLRS" as seed
    _hopIdx = 0;
    _packetCount = 0;
    _hopCount = 0;
    _isTxPhase = true;
    mlrsEnsurePayloads();

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
            7,                      // CR 4/7 (mLRS default, not bind-phrase-derived)
            SYNC_WORD_ELRS,         // 0x12 private LoRa — real mLRS derives
                                    // from bind phrase; the public sync byte
                                    // at least flags it as a private network.
            pwr,
            8,                      // preamble (LoRa default; mLRS uses 8 per
                                    // the reference implementation)
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
        txRc = _radio->startTransmit(_mlrsLoraPayload, params.payloadLen);
    } else {
        txRc = _radio->transmit(_mlrsFskPayload, params.payloadLen);
    }
    if (txRc == RADIOLIB_ERR_NONE) _packetCount++;

    // Protocol info output — v2 §7.2
    // Symmetric hopping: effective hop rate = packet_rate / 2
    uint32_t frameUs = 1000000UL / mode.rateHz;
    float effectiveHopsPerSec = mode.rateHz / 2.0f;
    uint32_t dwellMs = (2 * frameUs) / 1000;  // TX + RX frame before hop
    Serial.printf("[mLRS-915] %uch %.0f-%.0fMHz %s %uHz symmetric [FOOTPRINT]\n",
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
            txRc = _radio->startTransmit(_mlrsLoraPayload, params.payloadLen);
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
