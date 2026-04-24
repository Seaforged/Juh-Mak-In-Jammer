#include <Arduino.h>
#include "crossfire.h"
#include "rf_modes.h"  // for rfGetPower()
#include "protocol_params.h"

// ============================================================
// TBS Crossfire — dual-modulation (FSK 150 Hz + LoRa 50 Hz),
// dual-band (915 US / 868 EU)
// ============================================================

static SX1262 *_radio = nullptr;

// Runtime-selectable band (default: 915 US)
static uint8_t _bandIdx = CRSF_BAND_915;

// Runtime-selectable modulation (default: FSK 150 Hz)
static bool _loRaMode = false;

// FHSS hop sequence — sized for 915 band (100 channels, the larger)
static uint8_t _crsfHopSeq[100];
static uint8_t _crsfHopIdx = 0;

static bool     _crsfRunning     = false;
static uint32_t _crsfPacketCount = 0;
static uint32_t _crsfHopCount    = 0;
static float    _crsfCurrentMHz  = 0;
static unsigned long _crsfLastHopUs = 0;

// 16-byte dummy CRSF-like payload
static const uint8_t CRSF_PAYLOAD[] = {
    0xC8, 0x18, 0x16, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xAA
};

static void crsfBuildHopSequence(uint32_t seed) {
    uint8_t numCh = CRSF_BANDS[_bandIdx].channels;
    for (uint8_t i = 0; i < numCh; i++) {
        _crsfHopSeq[i] = i;
    }
    uint32_t rng = seed;
    for (uint8_t i = numCh - 1; i > 0; i--) {
        rng = rng * 1664525UL + 1013904223UL;
        uint8_t j = rng % (i + 1);
        uint8_t tmp = _crsfHopSeq[i];
        _crsfHopSeq[i] = _crsfHopSeq[j];
        _crsfHopSeq[j] = tmp;
    }
}

static float crsfChanToFreq(uint8_t chan) {
    return CRSF_BANDS[_bandIdx].freqStartMHz
         + (chan * CRSF_BANDS[_bandIdx].chanSpacingMHz);
}

void crossfireInit(SX1262 *radio) {
    _radio = radio;
    _crsfRunning = false;
}

void crossfireSetBand(uint8_t bandIdx) {
    if (bandIdx <= CRSF_BAND_868) {
        _bandIdx = bandIdx;
    }
}

// --- FSK 150 Hz mode (existing behavior) ---
void crossfireStart() {
    if (!_radio) return;

    _loRaMode = false;
    const CrossfireBand& band = CRSF_BANDS[_bandIdx];

    crsfBuildHopSequence(0xBAADF00D);
    _crsfHopIdx = 0;
    _crsfPacketCount = 0;
    _crsfHopCount = 0;

    int8_t pwr = rfGetPower();
    _radio->reset();
    delay(100);

    int state = _radio->beginFSK(
        crsfChanToFreq(_crsfHopSeq[0]),
        CRSF_FSK_BITRATE_KBPS,     // 85.1 kbps — v2 §3.2.3
        CRSF_FSK_DEVIATION_KHZ,    // 50 kHz — v2 §3.2.3 [VERIFY]
        156.2,                      // RX bandwidth kHz
        pwr,
        16,                         // preamble length
        1.8,                        // TCXO voltage
        false                       // use LDO
    );

    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("[CRSF] FSK config FAILED (error %d)\n", state);
        _crsfRunning = false;
        return;
    }

    _crsfCurrentMHz = crsfChanToFreq(_crsfHopSeq[0]);
    _crsfRunning = true;
    _crsfLastHopUs = micros();

    _radio->transmit(CRSF_PAYLOAD, sizeof(CRSF_PAYLOAD));
    _crsfPacketCount++;

    Serial.printf("[CRSF-%s] %uch %.0f-%.0fMHz FSK %.1fkbps %uHz %d dBm [FOOTPRINT]\n",
                  band.name, band.channels,
                  band.freqStartMHz, band.freqStopMHz,
                  CRSF_FSK_BITRATE_KBPS, CRSF_FSK_RATE_HZ, pwr);
}

// --- LoRa 50 Hz mode (new) ---
void crossfireStartLoRa() {
    if (!_radio) return;

    _loRaMode = true;
    const CrossfireBand& band = CRSF_BANDS[_bandIdx];

    crsfBuildHopSequence(0xBAADF00D);
    _crsfHopIdx = 0;
    _crsfPacketCount = 0;
    _crsfHopCount = 0;

    int8_t pwr = rfGetPower();
    _radio->reset();
    delay(100);

    // Crossfire LoRa 50 Hz — v2 §3.2.2. SF/BW/CR are proprietary; these are
    // reasonable approximations. Crossfire is NOT ELRS — uses explicit header.
    // [VERIFY] exact SF/BW/CR against an SDR capture of a real Crossfire TX.
    int state = _radio->begin(
        crsfChanToFreq(_crsfHopSeq[0]),
        500.0f,                   // BW 500 kHz [VERIFY]
        7,                        // SF7 [VERIFY]
        7,                        // CR 4/7 [VERIFY]
        RADIOLIB_SX126X_SYNC_WORD_PRIVATE,  // 0x12 placeholder [VERIFY]
        pwr,
        8,                        // preamble [VERIFY]
        1.8, false
    );

    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("[CRSF] LoRa config FAILED (error %d)\n", state);
        _crsfRunning = false;
        return;
    }

    // Crossfire uses explicit header — this is NOT ELRS.
    _radio->explicitHeader();

    _crsfCurrentMHz = crsfChanToFreq(_crsfHopSeq[0]);
    _crsfRunning = true;
    _crsfLastHopUs = micros();

    _radio->transmit(CRSF_PAYLOAD, sizeof(CRSF_PAYLOAD));
    _crsfPacketCount++;

    Serial.printf("[CRSF-%s] %uch %.0f-%.0fMHz LoRa SF7/BW500 %uHz %d dBm [FOOTPRINT, VERIFY SF/BW/CR]\n",
                  band.name, band.channels,
                  band.freqStartMHz, band.freqStopMHz,
                  CRSF_LORA_RATE_HZ, pwr);
}

void crossfireStop() {
    if (!_radio) return;

    _radio->standby();
    _crsfRunning = false;
    Serial.printf("[CRSF] TX OFF: %lu packets, %lu hops (%s)\n",
                  (unsigned long)_crsfPacketCount,
                  (unsigned long)_crsfHopCount,
                  _loRaMode ? "LoRa 50Hz" : "FSK 150Hz");
}

void crossfireUpdate() {
    if (!_crsfRunning || !_radio) return;

    uint32_t intervalUs = _loRaMode ? CRSF_LORA_PACKET_INTERVAL_US
                                    : CRSF_FSK_PACKET_INTERVAL_US;
    unsigned long nowUs = micros();
    if ((nowUs - _crsfLastHopUs) < intervalUs) return;

    _crsfLastHopUs = nowUs;

    uint8_t numCh = CRSF_BANDS[_bandIdx].channels;
    _crsfHopIdx = (_crsfHopIdx + 1) % numCh;
    _crsfHopCount++;

    float nextFreq = crsfChanToFreq(_crsfHopSeq[_crsfHopIdx]);
    _crsfCurrentMHz = nextFreq;

    _radio->setFrequency(nextFreq);
    _radio->transmit(CRSF_PAYLOAD, sizeof(CRSF_PAYLOAD));
    _crsfPacketCount++;
}

CrossfireParams crossfireGetParams() {
    return CrossfireParams{
        _crsfCurrentMHz,
        _crsfHopSeq[_crsfHopIdx],
        _crsfPacketCount,
        _crsfHopCount,
        rfGetPower(),
        _crsfRunning,
        _loRaMode,
        CRSF_BANDS[_bandIdx].name,
    };
}
