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

// ----- public API -----------------------------------------------------------
bool xr1RadioBegin() {
    pinMode(LR1121_BUSY, INPUT);
    pinMode(LED_RGB, OUTPUT);
    digitalWrite(LED_RGB, LOW);

    s_spi.begin(LR1121_SCK, LR1121_MISO, LR1121_MOSI, LR1121_NSS);

    pulseReset();
    waitBusyLow(200);

    // Install the RF switch table BEFORE begin() so the first TX/RX the chip
    // attempts during initialization is routed to the correct antenna path.
    s_radio.setRfSwitchTable(XR1_RFSW_DIO_PINS, XR1_RFSW_TABLE);

    // begin() signature (RadioLib 7.x LR1121 via LR1120 base):
    //   freq, bw, sf, cr, syncWord, power, preambleLength, tcxoVoltage
    // TCXO is passed here on purpose — the default (1.6 V) is silently wrong
    // for the 3.0 V crystal used on the T3S3 LR1121 V1.3 and (assumed)
    // RadioMaster XR1. Mismatch does not throw an error; it just destroys RX
    // sensitivity and frequency accuracy.
    const int16_t rc = s_radio.begin(
        /* freq   */ 915.0f,
        /* bw     */ 125.0f,
        /* sf     */ 7,
        /* cr     */ 5,
        /* sync   */ RADIOLIB_LR11X0_LORA_SYNC_WORD_PRIVATE,
        /* power  */ XR1_POWER_SUBGHZ_DBM[0],
        /* preamb */ 8,
        /* tcxoV  */ LR1121_TCXO_VOLTAGE
    );

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

    // Prefer the internal DC-DC regulator over the LDO — lower current draw
    // and matches the ELRS configuration that RadioMaster ships with. This
    // is the Phase-1 "useLDO=false" replacement after the RadioLib 7.x API
    // split regulator choice out of begin().
    s_radio.setRegulatorDCDC();

    // Read chip + firmware version for the status line. LR1121 returns four
    // bytes: [HW, TYPE, FW_MAJOR, FW_MINOR]. RadioLib exposes this through
    // getVersionInfo(); if that isn't available in the installed version we
    // fall back to zero and rely on the begin() success alone.
#if defined(RADIOLIB_LR11X0_GET_VERSION_INFO_DECLARED)
    uint8_t hw, type, fwMaj, fwMin;
    s_radio.getVersionInfo(&hw, &type, &fwMaj, &fwMin);
    s_status.hwVersion = (uint32_t)hw << 24 | (uint32_t)type << 16 |
                         (uint32_t)fwMaj << 8 | fwMin;
#else
    s_status.hwVersion = 0;
#endif

    s_status.initialized = true;
    return true;
}

static bool txTestOnBand(float freqMhz, float bwKhz, const char *label) {
    int16_t rc = s_radio.setFrequency(freqMhz);
    if (rc != RADIOLIB_ERR_NONE) {
        Serial.printf("[XR1] %s setFrequency(%.1f) => %d\n", label, freqMhz, rc);
        s_status.lastError = rc;
        return false;
    }
    rc = s_radio.setBandwidth(bwKhz);
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

    // Sub-GHz: 915 MHz, BW 125 kHz (default ELRS FCC915-ish)
    s_status.subGhzOk = txTestOnBand(915.0f, 125.0f, "SUB-GHZ");

    // 2.4 GHz: 2440 MHz with a wider bandwidth — LR1121 2.4 GHz LoRa uses
    // BW500/BW800 modes per Semtech's user manual; 812.5 kHz is the wide
    // ISM-band preset RadioLib exposes.
    s_status.twoGhzOk = txTestOnBand(2440.0f, 812.5f, "2.4GHZ ");

    return s_status.subGhzOk && s_status.twoGhzOk;
}

const Xr1RadioStatus &xr1RadioGetStatus() {
    return s_status;
}

void xr1RadioLedHeartbeat() {
    // Placeholder until the WS2812 driver lands — toggles GPIO8 at ~1 Hz.
    // On an actual WS2812 this produces no valid colour (the protocol needs
    // precise 800 kHz timing), but the pin activity is still visible on a
    // logic analyzer, which is enough to prove main loop liveness.
    static uint32_t lastToggle = 0;
    static bool level = false;
    const uint32_t now = millis();
    if (now - lastToggle >= 500) {
        lastToggle = now;
        level = !level;
        digitalWrite(LED_RGB, level ? HIGH : LOW);
    }
}
