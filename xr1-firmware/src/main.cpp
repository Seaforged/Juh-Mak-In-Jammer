// ============================================================================
// JJ XR1 Firmware — Phase 1 Hello Hardware
//
// Runs on the ESP32C3 inside a RadioMaster XR1. Responsibilities for this
// phase are deliberately small: bring up the LR1121, prove sub-GHz and
// 2.4 GHz paths work, and print "XR1 READY" so Phase 2 (UART command
// protocol) has a known-good starting point.
//
// Part of the Juh-Mak-In Jammer v3.0 three-emitter architecture. See
// docs/JJ_XR1_Phased_Development_Plan.md for the full roadmap.
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
    Serial.println("  Phase 1: Hello Hardware");
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

    if (!xr1RadioBegin()) {
        Serial.println("[XR1] FATAL: radio init failed — halting.");
        return;
    }

    const Xr1RadioStatus &st = xr1RadioGetStatus();
    if (st.hwVersion != 0) {
        Serial.printf("[XR1] LR1121 version: 0x%08lX\n",
                      (unsigned long)st.hwVersion);
    } else {
        Serial.println("[XR1] LR1121 version: (not read — RadioLib variant)");
    }

    if (!xr1RadioHelloSelfTest()) {
        Serial.println("[XR1] Self-test FAILED. Check SPI + RF switch wiring.");
        return;
    }

    xr1UartInit();
    remoteIdInit();     // NVS + esp_netif — stacks spin up lazily on first RID start
    Serial.println("XR1 READY");
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
