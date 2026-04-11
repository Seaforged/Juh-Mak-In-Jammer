#include "xr1_commands.h"

// TODO: UART command parser (Phase 2)
// See docs/JJ_XR1_Phased_Development_Plan.md §2.1 for the full command set
// (FREQ, MOD LORA/FSK, PWR, TX, TXRPT, STOP, HOP, BAND?, STATUS?, RESET).

static HardwareSerial *s_host = nullptr;

void xr1CommandsBegin(HardwareSerial &hostPort) {
    s_host = &hostPort;
}

void xr1CommandsPoll() {
    // Phase 2 will drain the UART line-by-line here and dispatch into the
    // radio wrapper. No-op for Phase 1.
    (void)s_host;
}
