// ============================================================================
// ASTM F3411-22a WiFi Neighbor Awareness Networking (NaN) — stubbed on the
// ESP32C3 per 7.md allowance. The esp_wifi_nan_* APIs are not exposed by the
// Arduino-ESP32 build of ESP-IDF for the C3 target (confirmed by grepping
// framework-arduinoespressif32/tools/sdk/esp32c3/include).
//
// If a future SDK adds support, fill in esp_wifi_nan_start() /
// esp_wifi_nan_publish() here; the transport plumbing (status flags, UART
// command, T3S3 wrapper) already exists.
// ============================================================================

#include "remote_id.h"
#include <Arduino.h>

extern RemoteIdStatus g_ridStatus;

bool remoteIdNanStart(const RemoteIdState &state) {
    (void)state;
    Serial.println("[XR1-RID] WiFi NaN not supported on this ESP-IDF build");
    g_ridStatus.nanActive = false;
    return false;
}

void remoteIdNanStop() {
    g_ridStatus.nanActive = false;
}
