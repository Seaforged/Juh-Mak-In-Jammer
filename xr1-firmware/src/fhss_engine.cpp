// TODO: FHSS hopping state machine (Phase 3)
//
// Autonomous frequency hopping driven off an ISR-safe timer. Must support
// arbitrary channel lists, configurable dwell time, and pseudo-random
// sequences so ELRS/Crossfire/FrSky hop patterns look correct to passive
// detectors rather than landing on channels in boring numerical order.

#include <Arduino.h>

// Intentionally empty in Phase 1 — the translation unit exists so later
// sprints have a known filename to drop into.
