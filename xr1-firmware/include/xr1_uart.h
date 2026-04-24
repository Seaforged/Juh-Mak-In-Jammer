#pragma once

// ============================================================================
// XR1 UART command parser — ASCII line protocol on Serial (UART0, GPIO 20/21)
// that the T3S3 master drives. Commands are newline-terminated; responses are
// single-line (OK, DONE, or ERR <code>), newline-terminated.
//
// Command set (see docs/3.md for the authoritative protocol):
//   PING                          -> OK
//   FREQ <MHz>                    -> OK | ERR <code>
//   LORA <SF> <BW_kHz> <CR> [<preamble> <implicit> [<len>]]
//                                -> OK | ERR <code>
//   FSK  <BR_kbps> <DEV_kHz>      -> OK | ERR <code>
//   PWR  <dBm>                    -> OK | ERR <code>
//   TX   <hex_payload>            -> OK | ERR <code>
//   TXRPT <interval_ms> <count>   -> OK  (then DONE when complete)
//   HOP   <ch1,ch2,...> <dwell> [<pkt_us> <pkts_per_hop> <payload_len>]
//                                -> OK  (runs until STOP)
//   STOP                          -> OK
//   STATUS                        -> OK <freq> <mod> <pwr> <state>
//   RESET                         -> OK  (then radio re-inits)
//   LED   <0|1|2|3|4>             -> OK
//   PAYLOAD <hex> [<seed_hex>]    -> OK
//        Cache a TX template for TXRPT/HOP. With optional 4-char hex seed,
//        the HOP engine recomputes ELRS CRC-14 per nonce on every TX:
//        crcInit = seed XOR (byte0 & 0x3F), matching ExpressLRS upstream.
// ============================================================================

void xr1UartInit();

// Called from loop() every iteration. Non-blocking: reads any available bytes,
// runs line dispatch on newline, and advances the TXRPT / HOP state machines.
void xr1UartUpdate();

// True when the loop should avoid long sleeps because a high-rate TX state
// machine is running (ELRS-style hopping or tight TXRPT intervals).
bool xr1UartNeedsFastLoop();

// Called by main.cpp when xr1RadioBegin() or the self-test fails so the
// parser can respond `ERR RADIO_FAIL` to PING. Without this the T3S3
// would see a healthy PING response and proceed to configure a dead
// radio.
void xr1UartMarkRadioFailed();
