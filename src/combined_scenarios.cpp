// ============================================================================
// Phase 6: multi-emitter scenario engine. Composes existing per-emitter
// start functions into realistic drone RF signatures. No radio code here —
// every RF operation is delegated to the module that owns its emitter.
// ============================================================================

#include "combined_scenarios.h"

#include "rf_modes.h"          // elrsSetRate, elrsSetDomain, elrsStart, elrsStop
#include "crossfire.h"         // crossfireStart, crossfireStop
#include "xr1_modes.h"         // xr1ModeElrs2g4Start, xr1ModeDjiEnergyStart, xr1ModesStop
#include "xr1_rid_modes.h"     // xr1RidStart, xr1RidStop, XR1_RID_* masks
#include "protocol_params.h"   // ELRS_RATE_*, ELRS_DOMAIN_*

#include <Arduino.h>

static CombinedScenarioStatus s_status = { COMBINED_NONE, "None",
                                           false, false, false, false, false,
                                           "", "" };

// ----- banner printer ------------------------------------------------------
// Matches the format specified in docs/8.md so the operator's terminal
// shows exactly which emitters are on the air for each scenario.
static constexpr const char *DEFAULT_SERIAL = "JJ-XR1-TEST-001";

static void printBanner(const char *scenarioName,
                        const char *subGhzLine,
                        const char *twoGhzLine,
                        const char *wifiLine,
                        const char *bleLine) {
    Serial.printf("[COMBINED: %s]\n", scenarioName);
    if (subGhzLine) Serial.printf("  SX1262: %s\n", subGhzLine);
    else            Serial.println("  SX1262: idle");
    if (twoGhzLine) Serial.printf("  LR1121: %s\n", twoGhzLine);
    else            Serial.println("  LR1121: idle");
    if (wifiLine)   Serial.printf("  WiFi:   %s\n", wifiLine);
    else            Serial.println("  WiFi:   idle");
    if (bleLine)    Serial.printf("  BLE:    %s\n", bleLine);
    else            Serial.println("  BLE:    idle");
    Serial.println("  Position: 36.8529\xB0N 75.9780\xB0W Alt: 120m Speed: 5 m/s Hdg: 270\xB0");
}

// ----- scenarios -----------------------------------------------------------
bool combinedScenarioRacing() {
    combinedScenarioStop();
    elrsSetRate(ELRS_RATE_200HZ);
    elrsSetDomain(ELRS_DOMAIN_FCC915);
    if (!elrsStart())                                   return false;
    if (!xr1ModeElrs2g4Start(0))                       { elrsStop(); return false; }
    xr1RidStart(XR1_RID_WIFI | XR1_RID_BLE);   // best-effort

    s_status = { COMBINED_RACING, "Racing Drone",
                 true, true, true, true, false,
                 "ELRS 200Hz", "ELRS 2.4G 500Hz" };
    printBanner("Racing Drone",
                "ELRS-FCC915 40ch SF6/BW500 200Hz implicit 0x12 | 10 dBm",
                "ELRS-ISM2G4 80ch SF6/BW812.5 500Hz | 12 dBm",
                "ODID beacon ch1/6/11 1Hz | Serial: " "JJ-XR1-TEST-001",
                "ODID Legacy ADV 3:1 Location | UUID 0xFFFA");
    return true;
}

bool combinedScenarioDji() {
    combinedScenarioStop();
    // No sub-GHz emitter in DJI scenarios — DJI control is 2.4 GHz only.
    if (!xr1ModeDjiEnergyStart())                       return false;
    xr1RidStart(XR1_RID_DJI | XR1_RID_BLE);

    s_status = { COMBINED_DJI, "DJI Consumer",
                 false, true, false, true, true,
                 "idle", "DJI energy" };
    printBanner("DJI Consumer",
                nullptr,
                "DJI-energy GFSK 250k/50k 20ch 2400.5-2481.5MHz dwell 50ms | 12 dBm",
                "DJI DroneID OUI 26:37:12 200ms ch1/6/11 | Serial: " "JJ-XR1-TEST-001",
                "ODID Legacy ADV 3:1 Location | UUID 0xFFFA");
    Serial.println("  NOTE: real DJI video is OFDM; LR1121 GFSK is energy approximation only");
    return true;
}

bool combinedScenarioLongRange() {
    combinedScenarioStop();
    crossfireStart();   // Crossfire FSK 150 Hz @ 915
    xr1RidStart(XR1_RID_WIFI | XR1_RID_BLE);

    s_status = { COMBINED_LONGRANGE, "Long Range FPV",
                 true, false, true, true, false,
                 "CRSF 915 FSK", "idle" };
    printBanner("Long Range FPV",
                "Crossfire-915 FSK 85kbps ~150Hz | 10 dBm",
                nullptr,
                "ODID beacon ch1/6/11 1Hz | Serial: " "JJ-XR1-TEST-001",
                "ODID Legacy ADV 3:1 Location | UUID 0xFFFA");
    return true;
}

bool combinedScenarioDualBand() {
    combinedScenarioStop();
    elrsSetRate(ELRS_RATE_200HZ);
    elrsSetDomain(ELRS_DOMAIN_FCC915);
    if (!elrsStart())                                   return false;
    if (!xr1ModeElrs2g4Start(0))                       { elrsStop(); return false; }

    s_status = { COMBINED_DUALBAND, "Dual-Band ELRS",
                 true, true, false, false, false,
                 "ELRS 200Hz", "ELRS 2.4G 500Hz" };
    printBanner("Dual-Band ELRS",
                "ELRS-FCC915 40ch SF6/BW500 200Hz implicit 0x12 | 10 dBm",
                "ELRS-ISM2G4 80ch SF6/BW812.5 500Hz | 12 dBm",
                nullptr,
                nullptr);
    return true;
}

bool combinedScenarioEverything() {
    combinedScenarioStop();
    elrsSetRate(ELRS_RATE_200HZ);
    elrsSetDomain(ELRS_DOMAIN_FCC915);
    if (!elrsStart())                                   return false;
    if (!xr1ModeElrs2g4Start(0))                       { elrsStop(); return false; }
    xr1RidStart(XR1_RID_ALL);    // WiFi ODID + BLE ODID + DJI DroneID

    s_status = { COMBINED_EVERYTHING, "Everything",
                 true, true, true, true, true,
                 "ELRS 200Hz", "ELRS 2.4G 500Hz" };
    printBanner("Everything",
                "ELRS-FCC915 40ch SF6/BW500 200Hz implicit 0x12 | 10 dBm",
                "ELRS-ISM2G4 80ch SF6/BW812.5 500Hz | 12 dBm",
                "ODID beacon + DJI DroneID ch1/6/11 | Serial: " "JJ-XR1-TEST-001",
                "ODID Legacy ADV 3:1 Location | UUID 0xFFFA");
    return true;
}

// ----- sub-GHz update pump -------------------------------------------------
void combinedScenarioUpdate() {
    if (!s_status.subGhzActive) return;
    if (s_status.id == COMBINED_LONGRANGE) crossfireUpdate();
    else                                   elrsUpdate();
}

// ----- stop ----------------------------------------------------------------
// Tear down every emitter across both boards regardless of which scenario
// was active. Each underlying stop is safe to call when nothing is running.
void combinedScenarioStop() {
    if (s_status.id == COMBINED_NONE) return;

    // T3S3 sub-GHz
    if (s_status.subGhzActive) {
        if (s_status.id == COMBINED_LONGRANGE) crossfireStop();
        else                                   elrsStop();
    }
    // XR1 LR1121
    if (s_status.twoGhzActive)  xr1ModesStop();
    // XR1 WiFi/BLE/DJI (RID) — single call tears down all transports
    if (s_status.wifiRidActive || s_status.bleRidActive || s_status.djiRidActive) {
        xr1RidStop();
    }

    Serial.printf("[COMBINED: %s] stopped\n", s_status.name);
    s_status = { COMBINED_NONE, "None",
                 false, false, false, false, false, "", "" };
}

CombinedScenarioStatus combinedScenarioGetStatus() {
    return s_status;
}
