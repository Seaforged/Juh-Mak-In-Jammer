#ifndef COMBINED_SCENARIOS_H
#define COMBINED_SCENARIOS_H

// ============================================================================
// Phase 6: multi-emitter scenarios that drive the T3S3 SX1262 sub-GHz stack
// AND the XR1's LR1121 2.4 GHz + WiFi + BLE emitters simultaneously, so the
// RF signature on the air matches a real drone type (sub-GHz control link +
// 2.4 GHz video/telemetry + Remote ID broadcasts).
//
// Each scenario is a pure orchestrator — it calls the existing per-emitter
// start functions (elrsStart, crossfireStart, xr1ModeElrs2g4Start,
// xr1RidStart) with a scenario-specific config. No new radio-driver code
// lives here.
//
// Scenarios:
//   c1  Racing Drone    — ELRS 915 + ELRS 2.4 + ODID WiFi/BLE
//   c2  DJI Consumer    — (no sub-GHz) + DJI energy 2.4 + DJI DroneID + ODID BLE
//   c3  Long Range FPV  — Crossfire 915 + ODID WiFi/BLE
//   c4  Dual-Band ELRS  — ELRS 915 + ELRS 2.4 only (no RID)
//   c5  Everything      — ELRS 915 + ELRS 2.4 + WiFi ODID + BLE ODID + DJI
// ============================================================================

#include <Arduino.h>
#include <stdint.h>

// Each function returns true if all component emitters came up cleanly.
// Callers should transition the menu state to STATE_COMBINED_SCENARIO_ACTIVE
// on success.
bool combinedScenarioRacing();
bool combinedScenarioDji();
bool combinedScenarioLongRange();
bool combinedScenarioDualBand();
bool combinedScenarioEverything();

// Drive whichever sub-GHz emitter the active scenario owns. Call from the
// menu state handler for STATE_COMBINED_SCENARIO_ACTIVE every iteration —
// without this, ELRS/Crossfire stay parked on their initial frequency
// because their own Update functions are only called from their dedicated
// menu-state cases.
void combinedScenarioUpdate();

// Tear down every emitter across both boards. Safe to call when nothing is
// running.
void combinedScenarioStop();

// Which scenario is currently active. 0 = none.
enum CombinedScenarioId : uint8_t {
    COMBINED_NONE      = 0,
    COMBINED_RACING    = 1,
    COMBINED_DJI       = 2,
    COMBINED_LONGRANGE = 3,
    COMBINED_DUALBAND  = 4,
    COMBINED_EVERYTHING = 5,
};

struct CombinedScenarioStatus {
    CombinedScenarioId id;
    const char *name;
    bool subGhzActive;        // T3S3 SX1262 running some protocol
    bool twoGhzActive;        // XR1 LR1121 running 2.4 GHz FHSS
    bool wifiRidActive;       // XR1 ODID WiFi beacon
    bool bleRidActive;        // XR1 ODID BLE advert
    bool djiRidActive;        // XR1 DJI DroneID
    const char *subGhzLabel;  // short OLED string, e.g. "ELRS 200Hz"
    const char *twoGhzLabel;
};
CombinedScenarioStatus combinedScenarioGetStatus();

#endif // COMBINED_SCENARIOS_H
