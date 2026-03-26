#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RadioLib.h>
#include "board_config.h"
#include "version.h"
#include "menu.h"
#include "rf_modes.h"

// --- OLED Display ---
Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);

// --- LoRa Radio ---
SPIClass loraSPI(HSPI);
SX1262 radio = new Module(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY, loraSPI);

// --- Boot Screen (shown for 2 seconds) ---
static void showBootScreen() {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(10, 0);
    display.println("JUH-MAK-IN JAMMER");
    display.setCursor(40, 12);
    display.printf("v%s", JAMMER_VERSION);
    display.setCursor(0, 28);
    display.println("RF Test Tool");
    display.setCursor(0, 40);
    display.printf("Board: %s", BOARD_NAME);
    display.setCursor(0, 56);
    display.println("Initializing...");
    display.display();
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
    int state = radio.begin(915.0);
    if (state == RADIOLIB_ERR_NONE) {
        Serial.println("OK");
    } else {
        Serial.printf("FAILED (error %d)\n", state);
    }

    // --- Initialize subsystems ---
    cwInit(&radio);
    menuInit(&display);

    // Hold boot screen for 2 seconds so user can read it
    delay(2000);

    // --- Ready ---
    Serial.println();
    Serial.println("JAMMER-RF ready.");
    Serial.println("Press BOOT button to navigate.");
    Serial.println("  SHORT press = cycle menu");
    Serial.println("  LONG  press = select/confirm");
    Serial.println();
    Serial.println("Serial commands (active during TX):");
    Serial.println("  d = cycle dwell time (sweep)");
    Serial.println("  s = cycle step size (sweep)");
    Serial.println("  p = cycle TX power (all modes)");
    Serial.println("  q = stop TX and return to menu");

    digitalWrite(LED_PIN, LOW);
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
        if (st == STATE_CW_ACTIVE)    cwStop();
        if (st == STATE_SWEEP_ACTIVE)  sweepStop();
        if (st == STATE_ELRS_ACTIVE)   elrsStop();
        Serial.println("TX stopped via serial.");
        break;

    default:
        // Ignore newlines, carriage returns, and unknown chars
        if (cmd > ' ') {
            Serial.printf("Unknown command: '%c'. Use d/s/p/q.\n", cmd);
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
                     || st == STATE_ELRS_ACTIVE);
    unsigned long blinkRate = txActive ? 200 : 1000;

    if (millis() - lastBlink >= blinkRate) {
        lastBlink = millis();
        ledState = !ledState;
        digitalWrite(LED_PIN, ledState ? HIGH : LOW);
    }

    delay(10);
}
