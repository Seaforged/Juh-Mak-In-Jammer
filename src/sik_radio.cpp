#include <Arduino.h>
#include "sik_radio.h"
#include "rf_modes.h"       // for rfGetPower()
#include "protocol_params.h"

// ============================================================
// SiK Radio GFSK FHSS+TDM Simulation — v2 ref §3.3
// ============================================================
// SiK radios are the standard MAVLink telemetry link for
// ArduPilot/PX4 drones. GFSK modulation means SENTRY-RF's CAD
// will NOT detect this — only RSSI sweep and FSK Phase 3 can.

static SX1262 *_radio = nullptr;
static const SikBand& _band = SIK_BANDS[SIK_BAND_US915];
static uint8_t _speedIdx = 1;  // default: 64 kbps (index 1)

// Channel table — built at start using SiK formula
static const uint8_t SIK_MAX_CHANNELS = 50;
static float _sikChannels[SIK_MAX_CHANNELS];
static uint8_t _numChannels = SIK_MAX_CHANNELS;

// FHSS hop sequence
static uint8_t _hopSeq[SIK_MAX_CHANNELS];
static uint8_t _hopIdx = 0;

// State
static bool     _running      = false;
static uint32_t _packetCount  = 0;
static uint32_t _hopCount     = 0;
static float    _currentMHz   = 0;
static unsigned long _lastHopUs = 0;

// TDM timing: ~20 ms TX window per channel = ~50 hops/sec — v2 §3.3.2
static const uint32_t SIK_HOP_INTERVAL_US = 20000;

// TX payload — random bytes, no valid MAVLink framing needed
// SiK TX window at 64 kbps × 20 ms = ~160 bytes max
static uint8_t _sikPayload[32];

// Build channel frequency table using SiK formula — v2 §3.3.2
// channel[n] = MIN + guard_delta + (n * channel_width) + NETID_skew
static void sikBuildChannelTable() {
    _numChannels = _band.defaultChannels;
    for (uint8_t i = 0; i < _numChannels; i++) {
        _sikChannels[i] = sikChanFreq(_band, _numChannels, i, SIK_DEFAULT_NETID);
    }
}

// Simple pseudo-random hop sequence
static void sikBuildHopSequence(uint32_t seed) {
    for (uint8_t i = 0; i < _numChannels; i++) _hopSeq[i] = i;
    uint32_t rng = seed;
    for (uint8_t i = _numChannels - 1; i > 0; i--) {
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

void sikInit(SX1262 *radio) {
    _radio = radio;
    _running = false;
}

void sikSetSpeed(uint8_t speedIndex) {
    if (speedIndex < SIK_AIR_SPEED_COUNT) {
        _speedIdx = speedIndex;
    }
}

void sikStart() {
    if (!_radio) return;

    float airSpeed = SIK_AIR_SPEEDS_KBPS[_speedIdx];

    // Build channel table and hop sequence
    sikBuildChannelTable();
    sikBuildHopSequence(0xA1B2C3D4);
    _hopIdx = 0;
    _packetCount = 0;
    _hopCount = 0;

    // Generate random payload
    for (uint8_t i = 0; i < sizeof(_sikPayload); i++) {
        _sikPayload[i] = (uint8_t)(esp_random() & 0xFF);
    }

    int8_t pwr = rfGetPower();

    // Full reset for clean state
    _radio->reset();
    delay(100);

    // Configure SX1262 in FSK mode — v2 §3.3.1
    // Deviation ~25 kHz is standard for GFSK at 64 kbps (BT=0.5 → dev = bitrate/4)
    float deviation = airSpeed / 4.0f;  // Approximate GFSK deviation
    if (deviation < 5.0f) deviation = 5.0f;

    int state = _radio->beginFSK(
        _sikChannels[_hopSeq[0]],
        airSpeed,           // bitrate kbps — v2 §3.3.1
        deviation,          // freq deviation kHz
        156.2,              // RX bandwidth kHz
        pwr,
        32,                 // preamble length (bits) — v2 §3.3
        1.8,                // TCXO voltage
        false               // LDO
    );

    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("[SiK] FSK config FAILED (error %d)\n", state);
        _running = false;
        return;
    }

    _currentMHz = _sikChannels[_hopSeq[0]];
    _running = true;
    _lastHopUs = micros();

    // Transmit first packet
    _radio->transmit(_sikPayload, sizeof(_sikPayload));
    _packetCount++;

    // Protocol info output — v2 §7.2
    float chanWidthKHz = (_band.maxFreqMHz - _band.minFreqMHz) * 1000.0f / (_numChannels + 2);
    Serial.printf("[SiK-%s] %uch %.1f-%.1fMHz GFSK@%.0fkbps TDM\n",
                  _band.name, _numChannels,
                  _band.minFreqMHz, _band.maxFreqMHz, airSpeed);
    Serial.printf("  Channel width: %.0fkHz  Dwell: ~20ms/freq  Hops: 50/s  Power: %d dBm\n",
                  chanWidthKHz, pwr);
}

void sikStop() {
    if (!_radio) return;
    _radio->standby();
    _running = false;
    Serial.printf("[SiK] TX OFF: %lu packets, %lu hops\n",
                  (unsigned long)_packetCount, (unsigned long)_hopCount);
}

void sikUpdate() {
    if (!_running || !_radio) return;

    unsigned long nowUs = micros();
    if ((nowUs - _lastHopUs) < SIK_HOP_INTERVAL_US) return;
    _lastHopUs = nowUs;

    // Hop to next channel
    _hopIdx = (_hopIdx + 1) % _numChannels;
    _hopCount++;

    float nextFreq = _sikChannels[_hopSeq[_hopIdx]];
    _currentMHz = nextFreq;

    _radio->setFrequency(nextFreq);
    _radio->transmit(_sikPayload, sizeof(_sikPayload));
    _packetCount++;
}

SikParams sikGetParams() {
    return SikParams{
        _currentMHz,
        _hopSeq[_hopIdx],
        _packetCount,
        _hopCount,
        SIK_AIR_SPEEDS_KBPS[_speedIdx],
        rfGetPower(),
        _running,
    };
}
