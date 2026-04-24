// ============================================================================
// JJ XR1 Firmware
//
// Runs on the ESP32C3 inside a RadioMaster XR1. Brings up the LR1121 radio,
// runs a two-band self-test, then hands control to the UART command parser
// (xr1_uart.cpp) and the Remote ID transport stacks (remote_id_*.cpp).
//
// Part of the Juh-Mak-In Jammer v3.0 three-emitter architecture
// (T3S3 SX1262 + XR1 LR1121 + XR1 WiFi/BLE). See docs/JJ_v3_Consolidated_
// Roadmap.md for the full architecture.
// ============================================================================

#include <Arduino.h>

#include "version.h"
#include "xr1_pins.h"
#include "xr1_radio.h"
#include "xr1_uart.h"
#include "remote_id.h"

static void printBanner() {
    Serial.println();
    Serial.println("================================================");
    Serial.printf ("  %s\n", XR1_FW_VERSION);
    Serial.printf ("  Built: %s\n", XR1_FW_BUILD_DATE);
    Serial.println("  Target: RadioMaster XR1 (ESP32C3 + LR1121)");
    Serial.println("  JJ v3 XR1 (LR1121 + WiFi + BLE)");
    Serial.println("================================================");
}

void setup() {
    // 1 KB RX buffer ensures the 80-channel HOP command line (~725 bytes)
    // from the T3S3 fits in one read without losing bytes if we momentarily
    // fall behind (e.g. during WiFi/BLE stack init). Must be called before
    // Serial.begin().
    Serial.setRxBufferSize(1024);
    Serial.begin(115200);
    // Give the USB-CDC host a moment to attach before we dump the banner.
    // On cold boot the first few serial lines are often lost otherwise.
    const uint32_t serialDeadline = millis() + 1500;
    while (!Serial && millis() < serialDeadline) {
        delay(10);
    }

    printBanner();

    const bool radioOk = xr1RadioBegin();
    if (!radioOk) {
        Serial.println("[XR1] FATAL: radio init failed -- continuing in degraded "
                       "mode so PING can report RADIO_FAIL to the T3S3.");
    } else {
        const Xr1RadioStatus &st = xr1RadioGetStatus();
        if (st.hwVersion != 0) {
            Serial.printf("[XR1] LR1121 version: 0x%08lX\n",
                          (unsigned long)st.hwVersion);
        } else {
            Serial.println("[XR1] LR1121 version: (not read -- RadioLib variant)");
        }
    }

    const bool selfTestOk = radioOk && xr1RadioHelloSelfTest();
    if (radioOk && !selfTestOk) {
        Serial.println("[XR1] Self-test FAILED. Check SPI + RF switch wiring.");
    }

    xr1UartInit();
    if (!radioOk || !selfTestOk) {
        // Mark the UART parser so PING returns ERR RADIO_FAIL instead of
        // cheerfully pretending everything's OK.
        xr1UartMarkRadioFailed();
    }
    remoteIdInit();     // NVS + esp_netif — RID still works even if LR1121 is dead
    Serial.println(radioOk && selfTestOk ? "XR1 READY"
                                         : "XR1 READY (RADIO_FAIL)");
}

void loop() {
    xr1UartUpdate();
    remoteIdUpdate();
    xr1LedUpdate();
    // Tight FHSS modes like ELRS 2.4 need the main loop to spin quickly enough
    // to honor sub-10 ms packet timers. When idle we still sleep to keep the
    // C3 cool and quiet.
    if (xr1UartNeedsFastLoop()) delay(0);
    else                        delay(20);
}
