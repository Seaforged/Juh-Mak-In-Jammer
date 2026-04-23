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
    Serial.println("XR1 READY");
}

void loop() {
    xr1UartUpdate();
    xr1LedUpdate();
    // 20 Hz LED refresh is plenty smooth for the blink patterns we render and
    // doesn't starve the UART parser — xr1UartUpdate is itself non-blocking.
    delay(50);
}
