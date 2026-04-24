#ifndef MENU_H
#define MENU_H

#include <Adafruit_SSD1306.h>

// ============================================================
// Menu State Machine
// Short press BOOT button = cycle selection
// Long press BOOT button  = confirm / enter / toggle
// ============================================================

// Top-level application states
enum AppState {
    STATE_MAIN_MENU,
    STATE_SIGGEN_MENU,    // Mode 2 submenu
    STATE_CW_ACTIVE,      // CW tone transmitting
    STATE_SWEEP_ACTIVE,   // Frequency sweep running
    STATE_ELRS_ACTIVE,    // ELRS FHSS transmitting
    STATE_FALSEPOS_MENU,  // Mode 3 submenu
    STATE_FP_ACTIVE,      // False positive mode running
    STATE_RID_ACTIVE,     // Mode 1: RID spoofer running
    STATE_COMBINED_ACTIVE,// Mode 4: Combined RID + ELRS
    STATE_SWARM_ACTIVE,   // Mode 5: Drone swarm simulator
    STATE_CROSSFIRE_ACTIVE,
    STATE_RAMP_ACTIVE,
    STATE_SIK_ACTIVE,
    STATE_MLRS_ACTIVE,
    STATE_CUSTOM_LORA_ACTIVE,
    STATE_INFRA_ACTIVE,
    STATE_XR1_ACTIVE,        // Phase 4: XR1 LR1121 running a 2.4 GHz protocol
    STATE_XR1_RID_ACTIVE,    // Phase 5: XR1 ESP32C3 emitting Remote ID (WiFi/BLE/DJI)
    STATE_COMBINED_SCENARIO_ACTIVE, // Phase 6: multi-emitter combined scenario
};

// Main menu item indices
enum MainMenuItem {
    MAIN_RID_SPOOFER = 0,
    MAIN_RF_SIGGEN,
    MAIN_FALSE_POS,
    MAIN_COMBINED,
    MAIN_SWARM,
    MAIN_COUNT  // sentinel — always last
};

// Signal generator submenu
enum SigGenMenuItem {
    SIGGEN_CW_TONE = 0,
    SIGGEN_SWEEP,
    SIGGEN_ELRS,
    SIGGEN_CROSSFIRE,
    SIGGEN_POWER_RAMP,
    SIGGEN_BACK,
    SIGGEN_COUNT
};

// Button press types returned by the debouncer
enum ButtonPress {
    BTN_NONE,
    BTN_SHORT,   // < 500 ms
    BTN_LONG     // >= 500 ms
};

// False positive submenu indices
enum FpMenuItem {
    FP_MENU_LORAWAN = 0,
    FP_MENU_ISM_BURST,
    FP_MENU_MIXED,
    FP_MENU_BACK,
    FP_MENU_COUNT
};

// --- Public API ---
void menuInit(Adafruit_SSD1306 *oled);
void menuUpdate();           // call every loop() iteration
AppState menuGetState();
void menuSetState(AppState s);  // set state directly (for serial mode switching)
void menuRequestRedraw();       // force OLED redraw on next update

// Button sampling — call from loop()
ButtonPress buttonRead();

#endif // MENU_H
