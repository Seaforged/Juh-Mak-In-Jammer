# JJ Project: RadioMaster XR1 Integration Research Document
## Dual-Band Signal Emulation via ESP32C3 + Semtech LR1121

**Document Version:** 1.0  
**Date:** April 11, 2026  
**Authors:** ND (Seaforged), Claude (Technical Lead)  
**Project:** Juh-Mak-In Jammer (JJ) — Counter-UAS Test Signal Emulator  
**Status:** Research Complete — Awaiting Sprint Development  

---

## 1. Executive Summary

This document captures the complete research findings for integrating a RadioMaster XR1 Nano Multi-Frequency ExpressLRS Receiver into the Juh-Mak-In Jammer (JJ) project. The XR1 contains an ESP32C3 microcontroller and a Semtech LR1121 dual-band transceiver, providing JJ with 2.4 GHz transmission capability that currently does not exist in the test suite.

**Key Finding:** By flashing custom firmware (RadioLib-based) onto the XR1's ESP32C3, the combined T3S3 + XR1 system can emulate the RF footprint of approximately 70–80% of real-world drone control protocols, covering ELRS, TBS Crossfire, FrSky R9, ImmersionRC Ghost, MAVLink telemetry, and generic FHSS patterns across both sub-GHz (150–960 MHz) and 2.4 GHz ISM bands.

**Hardware Cost:** $17.99 (RadioMaster XR1 Dual-Band Antenna edition)

---

## 2. Hardware Specifications

### 2.1 RadioMaster XR1 Nano Receiver

| Parameter | Value | Source |
|-----------|-------|--------|
| MCU | ESP32C3 (RISC-V, 160 MHz, 4 MB flash) | Amazon product listing |
| RF Chip | Semtech LR1121 (3rd generation) | Amazon product listing |
| RF Connector | IPEX-1 (u.FL) | Amazon product listing |
| Antenna | 1x T-Antenna, Dual-Band (2.4 GHz + 900 MHz) | Amazon product listing |
| Frequency Range | 2.4 GHz ISM + Sub-G 900 MHz | Amazon product listing |
| Telemetry Power | 100 mW | Amazon product listing |
| Max Packet Rate | DK500 Hz / K1000 Hz | Amazon product listing |
| Working Voltage | 5V | Amazon product listing |
| Weight | 1.0 g (without antenna) | Amazon product listing |
| Dimensions | 20 mm × 13 mm × 3 mm | Amazon product listing |
| Bus Interface 1 | CRSF (primary serial) | Amazon product listing |
| Bus Interface 2 | UART (secondary serial) | Amazon product listing |
| Pre-installed FW | ExpressLRS v3.5.1 | Amazon product listing |
| FW Target | RadioMaster XR1 2.4/900 RX | Amazon product listing |

### 2.2 Semtech LR1121 RF Capabilities

| Parameter | Value | Source |
|-----------|-------|--------|
| Sub-GHz Range | 150–960 MHz | Semtech LR1121 Datasheet Rev 2.0 |
| 2.4 GHz Range | 2.4–2.5 GHz ISM | Semtech LR1121 Datasheet Rev 2.0 |
| S-Band Range | 1.9–2.1 GHz | Semtech LR1121 Datasheet Rev 2.0 |
| Sub-GHz TX Power (HP) | +22 dBm max | Semtech LR1121 Datasheet Rev 2.0 |
| Sub-GHz TX Power (LP) | +15 dBm max | Semtech LR1121 Datasheet Rev 2.0 |
| 2.4 GHz TX Power | +13 dBm max | Semtech LR1121 Datasheet Rev 2.0 |
| Modulation: LoRa | Sub-GHz, S-Band, 2.4 GHz | Semtech LR1121 Datasheet Rev 2.0 |
| Modulation: (G)FSK | Sub-GHz, 2.4 GHz | Semtech LR1121 Datasheet Rev 2.0 |
| Modulation: LR-FHSS | Sub-GHz, S-Band, 2.4 GHz (TX only) | Semtech LR1121 Datasheet Rev 2.0 |
| Modulation: Sigfox | Sub-GHz only | Semtech LR1121 Datasheet Rev 2.0 |
| SPI Interface | CPOL=0, CPHA=0 (Mode 0), slave only | Semtech LR1121 User Manual v1.1 |
| DC-DC Converter | Integrated, software selectable | Semtech LR1121 Datasheet Rev 2.0 |
| Flash Memory | Internal, stores firmware + config + keys | Semtech LR1121 Datasheet Rev 2.0 |
| Frequency Synthesizer | Shared between sub-GHz and HF bands | NiceRF LoRa1121 documentation |

**Critical Limitation:** The frequency synthesizer is shared between bands. The LR1121 can operate on sub-GHz OR 2.4 GHz at any given moment, but NOT simultaneously on both. Band switching is a software operation.

### 2.3 ESP32C3 MCU Capabilities

| Parameter | Value |
|-----------|-------|
| Architecture | RISC-V single-core, 160 MHz |
| Flash | 4 MB |
| SRAM | 400 KB |
| WiFi | 802.11 b/g/n 2.4 GHz |
| Bluetooth | BLE 5.0 |
| SPI | 1x GP-SPI (SPI2) |
| UART | 2x UART |
| GPIO | 22 total (limited by package) |
| USB | USB Serial/JTAG |
| PlatformIO Board | esp32-c3-devkitm-1 |

**Hidden Bonus:** The ESP32C3's built-in WiFi radio provides an independent 2.4 GHz transmitter separate from the LR1121. This can emit WiFi beacon frames mimicking WiFi-based drone control (DJI Spark, Parrot, toy drones) without consuming LR1121 resources.

---

## 3. Pin Mapping

### 3.1 XR1 External Pads (User-Accessible)

The XR1 exposes 4 castled pads plus a secondary UART port:

| Pad Label | Function | Notes |
|-----------|----------|-------|
| 5V | Power input | 5V DC, powers both ESP32C3 and LR1121 |
| G | Ground | Common ground |
| TX | CRSF/Serial TX | ESP32C3 UART output → host MCU RX |
| RX | CRSF/Serial RX | ESP32C3 UART input ← host MCU TX |

Secondary UART pads (RX/TX/CTS) are also exposed on the PCB for additional serial connectivity.

### 3.2 ESP32C3 ↔ LR1121 Internal SPI Pin Mapping

Based on the ExpressLRS Generic C3 LR1121 target definition (source: ExpressLRS/targets repository, `RX/Generic C3 LR1121.json`):

| ELRS Function | ESP32C3 GPIO | LR1121 Pin | Description |
|---------------|-------------|------------|-------------|
| radio_sck | GPIO 6 | SCK | SPI clock |
| radio_mosi | GPIO 4 | MOSI | SPI data out (MCU → radio) |
| radio_miso | GPIO 5 | MISO | SPI data in (radio → MCU) |
| radio_nss | GPIO 7 | NSS | Chip select (active LOW) |
| radio_rst | GPIO 2 | NRESET | Radio hardware reset |
| radio_busy | GPIO 3 | BUSY | Radio busy flag |
| radio_dio1 | GPIO 1 | DIO9 | Interrupt line |
| led_rgb | GPIO 8 | — | WS2812B RGB status LED |
| button | GPIO 9 | — | Bind/boot button |
| serial_rx | GPIO 20 | — | UART RX (CRSF input from host) |
| serial_tx | GPIO 21 | — | UART TX (CRSF output to host) |

**IMPORTANT NOTE:** The XR1 may use a manufacturer-specific overlay that differs slightly from the Generic C3 LR1121 target. The exact pin mapping should be verified by either:
1. Reading the hardware.json from the XR1's web UI (connect via WiFi AP after holding bind button for 5–7 seconds)
2. Inspecting the ELRS targets.json entry for "RadioMaster XR1 2.4/900 RX"
3. Physical probing with a multimeter in continuity mode

**Verification Sprint Required:** Before writing custom firmware, a "Hello Hardware" sprint must confirm these pins by initializing RadioLib and successfully calling `radio.getVersion()` on the LR1121.

### 3.3 T3S3 ↔ XR1 Wiring (UART Connection)

| XR1 Pad | T3S3 GPIO | T3S3 Function | Wire Color (suggested) |
|---------|-----------|---------------|----------------------|
| 5V | 5V (USB VBUS) | Power | Red |
| G | GND | Ground | Black |
| TX | GPIO 44 | UART1 RX | Yellow |
| RX | GPIO 43 | UART1 TX | Green |

**Baud Rate:** 420000 for CRSF protocol (stock ELRS), or custom rate for RadioLib firmware.

**Conflict Note:** GPIO 43/44 on the T3S3 are also used for GPS in SENTRY-RF. Since JJ does not use GPS, these pins are available. If future JJ builds require GPS, the secondary UART pads on the XR1 could be used instead, or a software UART could be implemented.

---

## 4. Drone Protocol RF Analysis

### 4.1 Protocol-to-Modulation Mapping

| Protocol | Frequency Band | Modulation Type | LR1121 Support | Emulation Feasibility |
|----------|---------------|-----------------|----------------|----------------------|
| ExpressLRS 900 | 868/915 MHz | LoRa + FSK | YES — native | TIER 1: Perfect, stock ELRS firmware |
| ExpressLRS 2.4 | 2.4 GHz ISM | LoRa + FSK + FLRC | YES — native | TIER 1: Perfect, stock ELRS firmware |
| TBS Crossfire | 868/915 MHz | LoRa (50Hz) + FSK (150Hz) | YES — native | TIER 2: RF footprint match, custom packets |
| ImmersionRC Ghost | 2.4 GHz | LoRa | YES — native | TIER 2: RF footprint match |
| FrSky R9 | 868/915 MHz | LoRa | YES — native | TIER 2: RF footprint match |
| FrSky ACCST D16 | 2.4 GHz | GFSK FHSS (~50 channels) | YES — GFSK supported | TIER 3: Feasible, reverse-engineered protocol |
| FrSky ACCESS | 2.4 GHz | GFSK FHSS | YES — GFSK supported | TIER 3: Feasible, needs protocol RE |
| Futaba S-FHSS | 2.4 GHz | GFSK FHSS | YES — GFSK supported | TIER 3: Feasible, DIY Multi-Module has this |
| FlySky AFHDS 2A | 2.4 GHz | GFSK FHSS | YES — GFSK supported | TIER 3: Feasible, DIY Multi-Module has this |
| MAVLink 433/915 | 433/915 MHz | FSK narrowband | YES — native | TIER 2: Straightforward |
| DJI OcuSync (control) | 2.4 GHz | FHSS narrowband bursts | PARTIAL | TIER 3: Energy approximation only |
| DJI OcuSync (video) | 2.4/5.8 GHz | OFDM wideband | NO — impossible | TIER 4: Wrong modulation type |
| DJI WiFi drones | 2.4 GHz | 802.11 WiFi | YES — via ESP32C3 WiFi | TIER 2: ESP32C3 WiFi radio, not LR1121 |
| Spektrum DSM2/DSMX | 2.4 GHz | DSSS/FHSS | NO — no DSSS | TIER 4: Wrong modulation type |
| Analog FPV video | 5.8 GHz | FM/AM | NO — wrong band | TIER 4: LR1121 max is 2.5 GHz |

### 4.2 Protocol Detail: TBS Crossfire

- **Source:** NoirFPV Crossfire FAQ; Oscar Liang ExpressLRS vs Crossfire comparison
- **RF Layer:** LoRa modulation at 50 Hz packet rate, FSK at 150 Hz
- **Frequency:** 850–870 MHz (EU) / 902–927 MHz (US), full ISM band FHSS
- **Power:** Up to 2W (full TX), 250 mW (Micro TX)
- **Protocol:** CRSF serial between module and radio, proprietary OTA packets
- **Emulation approach:** Use RadioLib to configure LR1121 for LoRa at matching SF/BW/CR, hop across same frequency set at matching timing. Packet payload is not protocol-correct but RF footprint (RSSI, CAD, frequency pattern) will match what a detector sees.

### 4.3 Protocol Detail: FrSky ACCST D16

- **Source:** DIY-Multiprotocol-TX-Module GitHub; FrSky protocol documentation
- **RF Layer:** GFSK modulation, ~50 channels across 2.400–2.483 GHz
- **Hop Pattern:** Pseudo-random, bound to TX ID
- **Packet Rate:** 18 ms (D16 16ch), 9 ms (D16 8ch)
- **Protocol:** Fully reverse-engineered by DIY Multi-Module project (CC2500-based)
- **Emulation approach:** The LR1121 supports GFSK at 2.4 GHz. Configure matching bitrate, deviation, and channel spacing. Hop across ~50 channels at 18 ms intervals. The DIY Multi-Module source code (github.com/pascallanger/DIY-Multiprotocol-TX-Module) provides the complete packet format, hopping sequence generation, and binding protocol for FrSky D8, D16 v1, and D16 v2.

### 4.4 Protocol Detail: DJI OcuSync

- **Source:** DJI Mavic Pilots forum; DeepSig commercial drone signals analysis; IEEE paper on DJI signal identification
- **RF Layer:** OFDM (video downlink) + FHSS (control uplink)
- **Frequency:** 2.4 GHz and 5.8 GHz dual-band
- **Video modulation:** OFDM with QAM (32-QAM or 64-QAM), up to 40 Mb/s
- **Control modulation:** FHSS narrowband bursts, FSK-like
- **Channel widths:** 20 MHz, 10 MHz, or 1.4 MHz selectable
- **Why OFDM is impossible:** OFDM requires simultaneous multi-carrier transmission (multiple frequencies transmitted in parallel). The LR1121 is a single-carrier radio — it can only produce one frequency at a time. This is a fundamental hardware limitation, not a firmware limitation.
- **What IS possible:** The FHSS control uplink uses narrowband bursts that produce detectable RF energy at specific frequencies. The LR1121 could generate FSK bursts hopping across 2.4 GHz channels to approximate this energy pattern. A SENTRY-RF energy detector would see "something hopping at 2.4 GHz" — useful for testing, even though it's not OcuSync-correct.

---

## 5. Firmware Architecture Options

### 5.1 Option A: Stock ELRS Firmware (CRSF Passthrough)

**Difficulty:** Easy  
**Fidelity:** Perfect for ELRS signals  

The T3S3 acts as a mock radio handset, generating CRSF channel packets at 420000 baud and sending them to the XR1 over UART. The XR1's ELRS firmware handles all RF transmission.

**CRSF Protocol Summary:**
- Baud: 420000 (ELRS default) or 921600 (high-speed)
- Frame format: `[SYNC] [LENGTH] [TYPE] [PAYLOAD...] [CRC8]`
- SYNC byte: 0xC8 (from handset to module)
- Key frame type: 0x16 = RC Channels Packed (11-bit channels × 16)
- CRC: CRC-8/DVB-S2 polynomial

**Implementation:** New JJ serial command `l` (link) starts sending CRSF channel frames at the configured packet rate. The XR1 transmits real ELRS packets that SENTRY-RF can detect.

**Limitations:** Only produces ELRS signals. Cannot emulate other protocols.

### 5.2 Option B: Custom RadioLib Firmware on XR1

**Difficulty:** Medium  
**Fidelity:** High for all LoRa/FSK protocols  

Flash the XR1's ESP32C3 with custom PlatformIO firmware using RadioLib. The T3S3 sends custom serial commands over UART specifying frequency, modulation, power, and hopping parameters. The XR1 executes these as raw RF transmissions.

**PlatformIO Configuration for XR1:**
```ini
[env:xr1_custom]
platform = espressif32
board = esp32-c3-devkitm-1
board_build.mcu = esp32c3
framework = arduino
monitor_speed = 115200
lib_deps =
    jgromes/RadioLib@^7.0.0
build_flags =
    -DARDUINO_USB_MODE=1
    -DARDUINO_USB_CDC_ON_BOOT=1
```

**RadioLib LR1121 Initialization (based on Generic C3 LR1121 pins):**
```cpp
#include <RadioLib.h>

// XR1 ESP32C3 → LR1121 SPI pins (from ELRS Generic C3 LR1121 target)
#define LR1121_SCK   6
#define LR1121_MOSI  4
#define LR1121_MISO  5
#define LR1121_NSS   7   // Chip select
#define LR1121_RST   2   // Reset
#define LR1121_BUSY  3   // Busy flag
#define LR1121_DIO9  1   // Interrupt (DIO9 on LR1121)

SPIClass spi(SPI);
LR1121 radio = new Module(LR1121_NSS, LR1121_DIO9, LR1121_RST, LR1121_BUSY, spi);

void setup() {
    Serial.begin(115200);
    spi.begin(LR1121_SCK, LR1121_MISO, LR1121_MOSI, LR1121_NSS);
    
    // Initialize at 915 MHz, 125 kHz BW, SF7, CR 4/5
    int state = radio.begin(915.0, 125.0, 7, 5);
    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("LR1121 init failed: %d\n", state);
        while(true) delay(100);
    }
    
    // Enable DC-DC converter for efficiency
    radio.setRegulatorDCDC();
    
    // Set RF switch configuration if needed
    // (XR1 may require specific DIO5/DIO6 RF switch config)
    
    Serial.println("LR1121 ready");
}
```

**Band Switching Example:**
```cpp
// Switch to 2.4 GHz LoRa
radio.setFrequency(2440.0);  // 2440 MHz
radio.setSpreadingFactor(7);
radio.setBandwidth(812.5);   // Wide BW for 2.4 GHz
radio.transmit("test");

// Switch back to 915 MHz
radio.setFrequency(915.0);
radio.setSpreadingFactor(7);
radio.setBandwidth(125.0);
radio.transmit("test");
```

**FHSS Hopping Example (Crossfire-like):**
```cpp
// Crossfire-like hopping across 902–927 MHz
const float channels[] = {903.0, 905.5, 908.0, 910.5, 913.0, 
                          915.5, 918.0, 920.5, 923.0, 925.5};
const int numChannels = 10;
int hopIndex = 0;

void hop() {
    radio.setFrequency(channels[hopIndex]);
    radio.transmit(payload, payloadLen);
    hopIndex = (hopIndex + 1) % numChannels;
}
// Call hop() every 20 ms for Crossfire-like timing
```

### 5.3 Option C: Hybrid (ELRS + Custom Command Protocol)

**Difficulty:** Hard  
**Fidelity:** Maximum  

Modify the ELRS firmware to add custom serial commands alongside normal CRSF operation. This would allow runtime switching between authentic ELRS transmission and arbitrary signal generation without reflashing.

**Not recommended for initial implementation** due to complexity of maintaining a fork of ELRS firmware. Use Option A or B depending on the test scenario.

---

## 6. RF Switch Configuration

The LR1121 has three antenna ports: SMA (sub-GHz), IPX III (2.4 GHz), and IPX (WiFi). The RF switch that routes the signal to the correct antenna port is controlled by DIO5 and DIO6.

From the ELRS codebase and LR1121 datasheet, the RF switch truth table is:

| Mode | DIO5 | DIO6 | Active Path |
|------|------|------|-------------|
| Sub-GHz TX (HP) | LOW | HIGH | SMA (sub-GHz) |
| Sub-GHz TX (LP) | LOW | HIGH | SMA (sub-GHz) |
| Sub-GHz RX | HIGH | LOW | SMA (sub-GHz) |
| 2.4 GHz TX | HIGH | LOW | IPEX (2.4 GHz) |
| 2.4 GHz RX | LOW | HIGH | IPEX (2.4 GHz) |
| Off | LOW | LOW | None |

**RadioLib RF Switch Table Configuration:**
```cpp
// ELRS-style RF switch configuration for LR1121
// Format: [DIO5, DIO6, mode1, mode2, mode3, mode4, mode5, mode6]
// From ELRS power_values and radio_rfsw_ctrl
static const uint32_t rfswitch_dio_pins[] = {
    RADIOLIB_LR11X0_DIO5, RADIOLIB_LR11X0_DIO6, RADIOLIB_NC, RADIOLIB_NC, RADIOLIB_NC
};

static const Module::RfSwitchMode_t rfswitch_table[] = {
    {Module::MODE_IDLE, {LOW, LOW}},
    {Module::MODE_RX,   {HIGH, LOW}},
    {Module::MODE_TX,   {LOW, HIGH}},
    END_OF_MODE_TABLE
};

// Apply in setup:
radio.setRfSwitchTable(rfswitch_dio_pins, rfswitch_table);
```

**WARNING:** The exact RF switch configuration on the XR1 may differ from the generic LR1121 reference design. This MUST be verified during the Hello Hardware sprint. Incorrect RF switch config will result in no RF output even if SPI communication succeeds.

---

## 7. Power Configuration

### 7.1 LR1121 Power Levels (from ELRS Generic C3 LR1121 target)

**2.4 GHz Band (power_values):**

| Index | Register Value | Approximate dBm | Approximate mW |
|-------|---------------|-----------------|-----------------|
| 0 | 12 | ~12 dBm | ~16 mW |
| 1 | 16 | ~16 dBm | ~40 mW |
| 2 | 19 | ~19 dBm | ~80 mW |
| 3 | 22 | ~22 dBm | ~160 mW |

**900 MHz Band (power_values_dual):**

| Index | Register Value | Approximate dBm | Notes |
|-------|---------------|-----------------|-------|
| 0 | -12 | ~-12 dBm | Very low power |
| 1 | -9 | ~-9 dBm | Low power |
| 2 | -6 | ~-6 dBm | Medium-low |
| 3 | -2 | ~-2 dBm | Medium |

**Note:** The 900 MHz power values for the C3 variant are significantly lower than the ESP32 variant (-12 to -2 vs -10 to +1). This is likely because the XR1 is a receiver with limited PA capability at sub-GHz. The 100 mW telemetry spec applies to the 2.4 GHz band.

**LNA Gain:** power_lna_gain = 12 (Low Noise Amplifier configuration for receive sensitivity)

---

## 8. Three-Emitter Architecture

The combined T3S3 + XR1 system provides THREE independent RF emitters:

| Emitter | Chip | Frequency Range | Modulation | Control |
|---------|------|----------------|------------|---------|
| T3S3 onboard | SX1262 | 150–960 MHz | LoRa, (G)FSK | Direct SPI from ESP32-S3 |
| XR1 LR1121 | LR1121 | 150–960 MHz + 2.4 GHz | LoRa, (G)FSK, LR-FHSS | SPI from ESP32C3 (UART-commanded by T3S3) |
| XR1 WiFi | ESP32C3 internal | 2.4 GHz (WiFi channels) | 802.11 b/g/n | Software on ESP32C3 |

**Simultaneous Operation:**
- The T3S3 SX1262 and XR1 LR1121 can transmit simultaneously on different frequencies (e.g., SX1262 at 915 MHz while LR1121 at 2.4 GHz)
- The XR1's LR1121 and WiFi radio can potentially operate concurrently (WiFi on fixed channel, LR1121 hopping) — needs testing
- This enables emulating paired dual-band detection scenarios (simultaneous 900 MHz + 2.4 GHz), which is the strongest drone indicator per D-TECT-R research

---

## 9. Development Roadmap

### Sprint XR1-1: Hello Hardware (Estimated: 2–3 hours)
**Goal:** Verify ESP32C3 ↔ LR1121 SPI communication and confirm pin mapping  
**Prerequisites:** USB-to-Serial adapter (CP2102 or FTDI), soldering to XR1 pads  
**Deliverables:**
- [ ] Flash basic RadioLib sketch to ESP32C3 via UART bootloader
- [ ] Successfully call `radio.getVersion()` and confirm LR1121 responds
- [ ] Confirm exact pin mapping matches Generic C3 LR1121 target
- [ ] Read hardware.json from XR1 web UI for reference
- [ ] Document any pin differences from generic target

**CRITICAL:** Back up the ELRS firmware before flashing custom code. The XR1 can be reflashed to ELRS via WiFi or UART at any time.

### Sprint XR1-2: Basic UART Command Protocol (Estimated: 4–6 hours)
**Goal:** T3S3 sends serial commands → XR1 transmits on specified frequency  
**Deliverables:**
- [ ] Define simple serial command protocol between T3S3 and XR1
- [ ] Implement LoRa transmission at arbitrary frequency/SF/BW
- [ ] Implement GFSK transmission at arbitrary frequency/bitrate/deviation
- [ ] Verify SENTRY-RF detects XR1 transmissions

### Sprint XR1-3: FHSS Pattern Engine (Estimated: 6–8 hours)
**Goal:** XR1 autonomously hops across frequency channels at specified timing  
**Deliverables:**
- [ ] Implement configurable hopping pattern (channel list + dwell time)
- [ ] Pre-built patterns: ELRS 900, ELRS 2.4, Crossfire 868/915, generic 2.4 GHz
- [ ] T3S3 can start/stop/configure hopping via serial commands

### Sprint XR1-4: WiFi Drone Emulation (Estimated: 4–6 hours)
**Goal:** ESP32C3 WiFi radio emits beacon frames mimicking WiFi-based drones  
**Deliverables:**
- [ ] Emit WiFi beacon frames with configurable SSID (e.g., "DJI-Spark-XXXX")
- [ ] Emit WiFi probe requests at configurable intervals
- [ ] Verify SENTRY-RF WiFi scanning detects these signals

### Sprint XR1-5: Dual-Band Simultaneous Emission (Estimated: 4–6 hours)
**Goal:** T3S3 SX1262 + XR1 LR1121 transmit simultaneously on different bands  
**Deliverables:**
- [ ] T3S3 emits 915 MHz signals while XR1 emits 2.4 GHz signals
- [ ] SENTRY-RF (LR1121 board) detects correlated dual-band activity
- [ ] Validate paired dual-band detection logic

---

## 10. Key References

### Hardware Datasheets
1. **Semtech LR1121 Datasheet Rev 2.0** — Full electrical specifications, pin functions, RF parameters. Available: https://files.waveshare.com/wiki/Core1121/LR1121_H2_DS_v2_0.pdf
2. **Semtech LR1121 User Manual v1.1** — SPI command reference, mode transitions, TX/RX operation. Available: https://www.mouser.com/pdfDocs/UserManual_LR1121_v1_1.pdf
3. **Semtech LR1121 Product Page** — Overview and downloads. Available: https://www.semtech.com/products/wireless-rf/lora-connect/lr1121

### Software Libraries
4. **RadioLib** — Universal wireless communication library, LR1121 class supported. GitHub: https://github.com/jgromes/RadioLib / API: https://jgromes.github.io/RadioLib/class_l_r1121.html
5. **ExpressLRS** — Open-source RC link firmware (XR1 stock firmware). GitHub: https://github.com/ExpressLRS/ExpressLRS
6. **ExpressLRS Targets** — Hardware target definitions including pin mappings. GitHub: https://github.com/ExpressLRS/targets
7. **DIY-Multiprotocol-TX-Module** — Reverse-engineered FrSky/Spektrum/FlySky protocols. GitHub: https://github.com/pascallanger/DIY-Multiprotocol-TX-Module
8. **Semtech LR1121 Firmware Images** — Official transceiver firmware binaries. GitHub: https://github.com/Lora-net/radio_firmware_images

### ELRS Pin Mapping Sources
9. **DeepWiki: Dual-band Receivers (LR1121)** — Compiled ELRS target pin tables for ESP32, ESP32-C3, ESP32-S3 with LR1121. URL: https://deepwiki.com/ExpressLRS/targets/4.2.3-dual-band-receivers-(lr1121)
10. **ELRS Generic C3 LR1121.json** — The base layout file for ESP32C3 + single LR1121 receivers. GitHub: https://github.com/ExpressLRS/targets/blob/master/RX/Generic%20C3%20LR1121.json

### Drone RF Protocol References
11. **DeepSig: Introduction to Commercial Drone Signals** — Overview of DJI, WiFi, and hobby drone RF signatures. URL: https://www.deepsig.ai/introduction-to-commercial-drone-signals/
12. **Oscar Liang: ExpressLRS vs Crossfire** — Comparison of LoRa/FSK usage in ELRS and Crossfire. URL: https://oscarliang.com/expresslrs/
13. **Oscar Liang: FPV Protocols Explained** — CRSF, SBUS, ACCST, ACCESS, DSM2, DSMX technical summary. URL: https://oscarliang.com/rc-protocols/
14. **TBS Crossfire Manual** — Official Crossfire documentation with RF band details. URL: https://ia600700.us.archive.org/22/items/tbs-crossfire-manual/tbs-crossfire-manual.pdf
15. **DJI OcuSync Analysis** — Forum discussion with OFDM/FHSS modulation details. URL: https://mavicpilots.com/threads/very-interesting-ocusync.109873/
16. **IEEE/ETR Paper: Identifying and Analyzing DJI Drone Signals** — Academic analysis of DJI Air 3 and Phantom 4 signal characteristics with spectrogram analysis. URL: https://journals.ru.lv/index.php/ETR/article/download/8486/6933/10816

### Product Information
17. **RadioMaster XR1 Product Page** — Official specifications and antenna options. URL: https://radiomasterrc.com/products/xr1-nano-multi-frequency-expresslrs-receiver
18. **Oscar Liang: RadioMaster XR1/XR2/XR3/XR4 Review** — Detailed comparison of XR series receivers. URL: https://oscarliang.com/radiomaster-xr1-xr2-xr3-xr4-elrs-receivers/

---

## 11. Legal and Regulatory Considerations

**This device is for COUNTER-UAS TESTING ONLY.**

- All transmissions must comply with FCC Part 15 ISM band regulations (915 MHz, 2.4 GHz)
- Maximum EIRP must not exceed FCC limits for the applicable band
- The LR1121's 100 mW (20 dBm) telemetry power on 2.4 GHz is within FCC ISM limits
- Sub-GHz power on the XR1 C3 variant is very low (-12 to -2 dBm), well within limits
- WiFi beacon emission should use minimal power and non-deceptive SSIDs
- This is a TEST TOOL — it does not interfere with or jam actual drone communications
- Emulated signals are for validating SENTRY-RF detection algorithms

---

## 12. Glossary

| Term | Definition |
|------|-----------|
| CRSF | Crossfire Serial Protocol — bidirectional serial protocol between radio handset and TX module |
| (G)FSK | (Gaussian) Frequency Shift Keying — digital modulation using frequency changes |
| LoRa | Long Range — Semtech proprietary CSS (Chirp Spread Spectrum) modulation |
| OFDM | Orthogonal Frequency Division Multiplexing — multi-carrier modulation (used by DJI) |
| DSSS | Direct Sequence Spread Spectrum — spreading technique (used by Spektrum) |
| FHSS | Frequency Hopping Spread Spectrum — rapidly changing transmission frequency |
| LR-FHSS | Long Range FHSS — Semtech's intra-packet hopping modulation |
| ISM | Industrial, Scientific, Medical — license-free radio frequency bands |
| CAD | Channel Activity Detection — LoRa feature to detect preamble presence |
| SF | Spreading Factor — LoRa parameter controlling range vs data rate (SF6–SF12) |
| BW | Bandwidth — signal bandwidth in kHz |
| CR | Coding Rate — LoRa error correction ratio (4/5 to 4/8) |
| NSS | Not Slave Select — SPI chip select, active LOW |
| DIO | Digital I/O — configurable interrupt/status pins on Semtech radios |
| TCXO | Temperature Compensated Crystal Oscillator — precision frequency reference |
| PA | Power Amplifier — RF output amplification stage |
| LNA | Low Noise Amplifier — RF input amplification for receiver sensitivity |
