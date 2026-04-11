#pragma once

// ============================================================================
// UART command parser — T3S3 host to XR1.
// Phase 2 deliverable. Commands are plain ASCII at 115200 baud terminated
// with '\n'. Responses are "OK\n" on success or "ERR <reason>\n" on failure.
//
// Planned command set (see docs/JJ_XR1_Phased_Development_Plan.md §2.1):
//   FREQ <MHz>, MOD LORA <SF> <BW> <CR>, MOD FSK <BR> <DEV>,
//   PWR <dBm>, TX <hex>, TXRPT <ms> <count>, STOP,
//   HOP <ch1,ch2,...> <dwell_ms>, BAND?, STATUS?, RESET
//
// Phase 1 only wires the entry points so the command parser source file
// exists and compiles; no command dispatch yet.
// ============================================================================

#include <Arduino.h>

void xr1CommandsBegin(HardwareSerial &hostPort);
void xr1CommandsPoll();
