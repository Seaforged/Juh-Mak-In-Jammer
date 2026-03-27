#include <Arduino.h>
#include "crossfire.h"
#include "rf_modes.h"  // for rfGetPower()

// ============================================================
// TBS Crossfire 915 MHz FSK Simulation
// ============================================================

static SX1262 *_radio = nullptr;

static constexpr uint8_t  CRSF_NUM_CHANNELS  = 100;
static constexpr float    CRSF_BAND_START    = 902.165f;
static constexpr float    CRSF_CHAN_SPACING   = 0.260f;   // MHz
static constexpr uint32_t CRSF_HOP_INTERVAL_US = 20000;   // 20ms = 50 Hz hop rate

// FHSS hop sequence
static uint8_t _crsfHopSeq[CRSF_NUM_CHANNELS];
static uint8_t _crsfHopIdx = 0;

static bool     _crsfRunning     = false;
static uint32_t _crsfPacketCount = 0;
static uint32_t _crsfHopCount    = 0;
static float    _crsfCurrentMHz  = CRSF_BAND_START;
static unsigned long _crsfLastHopUs = 0;

// 16-byte dummy CRSF-like payload
static const uint8_t CRSF_PAYLOAD[] = {
    0xC8, 0x18, 0x16, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xAA
};

static void crsfBuildHopSequence(uint32_t seed) {
    for (uint8_t i = 0; i < CRSF_NUM_CHANNELS; i++) {
        _crsfHopSeq[i] = i;
    }
    uint32_t rng = seed;
    for (uint8_t i = CRSF_NUM_CHANNELS - 1; i > 0; i--) {
        rng = rng * 1664525UL + 1013904223UL;
        uint8_t j = rng % (i + 1);
        uint8_t tmp = _crsfHopSeq[i];
        _crsfHopSeq[i] = _crsfHopSeq[j];
        _crsfHopSeq[j] = tmp;
    }
}

static float crsfChanToFreq(uint8_t chan) {
    return CRSF_BAND_START + (chan * CRSF_CHAN_SPACING);
}

void crossfireInit(SX1262 *radio) {
    _radio = radio;
    _crsfRunning = false;
}

void crossfireStart() {
    if (!_radio) return;

    crsfBuildHopSequence(0xBAADF00D);
    _crsfHopIdx = 0;
    _crsfPacketCount = 0;
    _crsfHopCount = 0;

    int8_t pwr = rfGetPower();

    // Full reset for clean state
    _radio->reset();
    delay(100);

    // FSK mode: 85.1 kbps, 50 kHz deviation, 156.2 kHz RX BW
    int state = _radio->beginFSK(
        crsfChanToFreq(_crsfHopSeq[0]),
        85.1,     // bitrate kbps
        50.0,     // freq deviation kHz
        156.2,    // RX bandwidth kHz
        pwr,
        16,       // preamble length
        1.8,      // TCXO voltage
        false     // use LDO
    );

    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("CRSF FSK config FAILED (error %d)\n", state);
        _crsfRunning = false;
        return;
    }

    _crsfCurrentMHz = crsfChanToFreq(_crsfHopSeq[0]);
    _crsfRunning = true;
    _crsfLastHopUs = micros();

    // Transmit first packet
    _radio->transmit(CRSF_PAYLOAD, sizeof(CRSF_PAYLOAD));
    _crsfPacketCount++;

    Serial.printf("CRSF TX ON: FSK 85.1k, 100ch, 50Hz, %d dBm\n", pwr);
}

void crossfireStop() {
    if (!_radio) return;

    _radio->standby();
    _crsfRunning = false;
    Serial.printf("CRSF TX OFF: %lu packets, %lu hops\n",
                  (unsigned long)_crsfPacketCount,
                  (unsigned long)_crsfHopCount);
}

void crossfireUpdate() {
    if (!_crsfRunning || !_radio) return;

    unsigned long nowUs = micros();
    if ((nowUs - _crsfLastHopUs) < CRSF_HOP_INTERVAL_US) return;

    _crsfLastHopUs = nowUs;

    _crsfHopIdx = (_crsfHopIdx + 1) % CRSF_NUM_CHANNELS;
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
    };
}
