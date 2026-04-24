#include "xr1_radio.h"

#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>

#include "xr1_pins.h"
#include "xr1_config.h"

// ----- RadioLib objects -----------------------------------------------------
// ESP32C3 exposes exactly one general-purpose SPI peripheral. The `SPI` macro
// resolves to FSPI on the C3; aliasing it explicitly keeps intent obvious.
static SPIClass s_spi(FSPI);
static SPISettings s_spiSettings(XR1_SPI_HZ_BRINGUP, MSBFIRST, SPI_MODE0);

static Module s_module(LR1121_NSS, LR1121_DIO9, LR1121_RST, LR1121_BUSY,
                       s_spi, s_spiSettings);
static LR1121 s_radio(&s_module);

static Xr1RadioStatus s_status = {};

// LED renderer state
static Xr1LedMode s_ledMode = XR1_LED_AUTO;
static uint32_t s_lastTxSubMs = 0;   // millis() of last sub-GHz TX
static uint32_t s_lastTxHfMs  = 0;   // millis() of last 2.4 GHz TX

// Arduino-ESP32's neopixelWrite(pin, r, g, b) takes RGB parameters but
// already outputs GRB bytes on the wire internally — standard WS2812 protocol.
// The hardware.json led_rgb_isgrb=true flag confirms the XR1's LED is
// standard-wired, so we pass RGB straight through. No swap here: an earlier
// swap was present and was painting dim red where dim green was intended,
// causing the idle pulse to look like a red error blink. See the LED_RGB_IS_GRB
// comment in xr1_config.h for the ELRS-convention context.
static inline void writeLed(uint8_t r, uint8_t g, uint8_t b) {
    neopixelWrite(LED_RGB, r, g, b);
}

// ----- helpers --------------------------------------------------------------
static void waitBusyLow(uint32_t timeoutMs) {
    // LR1121 holds BUSY high during internal reset processing. RadioLib will
    // also poll this, but blocking here after reset gives a cleaner first
    // SPI transaction and easier failure attribution.
    const uint32_t start = millis();
    while (digitalRead(LR1121_BUSY) == HIGH) {
        if (millis() - start > timeoutMs) {
            return;
        }
        delay(1);
    }
}

static void pulseReset() {
    pinMode(LR1121_RST, OUTPUT);
    digitalWrite(LR1121_RST, LOW);
    delay(5);
    digitalWrite(LR1121_RST, HIGH);
    delay(10);
}

// Re-apply the band-appropriate PA config. LR1120::setOutputPower looks at
// this->highFreq (set by the last setFrequency call) to pick paSel.
static int16_t applyPower(int8_t dbm) {
    int16_t rc = s_radio.setOutputPower(dbm);
    if (rc == RADIOLIB_ERR_NONE) {
        s_status.powerDbm = dbm;
    }
    return rc;
}

static int16_t loraBegin(float freqMhz, float bwKhz, uint8_t sf, uint8_t cr,
                         int8_t powerDbm) {
    const int16_t rc = s_radio.begin(
        /* freq   */ freqMhz,
        /* bw     */ bwKhz,
        /* sf     */ sf,
        /* cr     */ cr,
        /* sync   */ RADIOLIB_LR11X0_LORA_SYNC_WORD_PRIVATE,
        /* power  */ powerDbm,
        /* preamb */ 8,
        /* tcxoV  */ LR1121_TCXO_VOLTAGE
    );
    if (rc != RADIOLIB_ERR_NONE) {
        return rc;
    }
    s_radio.setRegulatorDCDC();
    // Per RadioLib issue #1295, setRfSwitchTable must run after begin().
    s_radio.setRfSwitchTable(XR1_RFSW_DIO_PINS, XR1_RFSW_TABLE);
    return RADIOLIB_ERR_NONE;
}

// ----- public API -----------------------------------------------------------
bool xr1RadioBegin() {
    pinMode(LR1121_BUSY, INPUT);
    pinMode(LED_RGB, OUTPUT);
    writeLed(0, 0, 0);

    s_spi.begin(LR1121_SCK, LR1121_MISO, LR1121_MOSI, LR1121_NSS);

    pulseReset();
    waitBusyLow(200);

    const int16_t rc = loraBegin(915.0f, 125.0f, 7, 5, XR1_POWER_SUBGHZ_DBM[0]);
    s_status.lastError = rc;
    if (rc != RADIOLIB_ERR_NONE) {
        Serial.printf("[XR1] LR1121 begin() failed: %d\n", rc);
        Serial.println("[XR1] Hints:");
        Serial.println("      - check pin mapping against XR1 hardware.json");
        Serial.println("      - BUSY high forever => wrong RST pin or dead chip");
        Serial.println("      - version mismatch  => ELRS custom LR1121 firmware");
        Serial.println("                             still present; re-flash the");
        Serial.println("                             stock Semtech transceiver");
        Serial.println("                             image via ELRS web UI first.");
        return false;
    }

#if defined(RADIOLIB_LR11X0_GET_VERSION_INFO_DECLARED)
    uint8_t hw, type, fwMaj, fwMin;
    s_radio.getVersionInfo(&hw, &type, &fwMaj, &fwMin);
    s_status.hwVersion = (uint32_t)hw << 24 | (uint32_t)type << 16 |
                         (uint32_t)fwMaj << 8 | fwMin;
#endif

    s_status.initialized = true;
    s_status.freqMhz     = 915.0f;
    s_status.mod         = XR1_MOD_LORA;
    s_status.powerDbm    = XR1_POWER_SUBGHZ_DBM[0];
    s_status.sf          = 7;
    s_status.bwKhz       = 125.0f;
    s_status.cr          = 5;
    s_status.brKbps      = 0.0f;
    s_status.devKhz      = 0.0f;
    return true;
}

static bool txTestOnBand(float freqMhz, float bwKhz, const char *label) {
    int16_t rc = s_radio.setFrequency(freqMhz);
    if (rc != RADIOLIB_ERR_NONE) {
        Serial.printf("[XR1] %s setFrequency(%.1f) => %d\n", label, freqMhz, rc);
        s_status.lastError = rc;
        return false;
    }
    const int8_t powerDbm = (freqMhz > 1000.0f)
        ? XR1_POWER_2G4_DBM[0]
        : XR1_POWER_SUBGHZ_DBM[0];
    rc = s_radio.setOutputPower(powerDbm);
    if (rc != RADIOLIB_ERR_NONE) {
        Serial.printf("[XR1] %s setOutputPower(%d) => %d\n", label, powerDbm, rc);
        s_status.lastError = rc;
        return false;
    }
    rc = s_radio.setBandwidth(bwKhz, freqMhz > 1000.0f);
    if (rc != RADIOLIB_ERR_NONE) {
        Serial.printf("[XR1] %s setBandwidth(%.1f) => %d\n", label, bwKhz, rc);
        s_status.lastError = rc;
        return false;
    }
    const uint8_t payload[] = { 'J', 'J', 'X', 'R', '1', 0x00 };
    rc = s_radio.transmit(payload, sizeof(payload));
    if (rc != RADIOLIB_ERR_NONE) {
        Serial.printf("[XR1] %s transmit() => %d\n", label, rc);
        s_status.lastError = rc;
        return false;
    }
    Serial.printf("[XR1] %s TX OK @ %.1f MHz\n", label, freqMhz);
    return true;
}

bool xr1RadioHelloSelfTest() {
    if (!s_status.initialized) {
        return false;
    }
    s_status.subGhzOk = txTestOnBand(915.0f, 125.0f, "SUB-GHZ");
    s_status.twoGhzOk = txTestOnBand(2440.0f, 812.5f, "2.4GHZ ");
    // Leave radio in a clean post-test state: back to 915 / 125 / SF7 / CR5.
    // Callers use xr1RadioSet* afterwards; a missing return-to-base would hand
    // the 2.4 GHz settings to whoever calls PING→FREQ next and confuse them.
    s_radio.setFrequency(915.0f);
    s_radio.setBandwidth(125.0f, false);
    s_radio.setOutputPower(XR1_POWER_SUBGHZ_DBM[0]);
    s_status.freqMhz  = 915.0f;
    s_status.bwKhz    = 125.0f;
    s_status.powerDbm = XR1_POWER_SUBGHZ_DBM[0];
    return s_status.subGhzOk && s_status.twoGhzOk;
}

int16_t xr1RadioSetFrequency(float mhz) {
    int16_t rc = s_radio.setFrequency(mhz);
    if (rc != RADIOLIB_ERR_NONE) {
        s_status.lastError = rc;
        return rc;
    }
    s_status.freqMhz = mhz;

    // Adjust LoRa BW if the HF/sub-GHz set changed — a band hop can orphan the
    // current bandwidth value against the modem's accepted set.
    if (s_status.mod == XR1_MOD_LORA) {
        const bool high = (mhz > 1000.0f);
        const float bw  = high ? 812.5f : 125.0f;
        rc = s_radio.setBandwidth(bw, high);
        if (rc == RADIOLIB_ERR_NONE) s_status.bwKhz = bw;
    }

    // Re-apply power so the band's PA (LP vs. HF) is selected.
    applyPower(s_status.powerDbm);
    return rc;
}

int16_t xr1RadioSetLoRa(uint8_t sf, float bwKhz, uint8_t cr) {
    if (s_status.mod != XR1_MOD_LORA) {
        // Switch packet type by re-running LoRa begin() at current freq+power.
        int16_t rc = loraBegin(s_status.freqMhz, bwKhz, sf, cr, s_status.powerDbm);
        if (rc != RADIOLIB_ERR_NONE) { s_status.lastError = rc; return rc; }
        s_status.mod = XR1_MOD_LORA;
        s_status.sf = sf; s_status.bwKhz = bwKhz; s_status.cr = cr;
        return RADIOLIB_ERR_NONE;
    }
    int16_t rc = s_radio.setSpreadingFactor(sf);
    if (rc == RADIOLIB_ERR_NONE) rc = s_radio.setBandwidth(bwKhz, s_status.freqMhz > 1000.0f);
    if (rc == RADIOLIB_ERR_NONE) rc = s_radio.setCodingRate(cr);
    if (rc == RADIOLIB_ERR_NONE) {
        s_status.sf = sf; s_status.bwKhz = bwKhz; s_status.cr = cr;
    } else {
        s_status.lastError = rc;
    }
    return rc;
}

int16_t xr1RadioSetFSK(float brKbps, float devKhz) {
    // beginGFSK resets packet type to GFSK and applies LDO; re-pin freq and
    // power after it returns, and reinstall the RF switch table.
    int16_t rc = s_radio.beginGFSK(brKbps, devKhz, /*rxBw*/ 156.2f,
                                   /*preamble*/ 16, LR1121_TCXO_VOLTAGE);
    if (rc != RADIOLIB_ERR_NONE) { s_status.lastError = rc; return rc; }
    s_radio.setRegulatorDCDC();
    s_radio.setRfSwitchTable(XR1_RFSW_DIO_PINS, XR1_RFSW_TABLE);
    s_radio.setFrequency(s_status.freqMhz);
    applyPower(s_status.powerDbm);
    s_status.mod = XR1_MOD_GFSK;
    s_status.brKbps = brKbps;
    s_status.devKhz = devKhz;
    return RADIOLIB_ERR_NONE;
}

int16_t xr1RadioSetPower(int8_t dbm) {
    return applyPower(dbm);
}

int16_t xr1RadioTransmit(const uint8_t *data, size_t len) {
    int16_t rc = s_radio.transmit(const_cast<uint8_t *>(data), len);
    if (rc == RADIOLIB_ERR_NONE) {
        if (s_status.freqMhz > 1000.0f) s_lastTxHfMs  = millis();
        else                             s_lastTxSubMs = millis();
    } else {
        s_status.lastError = rc;
    }
    return rc;
}

int16_t xr1RadioSetPreamble(uint16_t symbols) {
    int16_t rc = s_radio.setPreambleLength(symbols);
    if (rc != RADIOLIB_ERR_NONE) s_status.lastError = rc;
    return rc;
}

int16_t xr1RadioSetImplicitHeader(uint8_t payloadLen) {
    int16_t rc = s_radio.implicitHeader(payloadLen);
    if (rc != RADIOLIB_ERR_NONE) s_status.lastError = rc;
    return rc;
}

int16_t xr1RadioSetExplicitHeader() {
    int16_t rc = s_radio.explicitHeader();
    if (rc != RADIOLIB_ERR_NONE) s_status.lastError = rc;
    return rc;
}

int16_t xr1RadioReset() {
    pulseReset();
    waitBusyLow(200);
    const int16_t rc = loraBegin(915.0f, 125.0f, 7, 5, XR1_POWER_SUBGHZ_DBM[0]);
    if (rc == RADIOLIB_ERR_NONE) {
        s_status.freqMhz = 915.0f;
        s_status.mod     = XR1_MOD_LORA;
        s_status.powerDbm = XR1_POWER_SUBGHZ_DBM[0];
        s_status.sf = 7; s_status.bwKhz = 125.0f; s_status.cr = 5;
        s_ledMode = XR1_LED_AUTO;
    }
    return rc;
}

const Xr1RadioStatus &xr1RadioGetStatus() {
    return s_status;
}

// ----- LED ------------------------------------------------------------------
void xr1LedSetOverride(Xr1LedMode mode) {
    s_ledMode = mode;
}

// Active window (ms) after a TX during which the LED shows that band's colour.
static constexpr uint32_t TX_INDICATE_WINDOW_MS = 250;

// Render idle pulse: ~2 s period, brief dim every cycle so the operator can
// tell the board is alive without it being visually loud.
static void renderIdlePulse(uint32_t now) {
    const uint32_t phase = now % 2000;
    if (phase < 1700) {
        writeLed(0, 12, 0);       // steady dim green
    } else if (phase < 1850) {
        writeLed(0, 0, 0);        // 150 ms off
    } else {
        writeLed(0, 12, 0);
    }
}

// Render sub-GHz-only: ~1 Hz blue blink (200 ms on / 800 ms off).
static void renderSubBlink(uint32_t now) {
    writeLed(0, 0, (now % 1000) < 200 ? 64 : 0);
}

// Render 2.4 GHz-only: double-blink purple in a 1 s period (two 100 ms pulses
// 250 ms apart), then a quiet remainder.
static void renderHfDoubleBlink(uint32_t now) {
    const uint32_t phase = now % 1000;
    const bool on = (phase < 100) || (phase >= 250 && phase < 350);
    writeLed(on ? 48 : 0, 0, on ? 48 : 0);   // magenta/purple
}

// Render both bands: alternate blue/purple at 2 Hz (250 ms per colour).
static void renderBothAlternate(uint32_t now) {
    const bool blue = ((now / 250) & 1) == 0;
    if (blue) writeLed(0, 0, 64);
    else       writeLed(48, 0, 48);
}

void xr1LedUpdate() {
    // Manual override wins.
    switch (s_ledMode) {
        case XR1_LED_OFF:   writeLed(0, 0, 0);     return;
        case XR1_LED_RED:   writeLed(64, 0, 0);    return;
        case XR1_LED_GREEN: writeLed(0, 64, 0);    return;
        case XR1_LED_BLUE:  writeLed(0, 0, 64);    return;
        case XR1_LED_AUTO:  break;
    }

    // Error solid-red takes precedence over idle/TX rendering.
    if (!s_status.initialized) {
        writeLed(64, 0, 0);
        return;
    }

    const uint32_t now = millis();
    const bool subActive = (now - s_lastTxSubMs) < TX_INDICATE_WINDOW_MS;
    const bool hfActive  = (now - s_lastTxHfMs)  < TX_INDICATE_WINDOW_MS;

    if (subActive && hfActive)  { renderBothAlternate(now); return; }
    if (subActive)              { renderSubBlink(now);      return; }
    if (hfActive)               { renderHfDoubleBlink(now); return; }
    renderIdlePulse(now);
}
