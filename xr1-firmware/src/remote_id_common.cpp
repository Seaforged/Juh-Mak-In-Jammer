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
#include "esp_wifi.h"

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

// ----- unified WiFi channel rotation ---------------------------------------
// Both the ODID WiFi beacon and DJI DroneID transports hop through channels
// 1/6/11. Phase 5 initially had each module run its own rotation timer, which
// produced a race when both were active at once (c5 Everything scenario):
// ODID rotated every 3 s and DJI every 2 s, so each one's esp_wifi_set_channel
// call overwrote the other's — beacons sometimes went out on the "wrong"
// channel for their module. Phase 6 arch review H1 called this out.
//
// Fix: one channel controller owned here. Rotates every 2 s (the faster of
// the two original rates; slower would hurt DJI coverage) whenever any WiFi
// transport is active. Individual transports just transmit on whatever
// channel is currently set — they no longer touch esp_wifi_set_channel.
static constexpr uint8_t WIFI_RID_CHANS[]   = { 1, 6, 11 };
static constexpr uint32_t WIFI_CHAN_DWELL_MS = 2000;

static uint32_t s_wifiChanLastMs = 0;
static uint8_t  s_wifiChanIdx    = 0;

static void ridWifiChanTick(uint32_t now) {
    const bool anyWifiTx = g_ridStatus.wifiActive || g_ridStatus.djiActive;
    if (!anyWifiTx) {
        // Reset state so the next session starts on channel 1 immediately.
        s_wifiChanLastMs = 0;
        s_wifiChanIdx    = 0;
        return;
    }
    if (s_wifiChanLastMs == 0) {
        // First tick of a new session — park on channel 1 right now and
        // arm the dwell timer.
        s_wifiChanIdx = 0;
        esp_wifi_set_channel(WIFI_RID_CHANS[0], WIFI_SECOND_CHAN_NONE);
        s_wifiChanLastMs = now;
        return;
    }
    if (now - s_wifiChanLastMs < WIFI_CHAN_DWELL_MS) return;
    s_wifiChanIdx = (uint8_t)((s_wifiChanIdx + 1) % (sizeof(WIFI_RID_CHANS) / sizeof(WIFI_RID_CHANS[0])));
    esp_wifi_set_channel(WIFI_RID_CHANS[s_wifiChanIdx], WIFI_SECOND_CHAN_NONE);
    s_wifiChanLastMs = now;
}

// Implemented in the per-transport files; declared here as extern "C" so the
// update tick can call them through a uniform dispatch list.
extern "C" void ridWifiTick();
extern "C" void ridBleTick();
extern "C" void ridDjiTick();
// NaN has no tick (stubbed), so no function here.

void remoteIdUpdate() {
    const uint32_t now = millis();
    ridWifiChanTick(now);   // rotate first, then transmit on the new channel
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
