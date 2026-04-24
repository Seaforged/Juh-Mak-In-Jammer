// ============================================================================
// T3S3-side control for XR1 Remote ID emission. Translates a bitmask of
// desired transports into a sequence of UART commands (`RID WIFI ...`,
// `RID BLE ...`, `RID DJI ...`, `RID NAN ...`) and tracks which transports
// reported OK.
// ============================================================================

#include "xr1_rid_modes.h"
#include "xr1_driver.h"

#include <Arduino.h>
#include <string.h>

// HardwareSerial handle used by xr1_driver. We piggyback on the same
// Serial1 without re-initializing it. The raw sendCmd helper in xr1_driver
// isn't exposed; to avoid refactoring that module, we build the RID command
// strings here and write them to Serial1 directly, reading the response.
extern HardwareSerial Serial1;

static Xr1RidStatus s_status = {};

// Default drone params — Virginia Beach, matching T3S3's rid_spoofer
// defaults so XR1 and T3S3 RID emissions line up if both are running.
static constexpr const char *DEFAULT_SERIAL = "JJ-XR1-TEST-001";
static constexpr double  DEFAULT_LAT = 36.8529;
static constexpr double  DEFAULT_LON = -75.9780;
static constexpr float   DEFAULT_ALT = 120.0f;
static constexpr float   DEFAULT_SPD = 5.0f;
static constexpr float   DEFAULT_HDG = 270.0f;

static bool readLineWithTimeout(char *buf, size_t bufSize, uint32_t timeoutMs) {
    const uint32_t deadline = millis() + timeoutMs;
    size_t n = 0;
    while (millis() < deadline) {
        while (Serial1.available() > 0 && n + 1 < bufSize) {
            int ci = Serial1.read();
            if (ci < 0) break;
            char c = (char)ci;
            if (c == '\r') continue;
            if (c == '\n') { buf[n] = '\0'; return true; }
            buf[n++] = c;
        }
        delay(1);
    }
    buf[n] = '\0';
    return false;
}

// Send a single RID sub-command; tolerate async XR1 debug lines. Returns
// true iff a line starting with "OK" came back before timeout.
static bool sendRidCmd(const char *cmd, uint32_t timeoutMs = 2000) {
    while (Serial1.available() > 0) { (void)Serial1.read(); }   // drain
    Serial1.print(cmd); Serial1.print('\n');

    const uint32_t deadline = millis() + timeoutMs;
    char line[160];
    while (millis() < deadline) {
        const uint32_t now = millis();
        if (now >= deadline) break;
        if (!readLineWithTimeout(line, sizeof(line), (uint32_t)(deadline - now))) {
            break;
        }
        if (strncmp(line, "OK",  2) == 0) return true;
        if (strncmp(line, "ERR", 3) == 0) {
            Serial.printf("[XR1-RID-ERR] '%s' -> '%s'\n", cmd, line);
            return false;
        }
        // Any other line is async XR1 debug — keep waiting.
    }
    Serial.printf("[XR1-RID-ERR] timeout on '%s'\n", cmd);
    return false;
}

bool xr1RidStart(uint8_t mask) {
    // If something is already running, clean slate first.
    if (s_status.running) xr1RidStop();

    // Capture params into status for OLED.
    strncpy(s_status.serial, DEFAULT_SERIAL, sizeof(s_status.serial) - 1);
    s_status.serial[sizeof(s_status.serial) - 1] = '\0';
    s_status.latitude       = DEFAULT_LAT;
    s_status.longitude      = DEFAULT_LON;
    s_status.altitudeMeters = DEFAULT_ALT;

    // Build the "serial lat lon alt spd hdg" suffix once.
    char args[96];
    snprintf(args, sizeof(args), "%s %.4f %.4f %.1f %.1f %.1f",
             DEFAULT_SERIAL, DEFAULT_LAT, DEFAULT_LON,
             DEFAULT_ALT, DEFAULT_SPD, DEFAULT_HDG);

    uint8_t started = 0;
    char cmd[160];

    if (mask & XR1_RID_WIFI) {
        snprintf(cmd, sizeof(cmd), "RID WIFI %s", args);
        if (sendRidCmd(cmd))  { started |= XR1_RID_WIFI;  Serial.println("[XR1-RID] WiFi beacon started"); }
    }
    if (mask & XR1_RID_BLE) {
        snprintf(cmd, sizeof(cmd), "RID BLE %s", args);
        if (sendRidCmd(cmd))  { started |= XR1_RID_BLE;   Serial.println("[XR1-RID] BLE advertisement started"); }
    }
    if (mask & XR1_RID_DJI) {
        snprintf(cmd, sizeof(cmd), "RID DJI %s MINI2", args);
        if (sendRidCmd(cmd))  { started |= XR1_RID_DJI;   Serial.println("[XR1-RID] DJI DroneID started"); }
    }
    if (mask & XR1_RID_NAN) {
        snprintf(cmd, sizeof(cmd), "RID NAN %s", args);
        if (sendRidCmd(cmd))  { started |= XR1_RID_NAN;   Serial.println("[XR1-RID] WiFi NaN started"); }
        else Serial.println("[XR1-RID] WiFi NaN not supported on XR1 (expected)");
    }

    s_status.activeMask = started;
    s_status.running    = (started != 0);
    return s_status.running;
}

void xr1RidStop() {
    if (!s_status.running) return;
    sendRidCmd("RID STOP");
    memset(&s_status, 0, sizeof(s_status));
    Serial.println("[XR1-RID] stopped");
}

Xr1RidStatus xr1RidGetStatus() {
    return s_status;
}
