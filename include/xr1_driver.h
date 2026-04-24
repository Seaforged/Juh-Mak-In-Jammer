#ifndef XR1_DRIVER_H
#define XR1_DRIVER_H

// ============================================================================
// T3S3-side driver for the XR1 LR1121 UART command protocol. Wraps the
// newline-terminated ASCII protocol documented in xr1-firmware/include/
// xr1_uart.h so the host code can call typed functions instead of building
// strings by hand.
//
// Transport: HardwareSerial `Serial1` on GPIO 43 (TX, -> XR1 RX / GPIO 20)
// and GPIO 44 (RX, <- XR1 TX / GPIO 21), 115200 8N1. Response timeout is
// 500 ms by default; override with xr1SetTimeoutMs() if a caller needs longer.
//
// Every function returns true iff the XR1 responded with a line starting
// with "OK". On any error (timeout, ERR response, XR1 unreachable), the
// error string is printed to the USB Serial as "[XR1-ERR] <msg>".
// ============================================================================

#include <Arduino.h>
#include <stdint.h>
#include <stddef.h>

void xr1Init();                                      // Serial1.begin() @ 115200 on GPIO 43/44
void xr1SetTimeoutMs(uint32_t ms);                   // change response timeout

bool xr1Ping();
bool xr1PingWithTimeout(uint32_t timeoutMs);
bool xr1SetFreq(float mhz);
// Base form leaves preamble and header mode at XR1 defaults (8 symbols,
// explicit header). Extended form appends "<preamble> <implicit> [<len>]" so
// protocol profiles like ELRS can request their exact preamble, implicit
// header mode, and fixed payload length.
bool xr1SetLoRa(uint8_t sf, float bwKhz, uint8_t cr);
bool xr1SetLoRaEx(uint8_t sf, float bwKhz, uint8_t cr,
                  uint16_t preamble, bool implicitHeader,
                  uint8_t implicitLen = 0);
bool xr1SetFSK(float bitrateKbps, float devKhz);
bool xr1SetPower(int8_t dbm);
bool xr1Transmit(const uint8_t *data, uint8_t len);
bool xr1StartHop(const float *channels, uint8_t count, uint16_t dwellMs);
bool xr1StartHopEx(const float *channels, uint8_t count, uint16_t dwellMs,
                   uint32_t packetIntervalUs, uint8_t packetsPerHop,
                   uint8_t payloadLen);
bool xr1Stop();
bool xr1GetStatus(char *buf, size_t bufLen);         // copies the response line (minus leading "OK ")

#endif // XR1_DRIVER_H
