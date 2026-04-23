#include "xr1_uart.h"

#include <Arduino.h>
#include <RadioLib.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include "xr1_radio.h"

// ----- constants ------------------------------------------------------------
// Names deliberately avoid LINE_MAX / PATH_MAX / ARG_MAX etc. — POSIX headers
// pulled in via Arduino.h define those as macros and shadow any C++ constant
// that shares the spelling.
// Bumped from 256 to 1024 for Phase 4 — the 80-channel ELRS 2.4 GHz HOP
// command line is ~725 bytes ("HOP " + 80 × "2440.400," + " <dwell>").
static constexpr size_t XR1_LINE_BUF   = 1024;
static constexpr size_t XR1_PAYLOAD_MAX = 64;    // bytes after hex decode
// Bumped from 32 to 80 for Phase 4 — full ELRS 2.4 GHz channel count.
static constexpr size_t XR1_HOP_MAX_CH  = 80;

// ----- line buffer ----------------------------------------------------------
static char    s_line[XR1_LINE_BUF];
static size_t  s_lineLen    = 0;
static bool    s_lineBroken = false;  // set true once we've overflowed;
                                      // cleared when the next '\n' arrives

// ----- last-TX payload cache (for TXRPT) -----------------------------------
static uint8_t s_lastPayload[XR1_PAYLOAD_MAX];
static size_t  s_lastPayloadLen = 0;

// ----- TXRPT state ----------------------------------------------------------
static bool      s_txRptActive    = false;
static uint32_t  s_txRptIntervalMs = 0;
static uint32_t  s_txRptRemaining  = 0;
static uint32_t  s_txRptNextMs     = 0;

// ----- HOP state ------------------------------------------------------------
static bool     s_hopActive     = false;
static float    s_hopChannels[XR1_HOP_MAX_CH];
static uint8_t  s_hopCount      = 0;
static uint8_t  s_hopIdx        = 0;
static uint16_t s_hopDwellMs    = 0;
static uint32_t s_hopNextMs     = 0;

// ----- response helpers -----------------------------------------------------
static inline void respondOk()            { Serial.println("OK"); }
static inline void respondDone()          { Serial.println("DONE"); }
static inline void respondErrCode(int c)  { Serial.printf("ERR %d\n", c); }
static inline void respondErrStr(const char *s){ Serial.printf("ERR %s\n", s); }

// ----- hex-payload decode ---------------------------------------------------
static bool hexNibble(char c, uint8_t &out) {
    if (c >= '0' && c <= '9') { out = c - '0';        return true; }
    if (c >= 'a' && c <= 'f') { out = 10 + (c - 'a'); return true; }
    if (c >= 'A' && c <= 'F') { out = 10 + (c - 'A'); return true; }
    return false;
}

static int decodeHex(const char *hex, uint8_t *out, size_t outMax) {
    const size_t n = strlen(hex);
    if (n == 0 || (n & 1))        return -1;          // empty / odd length
    if (n / 2 > outMax)           return -1;
    for (size_t i = 0; i < n; i += 2) {
        uint8_t hi, lo;
        if (!hexNibble(hex[i],     hi)) return -1;
        if (!hexNibble(hex[i + 1], lo)) return -1;
        out[i / 2] = (hi << 4) | lo;
    }
    return (int)(n / 2);
}

// ----- state-machine cancellation ------------------------------------------
static void cancelRepeatAndHop() {
    s_txRptActive = false;
    s_hopActive   = false;
}

// ----- per-command handlers ------------------------------------------------
static void cmdPing(char *) { respondOk(); }

static void cmdFreq(char *args) {
    float mhz;
    if (sscanf(args, "%f", &mhz) != 1) { respondErrStr("PARSE"); return; }
    int16_t rc = xr1RadioSetFrequency(mhz);
    if (rc == RADIOLIB_ERR_NONE) respondOk(); else respondErrCode(rc);
}

static void cmdLoRa(char *args) {
    unsigned sf, cr;
    float    bw;
    if (sscanf(args, "%u %f %u", &sf, &bw, &cr) != 3) { respondErrStr("PARSE"); return; }
    int16_t rc = xr1RadioSetLoRa((uint8_t)sf, bw, (uint8_t)cr);
    if (rc == RADIOLIB_ERR_NONE) respondOk(); else respondErrCode(rc);
}

static void cmdFsk(char *args) {
    float br, dev;
    if (sscanf(args, "%f %f", &br, &dev) != 2) { respondErrStr("PARSE"); return; }
    int16_t rc = xr1RadioSetFSK(br, dev);
    if (rc == RADIOLIB_ERR_NONE) respondOk(); else respondErrCode(rc);
}

static void cmdPwr(char *args) {
    int dbm;
    if (sscanf(args, "%d", &dbm) != 1) { respondErrStr("PARSE"); return; }
    int16_t rc = xr1RadioSetPower((int8_t)dbm);
    if (rc == RADIOLIB_ERR_NONE) respondOk(); else respondErrCode(rc);
}

static void cmdTx(char *args) {
    uint8_t buf[XR1_PAYLOAD_MAX];
    const int n = decodeHex(args, buf, XR1_PAYLOAD_MAX);
    if (n <= 0) { respondErrStr("PARSE"); return; }
    int16_t rc = xr1RadioTransmit(buf, (size_t)n);
    if (rc == RADIOLIB_ERR_NONE) {
        memcpy(s_lastPayload, buf, n);
        s_lastPayloadLen = n;
        respondOk();
    } else {
        respondErrCode(rc);
    }
}

static void cmdTxRpt(char *args) {
    unsigned interval, count;
    if (sscanf(args, "%u %u", &interval, &count) != 2) { respondErrStr("PARSE"); return; }
    if (s_lastPayloadLen == 0) { respondErrStr("NOTX"); return; }   // TX first
    if (count == 0)            { respondErrStr("RANGE"); return; }

    s_txRptActive     = true;
    s_txRptIntervalMs = interval;
    s_txRptRemaining  = count;
    s_txRptNextMs     = millis();   // fire immediately on the next update tick
    respondOk();
}

static void cmdHop(char *args) {
    // args: "f1,f2,f3,... <dwell_ms>"
    char *space = strrchr(args, ' ');
    if (!space) { respondErrStr("PARSE"); return; }
    *space = '\0';
    char *chanList = args;
    char *dwellStr = space + 1;

    unsigned dwell;
    if (sscanf(dwellStr, "%u", &dwell) != 1 || dwell == 0) { respondErrStr("PARSE"); return; }

    uint8_t count = 0;
    char *tok = strtok(chanList, ",");
    while (tok && count < XR1_HOP_MAX_CH) {
        float f;
        if (sscanf(tok, "%f", &f) != 1) { respondErrStr("PARSE"); return; }
        s_hopChannels[count++] = f;
        tok = strtok(nullptr, ",");
    }
    if (count == 0) { respondErrStr("PARSE"); return; }

    s_hopCount   = count;
    s_hopIdx     = 0;
    s_hopDwellMs = (uint16_t)dwell;
    s_hopNextMs  = millis();
    s_hopActive  = true;
    respondOk();
}

static void cmdStop(char *) {
    cancelRepeatAndHop();
    respondOk();
}

static void cmdStatus(char *) {
    const Xr1RadioStatus &st = xr1RadioGetStatus();
    const char *state = s_hopActive ? "HOP"
                      : s_txRptActive ? "TXRPT"
                      : "IDLE";
    const char *mod = (st.mod == XR1_MOD_LORA) ? "LORA" : "GFSK";
    Serial.printf("OK %.3f %s %d %s\n", st.freqMhz, mod, st.powerDbm, state);
}

static void cmdReset(char *) {
    respondOk();          // acknowledge before we pulse the reset line
    Serial.flush();       // ensure "OK" actually leaves the UART
    cancelRepeatAndHop();
    s_lastPayloadLen = 0;
    xr1RadioReset();      // pulseReset + begin()
}

static void cmdLed(char *args) {
    unsigned mode;
    if (sscanf(args, "%u", &mode) != 1 || mode > 4) { respondErrStr("PARSE"); return; }
    xr1LedSetOverride((Xr1LedMode)mode);
    respondOk();
}

// ----- dispatch -------------------------------------------------------------
struct CmdEntry {
    const char *name;
    void (*handler)(char *args);
};

static const CmdEntry kCommands[] = {
    { "PING",   cmdPing   },
    { "FREQ",   cmdFreq   },
    { "LORA",   cmdLoRa   },
    { "FSK",    cmdFsk    },
    { "PWR",    cmdPwr    },
    { "TX",     cmdTx     },
    { "TXRPT",  cmdTxRpt  },
    { "HOP",    cmdHop    },
    { "STOP",   cmdStop   },
    { "STATUS", cmdStatus },
    { "RESET",  cmdReset  },
    { "LED",    cmdLed    },
};

static void dispatch(char *line) {
    // Skip leading whitespace.
    while (*line == ' ' || *line == '\t') ++line;
    if (*line == '\0') return;   // silently ignore blank lines

    // Split verb and args on first whitespace.
    char *args = line;
    while (*args && *args != ' ' && *args != '\t') ++args;
    if (*args) { *args++ = '\0'; while (*args == ' ' || *args == '\t') ++args; }

    // Uppercase the verb for case-insensitive match.
    for (char *p = line; *p; ++p) *p = (char)toupper((unsigned char)*p);

    for (const CmdEntry &e : kCommands) {
        if (strcmp(line, e.name) == 0) { e.handler(args); return; }
    }
    respondErrStr("UNKNOWN");
}

// ----- state-machine tick --------------------------------------------------
static void tickTxRpt(uint32_t now) {
    if (!s_txRptActive)                  return;
    if ((int32_t)(now - s_txRptNextMs) < 0) return;

    xr1RadioTransmit(s_lastPayload, s_lastPayloadLen);
    s_txRptNextMs = now + s_txRptIntervalMs;
    if (--s_txRptRemaining == 0) {
        s_txRptActive = false;
        respondDone();
    }
}

static void tickHop(uint32_t now) {
    if (!s_hopActive)                   return;
    if ((int32_t)(now - s_hopNextMs) < 0) return;

    xr1RadioSetFrequency(s_hopChannels[s_hopIdx]);
    // Tag the hop with a tiny LoRa probe so the RF is actually on-air during
    // dwell. This keeps the test-signal envelope consistent across channels.
    const uint8_t probe[4] = { 'H', 'O', 'P', s_hopIdx };
    xr1RadioTransmit(probe, sizeof(probe));

    s_hopIdx  = (s_hopIdx + 1) % s_hopCount;
    s_hopNextMs = now + s_hopDwellMs;
}

// ----- public API -----------------------------------------------------------
void xr1UartInit() {
    // Serial is already begun by main setup() — we don't re-init it here to
    // avoid clobbering the initial banner. Zero local state only.
    s_lineLen = 0;
    s_lineBroken = false;
    s_lastPayloadLen = 0;
    cancelRepeatAndHop();
}

void xr1UartUpdate() {
    // Feed the parser from any buffered UART bytes.
    while (Serial.available() > 0) {
        const int ci = Serial.read();
        if (ci < 0) break;
        const char c = (char)ci;

        if (c == '\r') continue;        // tolerate CRLF
        if (c == '\n') {
            if (s_lineBroken) {
                respondErrStr("OVERFLOW");
                s_lineBroken = false;
            } else if (s_lineLen > 0) {
                s_line[s_lineLen] = '\0';
                dispatch(s_line);
            }
            s_lineLen = 0;
            continue;
        }

        if (s_lineLen < XR1_LINE_BUF - 1) {
            s_line[s_lineLen++] = c;
        } else {
            s_lineBroken = true;  // swallow the rest of this overlong line
        }
    }

    const uint32_t now = millis();
    tickTxRpt(now);
    tickHop(now);
}
