// ============================================================================
// T3S3 UART passthrough — bridges USB Serial <-> Serial1 (GPIO 43/44) so
// esptool can flash the XR1 LR1121 (ESP32C3) through the T3S3 once the CP2102
// is no longer wired. This sketch replaces the normal JJ firmware temporarily;
// re-flash the real firmware after XR1 programming is complete.
//
// Flashing procedure:
//   1. Flash this sketch to T3S3 via COM6:
//        cd tools/xr1_passthrough && pio run -e xr1_passthrough --target upload
//   2. Put the XR1 in ROM bootloader mode:
//        hold BIND/BOOT on XR1, unplug+replug T3S3 USB (which power-cycles
//        the XR1 via the 5 V rail), hold ~1 s after power returns, release.
//   3. Flash the XR1 firmware through the passthrough at 115200 baud:
//        python .../esptool.py --chip esp32c3 --port COM6 --baud 115200 \
//            write_flash 0x0 xr1-firmware/.pio/build/xr1/bootloader.bin \
//                        0x8000 .../partitions.bin \
//                        0xe000 .../boot_app0.bin \
//                        0x10000 .../firmware.bin
//      (higher baud rates won't work reliably through a software bridge)
//   4. Reflash the real T3S3 firmware from the repo root:
//        pio run -e t3s3 --target upload --upload-port COM6
// ============================================================================

#include <Arduino.h>

static constexpr uint8_t  XR1_TX_PIN = 43;   // T3S3 TX -> XR1 RX
static constexpr uint8_t  XR1_RX_PIN = 44;   // T3S3 RX <- XR1 TX
static constexpr uint32_t BAUD       = 115200;
static constexpr uint8_t  LED_PIN    = 37;

void setup() {
    // Large buffers so esptool's SLIP frames don't overrun at 115200 baud
    // while we're round-tripping them through two separate UART drivers.
    Serial.setRxBufferSize(2048);
    Serial.begin(BAUD);
    Serial1.setRxBufferSize(2048);
    Serial1.begin(BAUD, SERIAL_8N1, XR1_RX_PIN, XR1_TX_PIN);

    // Solid LED = the T3S3 is in passthrough mode, not running the real
    // firmware. Flipping this pin is enough of a tell during flash ops.
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);
}

void loop() {
    uint8_t buf[256];

    // USB -> XR1
    int n = Serial.available();
    if (n > 0) {
        if (n > (int)sizeof(buf)) n = sizeof(buf);
        n = Serial.readBytes(buf, n);
        Serial1.write(buf, n);
    }

    // XR1 -> USB
    n = Serial1.available();
    if (n > 0) {
        if (n > (int)sizeof(buf)) n = sizeof(buf);
        n = Serial1.readBytes(buf, n);
        Serial.write(buf, n);
    }
}
