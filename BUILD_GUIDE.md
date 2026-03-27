# JUH-MAK-IN JAMMER — Build Guide

## Prerequisites

1. **PlatformIO** — VS Code extension or CLI:
   ```bash
   pip install platformio
   ```

2. **Hardware**: LilyGo T3S3 V1.3 (ESP32-S3 + SX1262)
   - Same board as SENTRY-RF — you can flash either firmware
   - 868/915 MHz SMA antenna recommended (connect before powering on)
   - USB-C cable for power and programming
   - No external wiring needed — everything runs on the dev board

3. **Python 3** (for automated testing scripts):
   ```bash
   pip install pyserial
   ```

## Build & Flash

```bash
# Clone
git clone https://github.com/seaforged-dev/Juh-Mak-In-Jammer.git
cd Juh-Mak-In-Jammer

# Build
pio run -e jammer_rf

# Flash (replace COM6 with your port)
pio run -e jammer_rf -t upload --upload-port COM6

# Find your port
pio device list
```

If upload fails, hold BOOT button while pressing RESET, then release BOOT to enter download mode.

## Serial Monitor

```bash
pio device monitor -b 115200
```

On boot you should see:
```
================================
  JUH-MAK-IN JAMMER v1.1.0
  Component: RF
  Board: LilyGo T3S3
================================
OLED: OK
SX1262 init... OK
JAMMER-RF ready.
Serial commands:
  c = CW tone mode
  e = ELRS 915 FHSS mode
  b = Band sweep mode
  r = RID spoofer (WiFi+BLE)
  m = Mixed false positive (LoRaWAN+ELRS)
  x = Combined (RID + ELRS dual-core)
  w = Drone swarm simulator
  p = cycle TX power
  d = cycle dwell time (sweep)
  s = cycle step size (sweep)
  n = cycle swarm drone count (1/4/8/16)
  q = stop TX and return to menu
```

## Using Serial Commands

Type a single character and press Enter. The JAMMER responds immediately:

```
c           → CW TX ON: 915.00 MHz @ 10 dBm
p           → TX power: 14 dBm
p           → TX power: 17 dBm
p           → TX power: 20 dBm
p           → TX power: 22 dBm
q           → TX stopped via serial.
e           → ELRS TX ON: SF6 BW500, 80ch FHSS, 200Hz, 22 dBm
q           → TX stopped via serial.
w           → SWARM: 4 drones active, broadcasting RID beacons
n           → SWARM: drone count set to 8
q           → SWARM: Stopped after 247 beacons
```

Power cycles through: -9, 0, 5, 10, 14, 17, 20, 22 dBm (wraps around).

## OLED Menu (Button Control)

The BOOT button provides menu navigation without a serial connection:
- **Short press** (< 1s): Navigate / cycle options
- **Long press** (> 1s): Select / confirm

Menu structure:
```
[1] RID Spoofer      → WiFi + BLE Remote ID beacons
[2] RF Sig Gen       → CW Tone / Band Sweep / ELRS / Crossfire / Power Ramp
[3] False Pos Gen    → LoRaWAN / ISM Burst / Mixed
[4] Combined         → RID + ELRS dual-core
[5] Swarm Sim        → 1-16 virtual drones
```

## Automated Testing Against SENTRY-RF

Connect both boards via USB. The JAMMER and SENTRY-RF must be on different COM ports.

### Single Mode Test

```bash
# From the SENTRY-RF project directory:
python run_test.py c    # Test CW tone detection
python run_test.py e    # Test ELRS FHSS detection
python run_test.py b    # Test band sweep detection
python run_test.py r    # Test Remote ID (WiFi)
python run_test.py w    # Test drone swarm
python run_test.py q    # Test baseline (no TX)
```

Edit the COM port numbers at the top of `run_test.py` if they differ from COM6 (JAMMER) and COM8 (SENTRY-RF).

### Detection Probability Measurement

```bash
python threshold_test.py
```

This sweeps through all TX power levels (-9 to +22 dBm) in both CW and ELRS modes, measuring SENTRY-RF's detection probability at each level. Produces a table:

```
  Power    Avg RSSI        Pd   Det/Tot    Max Threat
  +22 dBm  -60.9 dBm     100%  7/  7      CRITICAL
  +14 dBm  -65.1 dBm     100%  8/  8      CRITICAL
  +10 dBm  -72.5 dBm      50%  4/  8      CRITICAL
   +0 dBm  -75.8 dBm      20%  2/ 10      CRITICAL
```

## Wiring

No external wiring needed. The JAMMER uses only the LilyGo T3S3's built-in components:
- **SX1262** radio for sub-GHz transmission (CW, sweep, ELRS, Crossfire)
- **ESP32-S3 WiFi** for Remote ID beacon broadcasting
- **ESP32-S3 BLE** for BLE Remote ID advertising
- **SSD1306 OLED** for menu display
- **BOOT button** for menu navigation

Connect a 868/915 MHz antenna to the SMA/U.FL port before transmitting.

## License

MIT License. See [LICENSE](LICENSE).
