#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RadioLib.h>
#include "board_config.h"
#include "version.h"
#include "menu.h"
#include "rf_modes.h"
#include "false_positive.h"
#include "rid_spoofer.h"
#include "combined_mode.h"
#include "swarm_sim.h"
#include "crossfire.h"
#include "power_ramp.h"
#include "sik_radio.h"
#include "mlrs_sim.h"
#include "custom_lora.h"
#include "infra_sim.h"
#include "protocol_params.h"
#include "splash.h"
#include "xr1_driver.h"
#include "xr1_modes.h"

// --- OLED Display ---
Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);

// --- LoRa Radio ---
SPIClass loraSPI(HSPI);
SX1262 radio = new Module(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY, loraSPI);

// --- Boot Splash Screen (shown for 2 seconds) ---
static void showBootScreen() {
    display.clearDisplay();

    // Drone + RF wave arcs bitmap in the top 40 pixels
    display.drawBitmap(0, 0, splash_bmp, SPLASH_WIDTH, SPLASH_HEIGHT, SSD1306_WHITE);

    // Project name centered at y=44
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(10, 44);
    display.print("JUH-MAK-IN JAMMER");

    // Version string centered at y=56
    display.setCursor(40, 56);
    display.printf("v%s", JAMMER_VERSION);

    display.display();
}

// --- Help Menu ---
static void printHelp() {
    Serial.printf("=== %s v%s — Drone Signal Emulator ===\n\n", JAMMER_NAME, JAMMER_VERSION);

    Serial.println("DRONE PROTOCOLS:");
    Serial.println("  e  ELRS FHSS      e1-e6=rate  f/a/u/i=domain  b=binding");
    Serial.println("  g  Crossfire      g/g9=915 g8=868 gl=LoRa50Hz");
    Serial.println("  k  SiK Radio      k1=64k  k2=125k  k3=250k");
    Serial.println("  l  mLRS           l1=19Hz  l2=31Hz  l3=50Hz(FSK)");
    Serial.println("  u  Custom LoRa    u?=settings  uf/us/ub/ur/uh/up/uw=config");
    Serial.println("  x1-x4 ELRS 2.4G   500/250/150/50Hz via XR1");
    Serial.println("  x5 Ghost 2.4G     approximate (proprietary)");
    Serial.println("  x6 FrSky D16 2.4G GFSK 250k/50k footprint");
    Serial.println("  x7 FlySky 2A 2.4G GFSK 250k/50k [APPROX]");
    Serial.println("  x8 DJI Energy 2.4G GFSK bursts [APPROX, not OcuSync]");
    Serial.println("  x9 Generic 2.4G   args: L|F <params...>");
    Serial.println("  xq Stop XR1       (leaves sub-GHz running)");

    Serial.println("\nINFRASTRUCTURE (False Positive Testing):");
    Serial.println("  i  LoRaWAN US915  Single node, 8 SB2 channels, 30-60s");
    Serial.println("  m  Mixed FP       LoRaWAN + ELRS interleaved");
    Serial.println("  f1 Meshtastic     16-sym preamble, sync 0x2B");
    Serial.println("  f2 Helium PoC     5 hotspots, rotating SB2 channels");
    Serial.println("  f3 LoRaWAN EU868  868.1/868.3/868.5 MHz");

    Serial.println("\nSPECIAL MODES:");
    Serial.println("  c  CW Tone        b=sweep  t=power ramp");
    Serial.println("  r  Remote ID      WiFi+BLE ASTM F3411 broadcast");
    Serial.println("  x  Combined       RID(Core0) + ELRS(Core1)");
    Serial.println("  w  Drone Swarm    n=cycle count (1/4/8/16)");

    Serial.println("\nCONTROLS:");
    Serial.println("  q  Stop TX        p=cycle power  h/?=this menu");
    Serial.println();
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("================================");
    Serial.printf("  %s v%s\n", JAMMER_NAME, JAMMER_VERSION);
    Serial.printf("  Component: %s\n", JAMMER_COMPONENT);
    Serial.printf("  Board: %s\n", BOARD_NAME);
    Serial.println("================================");

    // --- LED ---
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);

    // --- Boot Button ---
    pinMode(BOOT_BTN, INPUT_PULLUP);

    // --- OLED Init ---
    Wire.begin(OLED_SDA, OLED_SCL);
    if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
        Serial.println("ERROR: OLED init failed!");
    } else {
        Serial.println("OLED: OK");
        showBootScreen();
    }

    // --- LoRa SX1262 Init ---
    loraSPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);

    Serial.print("SX1262 init... ");
    int state = radio.begin(915.0, 125.0, 9, 7,
                            RADIOLIB_SX126X_SYNC_WORD_PRIVATE, 10, 8,
                            1.8, false);
    if (state == RADIOLIB_ERR_NONE) {
        Serial.println("OK");
    } else {
        Serial.printf("FAILED (error %d)\n", state);
    }

    // --- Initialize subsystems ---
    cwInit(&radio);
    fpInit(&radio);
    ridInit();
    combinedInit(&radio);
    swarmInit();
    crossfireInit(&radio);
    powerRampInit(&radio);
    sikInit(&radio);
    mlrsInit(&radio);
    customLoraInit(&radio);
    infraInit(&radio);
    menuInit(&display);

    // --- XR1 UART link (Phase 3) ---
    // The XR1 runs its own firmware and sends "XR1 READY" over UART after
    // boot. We ping it here so the operator sees whether the secondary
    // emitter is reachable before the menu takes over.
    xr1Init();
    Serial.println(xr1Ping() ? "[XR1] Connected" : "[XR1] No response — check wiring");

    // Diagnostic: dump XR1 self-reported state. "OK <freq> <mod> <pwr> <state>"
    // tells us whether LR1121 init + self-test succeeded (healthy) or whether
    // the red-blinking LED is hiding a failed radio init.
    {
        char statusBuf[128];
        if (xr1GetStatus(statusBuf, sizeof(statusBuf))) {
            Serial.printf("[XR1-STATUS] %s\n", statusBuf);
        } else {
            Serial.println("[XR1-STATUS] query failed");
        }
    }

    // Hold boot screen for 2 seconds so user can read it
    delay(2000);

    // --- Ready ---
    Serial.println();
    printHelp();

    digitalWrite(LED_PIN, LOW);
}

// --- Stop whatever mode is currently active ---
static void stopCurrentMode() {
    AppState st = menuGetState();
    if (st == STATE_CW_ACTIVE)       cwStop();
    if (st == STATE_SWEEP_ACTIVE)    sweepStop();
    if (st == STATE_ELRS_ACTIVE)     elrsStop();
    if (st == STATE_FP_ACTIVE)       fpStop();
    if (st == STATE_RID_ACTIVE)      ridStop();
    if (st == STATE_COMBINED_ACTIVE) combinedStop();
    if (st == STATE_SWARM_ACTIVE)    swarmStop();
    if (st == STATE_CROSSFIRE_ACTIVE) crossfireStop();
    if (st == STATE_RAMP_ACTIVE)     powerRampStop();
    if (st == STATE_SIK_ACTIVE)      sikStop();
    if (st == STATE_MLRS_ACTIVE)     mlrsStop();
    if (st == STATE_CUSTOM_LORA_ACTIVE) customLoraStop();
    if (st == STATE_INFRA_ACTIVE)    infraStop();
    if (st == STATE_XR1_ACTIVE)      xr1ModesStop();
}

// --- Serial command parser ---
static void handleSerialCommands() {
    if (!Serial.available()) return;

    char cmd = Serial.read();
    AppState st = menuGetState();

    switch (cmd) {
    case 'd':
        if (st == STATE_SWEEP_ACTIVE) {
            sweepCycleDwell();
        } else {
            Serial.println("(dwell only applies in sweep mode)");
        }
        break;

    case 's':
        if (st == STATE_SWEEP_ACTIVE) {
            sweepCycleStep();
        } else {
            Serial.println("(step only applies in sweep mode)");
        }
        break;

    case 'p':
        rfCyclePower();
        break;

    case 'q':
        stopCurrentMode();
        menuSetState(STATE_MAIN_MENU);
        Serial.println("TX stopped via serial.");
        break;

    // --- Direct mode selection via serial ---
    case 'c':   // CW Tone
        stopCurrentMode();
        cwStart();
        menuSetState(STATE_CW_ACTIVE);
        { CwParams p = cwGetParams();
          Serial.printf("[MODE] CW Tone: %.2f MHz, %d dBm\n", p.freqMHz, p.powerDbm); }
        break;

    case 'e': { // ELRS FHSS — e[1-6][f/a/u/i][b]
        // Parse optional rate digit, domain letter, binding flag
        delay(80);  // allow multi-char command to arrive
        uint8_t rateIdx = 0;   // default: 200 Hz
        uint8_t domIdx  = ELRS_DOMAIN_FCC915;  // default: FCC915
        bool    binding = false;

        // Rate digit (e1-e6)
        if (Serial.available()) {
            char c = Serial.peek();
            if (c >= '1' && c <= '6') {
                Serial.read();
                rateIdx = c - '1';
            }
        }
        delay(20);

        // Domain letter (f/a/u/i)
        if (Serial.available()) {
            char c = Serial.peek();
            switch (c) {
            case 'f': Serial.read(); domIdx = ELRS_DOMAIN_FCC915; break;
            case 'a': Serial.read(); domIdx = ELRS_DOMAIN_AU915;  break;
            case 'u': Serial.read(); domIdx = ELRS_DOMAIN_EU868;  break;
            case 'i': Serial.read(); domIdx = ELRS_DOMAIN_IN866;  break;
            default: break;  // no domain letter → keep default
            }
        }
        delay(20);

        // Binding flag (b)
        if (Serial.available() && Serial.peek() == 'b') {
            Serial.read();
            binding = true;
        }

        stopCurrentMode();
        elrsSetRate(rateIdx);
        elrsSetDomain(domIdx);
        bool ok;
        if (binding) {
            ok = elrsStartBinding();
        } else {
            ok = elrsStart();
        }
        if (ok) menuSetState(STATE_ELRS_ACTIVE);
        break;
    }

    case 'k': { // SiK Radio GFSK — optional digit selects air speed
        delay(50);
        uint8_t speedIdx = 1;  // default: 64 kbps (index 1 in SIK_AIR_SPEEDS_KBPS)
        if (Serial.available()) {
            char c = Serial.peek();
            if (c >= '1' && c <= '3') {
                Serial.read();
                speedIdx = c - '1' + 1;  // '1'→1(64k), '2'→2(125k), '3'→3(250k)
            }
        }
        stopCurrentMode();
        sikSetSpeed(speedIdx);
        sikStart();
        menuSetState(STATE_SIK_ACTIVE);
        break;
    }

    case 'l': { // mLRS — optional digit selects mode
        delay(50);
        uint8_t modeIdx = 0;  // default: 19 Hz LoRa
        if (Serial.available()) {
            char c = Serial.peek();
            if (c >= '1' && c <= '3') {
                Serial.read();
                modeIdx = c - '1';  // '1'→0(19Hz), '2'→1(31Hz), '3'→2(50Hz)
            }
        }
        stopCurrentMode();
        mlrsSetMode(modeIdx);
        mlrsStart();
        menuSetState(STATE_MLRS_ACTIVE);
        break;
    }

    case 'g': { // Crossfire — g/g9=915 FSK, g8=868 FSK, gl/gl9=915 LoRa, gl8=868 LoRa
        delay(80);
        bool loraMode = false;
        uint8_t bandIdx = CRSF_BAND_915;  // default

        // Check for 'l' (LoRa flag)
        if (Serial.available() && Serial.peek() == 'l') {
            Serial.read();
            loraMode = true;
        }
        delay(20);
        // Check for band digit
        if (Serial.available()) {
            char c = Serial.peek();
            if (c == '8')      { Serial.read(); bandIdx = CRSF_BAND_868; }
            else if (c == '9') { Serial.read(); bandIdx = CRSF_BAND_915; }
        }

        stopCurrentMode();
        crossfireSetBand(bandIdx);
        if (loraMode) {
            crossfireStartLoRa();
        } else {
            crossfireStart();
        }
        // Crossfire start functions set _crsfRunning internally; gate state
        // on that flag via crossfireGetParams().running check.
        if (crossfireGetParams().running) menuSetState(STATE_CROSSFIRE_ACTIVE);
        break;
    }

    case 'i':   // LoRaWAN US915 standalone false positive
        stopCurrentMode();
        fpStart(FP_LORAWAN);
        menuSetState(STATE_FP_ACTIVE);
        Serial.printf("[MODE] LoRaWAN US915 FP: SB2 channels, %d dBm\n", rfGetPower());
        break;

    case 't':   // Power Ramp (drone approach simulation)
        stopCurrentMode();
        powerRampStart();
        menuSetState(STATE_RAMP_ACTIVE);
        break;

    case 'f': { // Infrastructure false positive modes — f1/f2/f3
        delay(50);
        if (Serial.available()) {
            char c = Serial.read();
            stopCurrentMode();
            switch (c) {
            case '1': infraStart(INFRA_MESHTASTIC); break;
            case '2': infraStart(INFRA_HELIUM_POC); break;
            case '3': infraStart(INFRA_LORAWAN_EU); break;
            default:
                Serial.println("f1=Meshtastic f2=Helium f3=LoRaWAN-EU868");
                break;
            }
            if (c >= '1' && c <= '3') menuSetState(STATE_INFRA_ACTIVE);
        } else {
            Serial.println("f1=Meshtastic f2=Helium f3=LoRaWAN-EU868");
        }
        break;
    }

    case 'u': { // Custom LoRa — u=start, u?=show, uXvalue=configure
        delay(80);
        if (!Serial.available()) {
            // Bare 'u' — start transmission
            stopCurrentMode();
            customLoraStart();
            menuSetState(STATE_CUSTOM_LORA_ACTIVE);
        } else {
            // Read subcommand + value into buffer
            char buf[16];
            uint8_t len = 0;
            unsigned long deadline = millis() + 100;
            while (len < sizeof(buf) - 1 && millis() < deadline) {
                if (Serial.available()) {
                    char c = Serial.read();
                    if (c == '\n' || c == '\r') break;
                    buf[len++] = c;
                    deadline = millis() + 50;  // extend for more chars
                }
            }
            buf[len] = '\0';

            if (buf[0] == '?') {
                customLoraPrintConfig();
            } else if (len > 0) {
                customLoraConfigure(buf);
            }
        }
        break;
    }

    case 'b':   // Band Sweep
        stopCurrentMode();
        sweepStart();
        menuSetState(STATE_SWEEP_ACTIVE);
        { SweepParams sw = sweepGetParams();
          Serial.printf("[MODE] Band Sweep: %.1f-%.1f MHz, step %.0f kHz, %d dBm\n",
                        sw.startMHz, sw.endMHz, sw.stepMHz * 1000.0f, sw.powerDbm); }
        break;

    case 'r':   // Remote ID Spoofer
        stopCurrentMode();
        ridStart();
        menuSetState(STATE_RID_ACTIVE);
        Serial.println("[MODE] RID Spoofer: WiFi+BLE beacons");
        break;

    case 'm':   // Mixed False Positive (LoRaWAN + ELRS)
        stopCurrentMode();
        fpStart(FP_MIXED);
        menuSetState(STATE_FP_ACTIVE);
        Serial.printf("[MODE] Mixed FP (LoRaWAN+ELRS): %d dBm\n", rfGetPower());
        break;

    case 'x': {
        // Peek a follow-on char to decide:
        //   'x' alone          -> Combined RID + ELRS (legacy behavior)
        //   'x1'..'x4'         -> ELRS 2.4 GHz rate 500/250/150/50 Hz
        //   'x5'..'x8'         -> Ghost / FrSky / FlySky / DJI-energy
        //   'x9'               -> Generic 2.4 GHz (single-line args)
        //   'xq'               -> Stop XR1 only
        delay(80);
        if (!Serial.available()) {
            stopCurrentMode();
            combinedStart();
            menuSetState(STATE_COMBINED_ACTIVE);
            Serial.println("[MODE] Combined: RID(Core0) + ELRS(Core1)");
            break;
        }
        char sub = Serial.read();

        if (sub >= '1' && sub <= '4') {
            stopCurrentMode();
            if (xr1ModeElrs2g4Start(sub - '1')) {
                menuSetState(STATE_XR1_ACTIVE);
            } else {
                Serial.println("[XR1-MODE] start failed");
            }
        } else if (sub == '5') {
            stopCurrentMode();
            if (xr1ModeGhostStart())        menuSetState(STATE_XR1_ACTIVE);
        } else if (sub == '6') {
            stopCurrentMode();
            if (xr1ModeFrskyStart())        menuSetState(STATE_XR1_ACTIVE);
        } else if (sub == '7') {
            stopCurrentMode();
            if (xr1ModeFlyskyStart())       menuSetState(STATE_XR1_ACTIVE);
        } else if (sub == '8') {
            stopCurrentMode();
            if (xr1ModeDjiEnergyStart())    menuSetState(STATE_XR1_ACTIVE);
        } else if (sub == '9') {
            // x9 <L|F> <sf_or_br> <bw_or_dev> <cr> <count> <startMHz> <spacingMHz> <dwellMs> <pwrDbm>
            // Read the rest of the line (timeout 200 ms between chars).
            char argbuf[96];
            size_t n = 0;
            uint32_t deadline = millis() + 300;
            while (millis() < deadline && n + 1 < sizeof(argbuf)) {
                if (Serial.available()) {
                    char c = Serial.read();
                    if (c == '\n' || c == '\r') break;
                    argbuf[n++] = c;
                    deadline = millis() + 200;
                }
            }
            argbuf[n] = '\0';

            Xr1GenericCfg cfg = {};
            char modLetter = 0;
            unsigned sfInt = 0, crInt = 0, chInt = 0, dwInt = 0;
            int pwrInt = 0;
            float bwF = 0.0f, brF = 0.0f, devF = 0.0f, startMhz = 0.0f, spacing = 0.0f;
            int parsed = sscanf(argbuf, " %c %u %f %u %u %f %f %u %d",
                                &modLetter, &sfInt, &bwF, &crInt, &chInt,
                                &startMhz, &spacing, &dwInt, &pwrInt);
            if (parsed >= 9 && (modLetter == 'L' || modLetter == 'l')) {
                cfg.isLora = true;
                cfg.sf = (uint8_t)sfInt; cfg.bwKhz = bwF; cfg.cr = (uint8_t)crInt;
                cfg.channelCount = (uint8_t)chInt; cfg.startMhz = startMhz;
                cfg.spacingMhz = spacing; cfg.dwellMs = (uint16_t)dwInt;
                cfg.powerDbm = (int8_t)pwrInt;
                stopCurrentMode();
                if (xr1ModeGenericStart(cfg)) menuSetState(STATE_XR1_ACTIVE);
            } else if (parsed >= 9 && (modLetter == 'F' || modLetter == 'f')) {
                // GFSK form: %c %brkbps %devkhz (cr) %count %start %spacing %dwell %pwr
                cfg.isLora = false;
                cfg.brKbps = (float)sfInt;  // first numeric after F is bitrate kbps
                cfg.devKhz = bwF;           // second is deviation kHz
                cfg.channelCount = (uint8_t)chInt;
                cfg.startMhz = startMhz; cfg.spacingMhz = spacing;
                cfg.dwellMs = (uint16_t)dwInt; cfg.powerDbm = (int8_t)pwrInt;
                stopCurrentMode();
                if (xr1ModeGenericStart(cfg)) menuSetState(STATE_XR1_ACTIVE);
            } else {
                Serial.println("[XR1-MODE] x9 usage: x9 L <sf> <bw> <cr> <count> <startMHz> <spacingMHz> <dwellMs> <pwr>");
                Serial.println("                or: x9 F <brKbps> <devKhz> 0 <count> <startMHz> <spacingMHz> <dwellMs> <pwr>");
            }
        } else if (sub == 'q' || sub == 'Q') {
            xr1ModesStop();
            if (menuGetState() == STATE_XR1_ACTIVE) menuSetState(STATE_MAIN_MENU);
            Serial.println("[XR1-MODE] stopped (xq)");
        } else {
            Serial.printf("Unknown x-subcommand: '%c'. Type 'h' for help.\n", sub);
        }
        break;
    }

    case 'w':   // Drone Swarm Simulator
        stopCurrentMode();
        swarmStart();
        menuSetState(STATE_SWARM_ACTIVE);
        { SwarmParams sp = swarmGetParams();
          Serial.printf("[MODE] Swarm: %d drones, WiFi RID beacons\n", sp.droneCount); }
        break;

    case 'n':   // Cycle swarm drone count
        swarmCycleCount();
        break;

    case 'h':
    case '?':
        printHelp();
        break;

    default:
        if (cmd > ' ') {
            Serial.printf("Unknown command: '%c'. Type 'h' for help.\n", cmd);
        }
        break;
    }
}

void loop() {
    // State machine drives everything — menu, display, and mode execution
    menuUpdate();

    // Process serial commands for runtime parameter control
    handleSerialCommands();

    // Heartbeat blink: slow when idle, fast when transmitting
    static unsigned long lastBlink = 0;
    static bool ledState = false;
    AppState st = menuGetState();
    bool txActive = (st == STATE_CW_ACTIVE || st == STATE_SWEEP_ACTIVE
                     || st == STATE_ELRS_ACTIVE || st == STATE_FP_ACTIVE
                     || st == STATE_RID_ACTIVE || st == STATE_COMBINED_ACTIVE
                     || st == STATE_SWARM_ACTIVE
                     || st == STATE_CROSSFIRE_ACTIVE
                     || st == STATE_RAMP_ACTIVE
                     || st == STATE_SIK_ACTIVE
                     || st == STATE_MLRS_ACTIVE
                     || st == STATE_CUSTOM_LORA_ACTIVE
                     || st == STATE_INFRA_ACTIVE
                     || st == STATE_XR1_ACTIVE);
    unsigned long blinkRate = txActive ? 200 : 1000;

    if (millis() - lastBlink >= blinkRate) {
        lastBlink = millis();
        ledState = !ledState;
        digitalWrite(LED_PIN, ledState ? HIGH : LOW);
    }

    // yield() feeds the watchdog with minimal overhead (~1ms).
    // TX modes are rate-limited by SX1262 airtime (~6.7ms/hop at SF6/BW500),
    // so the 1ms yield cost is negligible vs the 6.7ms TX blocking.
    yield();
}
