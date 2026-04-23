// ============================================================================
// T3S3 -> XR1 UART driver. Thin wrapper around Serial1 that formats the ASCII
// command protocol defined in xr1-firmware/include/xr1_uart.h. Blocking on
// response (up to xr1SetTimeoutMs) so callers can treat XR1 operations as
// synchronous — Phase 3 infrastructure only, not used by operator-facing
// serial menu commands yet.
// ============================================================================

#include "xr1_driver.h"

#include <Arduino.h>
#include <string.h>
#include <stdio.h>

// Wiring: ESP32-S3 is flexible, so UART1 is mapped to the two pins ND routed
// to the XR1. See docs/XR1_Integration_Research.md §3.3.
static constexpr uint8_t  PIN_S3_TX_TO_XR1_RX = 43;  // T3S3 TX -> XR1 GPIO 20
static constexpr uint8_t  PIN_S3_RX_FROM_XR1_TX = 44;  // T3S3 RX <- XR1 GPIO 21
static constexpr uint32_t XR1_BAUD            = 115200;

static constexpr size_t   RESP_BUF_SZ         = 128;

static uint32_t s_timeoutMs = 500;

void xr1SetTimeoutMs(uint32_t ms) { s_timeoutMs = ms; }

void xr1Init() {
    // Larger RX buffer so the XR1's ~350-byte boot banner doesn't overflow
    // the default 256-byte HardwareSerial ring and drop "XR1 READY" before
    // we get a chance to see it. Must be called before begin().
    Serial1.setRxBufferSize(1024);
    Serial1.begin(XR1_BAUD, SERIAL_8N1, PIN_S3_RX_FROM_XR1_TX, PIN_S3_TX_TO_XR1_RX);

    // The XR1 emits ~1 s worth of self-test text on UART before its UART
    // parser is installed. Wait for the "XR1 READY" sentinel (or fall
    // through on timeout) and echo everything we read to USB Serial so the
    // operator can see the XR1's boot log mirrored through the T3S3.
    Serial.println("[XR1-BOOT] listening for XR1 banner...");
    const uint32_t deadline = millis() + 3500;
    String line;
    bool sawReady = false;
    size_t totalRx = 0;
    while (millis() < deadline && !sawReady) {
        while (Serial1.available() > 0) {
            const int ci = Serial1.read();
            if (ci < 0) break;
            ++totalRx;
            const char c = (char)ci;
            if (c == '\r') continue;
            if (c == '\n') {
                if (line.length() > 0) Serial.printf("[XR1-BOOT] %s\n", line.c_str());
                if (line.indexOf("XR1 READY") >= 0) { sawReady = true; break; }
                line = "";
            } else if (line.length() < 128) {
                line += c;
            }
        }
        if (!sawReady) delay(5);
    }
    Serial.printf("[XR1-BOOT] end — sawReady=%d totalRxBytes=%u\n",
                  (int)sawReady, (unsigned)totalRx);
    delay(50);
    while (Serial1.available() > 0) { (void)Serial1.read(); }
}

// Read a single line (up to '\n' or timeout). Strips trailing \r\n.
// Returns the number of bytes stored (excluding null terminator), or -1 on
// timeout.
static int readResponse(char *buf, size_t bufSize) {
    const uint32_t deadline = millis() + s_timeoutMs;
    size_t n = 0;
    while (millis() < deadline) {
        while (Serial1.available() > 0 && n + 1 < bufSize) {
            const int ci = Serial1.read();
            if (ci < 0) break;
            const char c = (char)ci;
            if (c == '\r') continue;
            if (c == '\n') { buf[n] = '\0'; return (int)n; }
            buf[n++] = c;
        }
        delay(1);
    }
    buf[n] = '\0';
    return -1;
}

// Is this line a protocol response (OK / ERR / DONE) rather than an async
// debug message from the XR1 (e.g., "[XR1] ..." or a banner line)?
static bool isResponseLine(const char *line) {
    return (strncmp(line, "OK", 2) == 0)
        || (strncmp(line, "ERR", 3) == 0)
        || (strncmp(line, "DONE", 4) == 0);
}

// Send a pre-formatted command line + '\n', then read lines until either a
// real response arrives or the timeout window closes. Any intermediate lines
// that look like async debug output are silently dropped — otherwise, boot
// banners and future asynchronous DONE messages would be mistaken for
// responses. Returns true iff we saw an OK line.
static bool sendCmdExpectOk(const char *cmd, char *respOut = nullptr, size_t respOutSize = 0) {
    // Drain anything that might have accumulated since the last command.
    while (Serial1.available() > 0) { (void)Serial1.read(); }

    Serial1.print(cmd);
    Serial1.print('\n');

    const uint32_t deadline = millis() + s_timeoutMs;
    char resp[RESP_BUF_SZ];
    while (millis() < deadline) {
        // Compute remaining time so readResponse can honor the overall deadline.
        const uint32_t remaining = deadline - millis();
        const uint32_t savedTo = s_timeoutMs;
        s_timeoutMs = remaining;
        const int n = readResponse(resp, sizeof(resp));
        s_timeoutMs = savedTo;

        if (n < 0) break;         // hit deadline with no newline — treat as timeout
        if (!isResponseLine(resp)) continue;   // async debug; keep waiting

        if (respOut && respOutSize > 0) {
            strncpy(respOut, resp, respOutSize - 1);
            respOut[respOutSize - 1] = '\0';
        }
        if (strncmp(resp, "OK", 2) == 0) return true;
        Serial.printf("[XR1-ERR] cmd '%s' -> '%s'\n", cmd, resp);
        return false;
    }
    Serial.printf("[XR1-ERR] timeout on '%s'\n", cmd);
    return false;
}

bool xr1Ping() {
    return sendCmdExpectOk("PING");
}

bool xr1SetFreq(float mhz) {
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "FREQ %.3f", mhz);
    return sendCmdExpectOk(cmd);
}

bool xr1SetLoRa(uint8_t sf, float bwKhz, uint8_t cr) {
    char cmd[48];
    snprintf(cmd, sizeof(cmd), "LORA %u %.3f %u",
             (unsigned)sf, bwKhz, (unsigned)cr);
    return sendCmdExpectOk(cmd);
}

bool xr1SetFSK(float bitrateKbps, float devKhz) {
    char cmd[48];
    snprintf(cmd, sizeof(cmd), "FSK %.3f %.3f", bitrateKbps, devKhz);
    return sendCmdExpectOk(cmd);
}

bool xr1SetPower(int8_t dbm) {
    char cmd[24];
    snprintf(cmd, sizeof(cmd), "PWR %d", (int)dbm);
    return sendCmdExpectOk(cmd);
}

bool xr1Transmit(const uint8_t *data, uint8_t len) {
    if (len == 0 || len > 64) {
        Serial.println("[XR1-ERR] xr1Transmit: invalid length");
        return false;
    }
    // "TX " + 128 hex + '\0' = 132
    char cmd[140];
    size_t pos = 0;
    pos += snprintf(cmd + pos, sizeof(cmd) - pos, "TX ");
    for (uint8_t i = 0; i < len; ++i) {
        pos += snprintf(cmd + pos, sizeof(cmd) - pos, "%02X", data[i]);
    }
    return sendCmdExpectOk(cmd);
}

bool xr1StartHop(const float *channels, uint8_t count, uint16_t dwellMs) {
    if (count == 0 || count > 32) {
        Serial.println("[XR1-ERR] xr1StartHop: invalid channel count");
        return false;
    }
    // "HOP " + (≤ 32 × ~9 chars) + " " + dwell
    char cmd[384];
    size_t pos = 0;
    pos += snprintf(cmd + pos, sizeof(cmd) - pos, "HOP ");
    for (uint8_t i = 0; i < count; ++i) {
        pos += snprintf(cmd + pos, sizeof(cmd) - pos, "%s%.3f",
                        i == 0 ? "" : ",", channels[i]);
    }
    pos += snprintf(cmd + pos, sizeof(cmd) - pos, " %u", (unsigned)dwellMs);
    return sendCmdExpectOk(cmd);
}

bool xr1Stop() {
    return sendCmdExpectOk("STOP");
}

bool xr1GetStatus(char *buf, size_t bufLen) {
    if (!buf || bufLen == 0) return false;
    char resp[RESP_BUF_SZ];
    if (!sendCmdExpectOk("STATUS", resp, sizeof(resp))) return false;

    // Response format: "OK <freq> <mod> <pwr> <state>" — strip leading "OK ".
    const char *body = resp;
    if (strncmp(body, "OK", 2) == 0) body += 2;
    while (*body == ' ') ++body;
    strncpy(buf, body, bufLen - 1);
    buf[bufLen - 1] = '\0';
    return true;
}
