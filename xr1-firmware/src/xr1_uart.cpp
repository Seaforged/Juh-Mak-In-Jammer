#include "xr1_uart.h"

#include <Arduino.h>
#include <RadioLib.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include "xr1_radio.h"
#include "remote_id.h"
#include "elrs_crc14.h"

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

// ----- last-TX payload cache (for TXRPT + HOP template) --------------------
static uint8_t  s_lastPayload[XR1_PAYLOAD_MAX];
static size_t   s_lastPayloadLen = 0;
// When > 0, the HOP engine uses this seed to recompute CRC-14 per nonce
// (upstream ELRS: `crcInit = OtaCrcInitializer ^ nonce`). Set by the
// extended PAYLOAD command. Zero means "don't recompute CRC".
static uint16_t s_lastCrcSeed    = 0;
static bool     s_lastCrcValid   = false;

// Set by main.cpp when LR1121 init or self-test fails. When true, PING
// responds ERR RADIO_FAIL so the T3S3 knows the XR1 is unreachable for
// RF ops (RID-only operation would still be fine, but we keep the
// signal coarse-grained to surface the problem loudly).
static bool s_radioFailed = false;
void xr1UartMarkRadioFailed() { s_radioFailed = true; }

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
static uint32_t s_hopPktIntervalUs = 0;
static uint32_t s_hopNextPktUs     = 0;
static uint8_t  s_hopPktsPerHop    = 0;
static uint8_t  s_hopPktsSent      = 0;
static uint8_t  s_hopPayloadLen    = 0;
static uint8_t  s_hopSeq           = 0;

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
    s_hopPktIntervalUs = 0;
    s_hopPktsPerHop    = 0;
    s_hopPayloadLen    = 0;
    s_hopPktsSent      = 0;
}

// ----- per-command handlers ------------------------------------------------
static void cmdPing(char *) {
    if (s_radioFailed) { respondErrStr("RADIO_FAIL"); return; }
    respondOk();
}

static void cmdFreq(char *args) {
    float mhz;
    if (sscanf(args, "%f", &mhz) != 1) { respondErrStr("PARSE"); return; }
    int16_t rc = xr1RadioSetFrequency(mhz);
    if (rc == RADIOLIB_ERR_NONE) respondOk(); else respondErrCode(rc);
}

static void cmdLoRa(char *args) {
    // Format: LORA <sf> <bw> <cr> [<preamble> <implicit> [<len>]]
    //   preamble — uint16 symbol count (default left at XR1's 8)
    //   implicit — 0 = explicit header (default), 1 = implicit header
    //   len      — fixed payload length for implicit-header modes
    unsigned sf, cr, preamble = 0, implicit = 0, implicitLen = 0;
    float    bw;
    int n = sscanf(args, "%u %f %u %u %u %u",
                   &sf, &bw, &cr, &preamble, &implicit, &implicitLen);
    if (n < 3) { respondErrStr("PARSE"); return; }

    int16_t rc = xr1RadioSetLoRa((uint8_t)sf, bw, (uint8_t)cr);
    if (rc != RADIOLIB_ERR_NONE) { respondErrCode(rc); return; }

    if (n >= 4 && preamble > 0) {
        rc = xr1RadioSetPreamble((uint16_t)preamble);
        if (rc != RADIOLIB_ERR_NONE) { respondErrCode(rc); return; }
    }
    if (n >= 5) {
        const uint8_t payloadLen = (n >= 6 && implicitLen > 0)
                                 ? (uint8_t)implicitLen
                                 : 8;
        rc = implicit ? xr1RadioSetImplicitHeader(payloadLen)
                      : xr1RadioSetExplicitHeader();
        if (rc != RADIOLIB_ERR_NONE) { respondErrCode(rc); return; }
    }
    respondOk();
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

// PAYLOAD <hex> [<seed_hex>] — cache a template for subsequent TXRPT/HOP
// transmits. Unlike TX, this does not transmit; it only stores.
//
// Without <seed_hex>: the XR1 HOP engine rolls byte-0 (nonce) but leaves
// the template's CRC untouched. Used for non-ELRS protocols.
//
// With <seed_hex> (4 hex chars = uint16 seed): the XR1 HOP engine, on
// each TX, rolls byte-0 and recomputes CRC-14 over bytes [0..N-3] with
// crcInit = seed XOR nonce (upstream ExpressLRS OtaCrcInitializer formula).
// This makes every packet in the nonce cycle carry a valid CRC instead of
// only nonce=0's CRC — closing the ELRS 2.4 GHz L4 content gap.
static void cmdPayload(char *args) {
    // Split optional seed off the end.
    char *space = strchr(args, ' ');
    uint16_t seed   = 0;
    bool     hasSeed = false;
    if (space) {
        *space = '\0';
        char *seedStr = space + 1;
        while (*seedStr == ' ' || *seedStr == '\t') ++seedStr;
        if (*seedStr) {
            unsigned s = 0;
            if (sscanf(seedStr, "%x", &s) != 1) { respondErrStr("PARSE"); return; }
            seed = (uint16_t)(s & 0xFFFF);
            hasSeed = true;
        }
    }

    uint8_t buf[XR1_PAYLOAD_MAX];
    const int n = decodeHex(args, buf, XR1_PAYLOAD_MAX);
    if (n <= 0) { respondErrStr("PARSE"); return; }
    memcpy(s_lastPayload, buf, n);
    s_lastPayloadLen = n;
    s_lastCrcSeed    = seed;
    s_lastCrcValid   = hasSeed;
    respondOk();
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
    // args: "f1,f2,f3,... <dwell_ms> [<pkt_interval_us> <pkts_per_hop> <payload_len>]"
    char *parts[5] = {};
    int argc = 0;
    char *save = nullptr;
    for (char *tok = strtok_r(args, " ", &save);
         tok && argc < 5;
         tok = strtok_r(nullptr, " ", &save)) {
        parts[argc++] = tok;
    }
    if (argc != 2 && argc != 5) { respondErrStr("PARSE"); return; }

    unsigned dwell = 0;
    unsigned pktIntervalUs = 0;
    unsigned pktsPerHop = 0;
    unsigned payloadLen = 0;
    if (sscanf(parts[1], "%u", &dwell) != 1 || dwell == 0) {
        respondErrStr("PARSE");
        return;
    }
    if (argc == 5) {
        if (sscanf(parts[2], "%u", &pktIntervalUs) != 1
         || sscanf(parts[3], "%u", &pktsPerHop) != 1
         || sscanf(parts[4], "%u", &payloadLen) != 1
         || pktIntervalUs == 0 || pktsPerHop == 0
         || payloadLen == 0 || payloadLen > XR1_PAYLOAD_MAX) {
            respondErrStr("PARSE");
            return;
        }
    }

    uint8_t count = 0;
    char *chanSave = nullptr;
    char *tok = strtok_r(parts[0], ",", &chanSave);
    while (tok) {
        if (count >= XR1_HOP_MAX_CH) {
            // Too many channels — reject the whole command rather than
            // silently truncating. The caller is responsible for keeping
            // within XR1_HOP_MAX_CH (80).
            respondErrStr("TOO_MANY_CHANNELS");
            return;
        }
        float f;
        if (sscanf(tok, "%f", &f) != 1) { respondErrStr("PARSE"); return; }
        s_hopChannels[count++] = f;
        tok = strtok_r(nullptr, ",", &chanSave);
    }
    if (count == 0) { respondErrStr("PARSE"); return; }

    s_hopCount   = count;
    s_hopIdx     = 0;
    s_hopDwellMs = (uint16_t)dwell;
    s_hopNextMs  = millis();
    s_hopPktIntervalUs = (uint32_t)pktIntervalUs;
    s_hopPktsPerHop    = (uint8_t)pktsPerHop;
    s_hopPayloadLen    = (uint8_t)payloadLen;
    s_hopPktsSent      = 0;
    s_hopSeq           = 0;
    s_hopNextPktUs     = micros();
    xr1RadioSetFrequency(s_hopChannels[0]);
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

// ----- RID subcommand dispatch ---------------------------------------------
// The top-level parser hands us everything after "RID " as a single args
// string. Example: "WIFI JJ-XR1-TEST-001 36.8529 -75.9780 120.0 5.0 270.0".
// Sub-verb is the first word; the remainder parses per-verb.
static bool parseRidArgs(char *s, RemoteIdState &out) {
    // Lat/lon parse as double (%lf) — float's 7-digit precision is
    // marginal for degree-level GPS coordinates and would truncate the
    // sub-meter accuracy the ODID/DJI encoders preserve downstream.
    // Altitude/speed/heading stay as float (they encode to int16 anyway).
    double lat = 0.0, lon = 0.0;
    float  alt = 0, spd = 0, hdg = 0;
    char serial[32] = { 0 };
    int  n = sscanf(s, "%31s %lf %lf %f %f %f",
                    serial, &lat, &lon, &alt, &spd, &hdg);
    if (n != 6) return false;
    strncpy(out.serial, serial, sizeof(out.serial) - 1);
    out.serial[sizeof(out.serial) - 1] = '\0';
    out.latitude       = lat;
    out.longitude      = lon;
    out.altitudeMeters = alt;
    out.speedMps       = spd;
    out.headingDeg     = hdg;
    return true;
}

static void cmdRid(char *args) {
    // Split sub-verb from the rest.
    while (*args == ' ' || *args == '\t') ++args;
    char *rest = args;
    while (*rest && *rest != ' ' && *rest != '\t') ++rest;
    if (*rest) { *rest++ = '\0'; while (*rest == ' ' || *rest == '\t') ++rest; }
    // Uppercase the sub-verb.
    for (char *p = args; *p; ++p) *p = (char)toupper((unsigned char)*p);

    if (strcmp(args, "STOP") == 0) {
        remoteIdStopAll();
        respondOk();
        return;
    }
    if (strcmp(args, "STATUS") == 0) {
        const RemoteIdStatus &st = remoteIdGetStatus();
        Serial.printf("OK WIFI:%s BLE:%s DJI:%s NAN:%s\n",
                      st.wifiActive ? "on" : "off",
                      st.bleActive  ? "on" : "off",
                      st.djiActive  ? "on" : "off",
                      st.nanActive  ? "on" : "off");
        return;
    }

    if (strcmp(args, "WIFI") == 0) {
        RemoteIdState s;
        if (!parseRidArgs(rest, s)) { respondErrStr("PARSE"); return; }
        if (remoteIdWifiStart(s)) respondOk(); else respondErrStr("WIFI_FAIL");
        return;
    }
    if (strcmp(args, "BLE") == 0) {
        RemoteIdState s;
        if (!parseRidArgs(rest, s)) { respondErrStr("PARSE"); return; }
        if (remoteIdBleStart(s))  respondOk(); else respondErrStr("BLE_FAIL");
        return;
    }
    if (strcmp(args, "NAN") == 0) {
        RemoteIdState s;
        if (!parseRidArgs(rest, s)) { respondErrStr("PARSE"); return; }
        // NaN is stubbed on C3; returns false. Report specifically so the
        // T3S3 can surface "not supported" to the operator.
        if (remoteIdNanStart(s))  respondOk();
        else                      respondErrStr("NAN_UNSUPPORTED");
        return;
    }
    if (strcmp(args, "DJI") == 0) {
        // DJI takes an extra trailing <model> param. Parse the 6 common
        // fields first, then grab the model off the tail.
        RemoteIdState s;
        char modelStr[16] = { 0 };
        double lat, lon;
        float alt, spd, hdg;
        char serial[32];
        int n = sscanf(rest, "%31s %lf %lf %f %f %f %15s",
                       serial, &lat, &lon, &alt, &spd, &hdg, modelStr);
        if (n != 7) { respondErrStr("PARSE"); return; }
        strncpy(s.serial, serial, sizeof(s.serial) - 1);
        s.serial[sizeof(s.serial) - 1] = '\0';
        s.latitude       = lat;
        s.longitude      = lon;
        s.altitudeMeters = alt;
        s.speedMps       = spd;
        s.headingDeg     = hdg;

        uint16_t model = 0x0A;   // default Mini 2
        if      (strcmp(modelStr, "M2") == 0)    model = 0x03;
        else if (strcmp(modelStr, "MINI2") == 0) model = 0x0A;
        else if (strcmp(modelStr, "AIR2S") == 0) model = 0x11;

        if (djiDroneIdStart(s, model)) respondOk(); else respondErrStr("DJI_FAIL");
        return;
    }

    respondErrStr("RID_UNKNOWN");
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
    { "RID",    cmdRid    },
    { "PAYLOAD",cmdPayload},
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
    if (s_hopPktIntervalUs > 0 && s_hopPktsPerHop > 0 && s_hopPayloadLen > 0) {
        // If the host pre-pushed a wire-authentic template via PAYLOAD (e.g.
        // a real ELRS OTA packet with CRC-14 computed on the T3S3), use it
        // so every TX carries that exact byte pattern. Otherwise fall back
        // to a synthetic hop template with a visible hop index + seq number.
        static const uint8_t kHopTemplate[13] = {
            0xE1, 0x25, 0x00, 0x00, 0x05, 0x7A, 0x3C,
            0xAA, 0x42, 0x19, 0x9C, 0x55, 0x00
        };
        const bool useCached = (s_lastPayloadLen > 0
                                && s_lastPayloadLen == s_hopPayloadLen);
        const uint32_t nowUs = micros();
        uint8_t burstBudget = 4;
        while (burstBudget-- > 0 && (int32_t)(nowUs - s_hopNextPktUs) >= 0) {
            uint8_t payload[XR1_PAYLOAD_MAX];
            if (useCached) {
                memcpy(payload, s_lastPayload, s_hopPayloadLen);
                // Nonce-roll byte 0 lower 6 bits (ELRS OTA header layout).
                const uint8_t nonce = (uint8_t)(s_hopSeq & 0x3F);
                payload[0] = (uint8_t)((payload[0] & 0xC0) | nonce);
                s_hopSeq++;

                // If the T3S3 pushed a CRC seed with PAYLOAD, recompute
                // CRC-14 over bytes [0..N-3] using crcInit = seed ^ nonce
                // (matching ExpressLRS OTA per-packet CRC init). Write the
                // 14-bit result back into the last two bytes in the same
                // big-endian layout the T3S3 builder used.
                if (s_lastCrcValid && s_hopPayloadLen >= 3) {
                    const uint16_t crcInit = (uint16_t)(s_lastCrcSeed ^ nonce);
                    const uint16_t crc     = elrs_crc14(payload,
                                                        s_hopPayloadLen - 2,
                                                        crcInit);
                    payload[s_hopPayloadLen - 2] = (uint8_t)((crc >> 6) & 0xFF);
                    payload[s_hopPayloadLen - 1] = (uint8_t)((crc << 2) & 0xFC);
                }
            } else {
                for (uint8_t i = 0; i < s_hopPayloadLen; ++i) {
                    payload[i] = kHopTemplate[i % sizeof(kHopTemplate)];
                }
                payload[s_hopPayloadLen - 1] = s_hopSeq++;
                if (s_hopPayloadLen > 1) payload[s_hopPayloadLen - 2] = s_hopIdx;
            }
            xr1RadioTransmit(payload, s_hopPayloadLen);

            ++s_hopPktsSent;
            if (s_hopPktsSent >= s_hopPktsPerHop) {
                s_hopPktsSent = 0;
                s_hopIdx = (uint8_t)((s_hopIdx + 1) % s_hopCount);
                xr1RadioSetFrequency(s_hopChannels[s_hopIdx]);
            }
            s_hopNextPktUs += s_hopPktIntervalUs;
        }
        return;
    }
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
    // Serial is already begun by main setup() -- we don't re-init it here to
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

bool xr1UartNeedsFastLoop() {
    return s_hopActive || s_txRptActive;
}
