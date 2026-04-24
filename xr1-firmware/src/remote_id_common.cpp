// ============================================================================
// Shared state + lifecycle hooks for the four Remote ID transports on the
// XR1's ESP32C3. Hosts the g_ridStatus global and the one-time NVS / netif /
// event-loop init that the WiFi and BLE modules both need before bringing up
// their respective stacks.
// ============================================================================

#include "remote_id.h"

#include <Arduino.h>

#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"

// Shared with the per-transport .cpp files via `extern`. Keeping a single
// status struct avoids each module carrying its own counters and lets the
// STATUS UART command be built from one place.
RemoteIdStatus g_ridStatus = {};
bool           g_ridCommonInitDone = false;

void ridCommonInit() {
    if (g_ridCommonInitDone) return;

    // NVS is required by the WiFi driver even if we never persist credentials.
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    esp_netif_init();
    esp_event_loop_create_default();

    g_ridCommonInitDone = true;
}

void remoteIdInit() {
    ridCommonInit();
}

// Implemented in the per-transport files; declared here as extern "C" so the
// update tick can call them through a uniform dispatch list.
extern "C" void ridWifiTick();
extern "C" void ridBleTick();
extern "C" void ridDjiTick();
// NaN has no tick (stubbed), so no function here.

void remoteIdUpdate() {
    ridWifiTick();
    ridBleTick();
    ridDjiTick();
}

void remoteIdStopAll() {
    if (g_ridStatus.wifiActive) remoteIdWifiStop();
    if (g_ridStatus.bleActive)  remoteIdBleStop();
    if (g_ridStatus.djiActive)  djiDroneIdStop();
    if (g_ridStatus.nanActive)  remoteIdNanStop();
}

const RemoteIdStatus &remoteIdGetStatus() {
    return g_ridStatus;
}
