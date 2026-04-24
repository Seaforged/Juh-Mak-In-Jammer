# JUH-MAK-IN JAMMER — Build Guide (v3.0)

Hardware bring-up and flashing guide for the dual-board JJ v3.0 system.
For operator instructions after the hardware is built and flashed, see
[`docs/USER_GUIDE.md`](docs/USER_GUIDE.md).

---

## 1. Bill of Materials

| # | Component | Details | Qty | ~Cost | Source |
|---|---|---|---:|---:|---|
| 1 | LilyGo T3-S3 V1.3 | ESP32-S3 + SX1262, OLED, SMA | 1 | $25 | LilyGo / AliExpress |
| 2 | RadioMaster XR1 Nano | ESP32-C3 + LR1121, dual-band | 1 | $12 | RadioMaster / GetFPV |
| 3 | Sub-GHz antenna | 915 MHz SMA whip | 1 | $5 | Amazon |
| 4 | 2.4 GHz antenna | u.FL / MHF4 ISM 2.4G | 1 | $3 | Amazon |
| 5 | Jumper wires | 22 AWG female–female, 4 pcs | 1 set | $2 | Amazon |
| 6 | USB-C data cable | Must support data, not charge-only | 1 | $5 | any |
| | **Total** | | | **~$52** | |

Optional but recommended: an SMA inline attenuator (10–20 dB) and/or a
shielded enclosure for conducted testing against SENTRY-RF.

---

## 2. Wiring

```
T3S3 (ESP32-S3 + SX1262)         XR1 Nano (ESP32-C3 + LR1121)
┌──────────────────────┐         ┌──────────────────────┐
│                      │         │                      │
│  5V (USB VBUS) ──────┼─── Red ─┼── 5V                 │
│  GND ────────────────┼─ Black ─┼── GND                │
│  GPIO 43 (UART TX) ──┼─ Green ─┼── GPIO 20 (RX)       │
│  GPIO 44 (UART RX) ──┼─Yellow ─┼── GPIO 21 (TX)       │
│                      │         │                      │
│  [SMA] Sub-GHz ant   │         │  [u.FL] 2.4 GHz ant  │
│  [OLED] 128x64       │         │  [LED] WS2812 status │
│  [USB-C] PC/power    │         │                      │
└──────────────────────┘         └──────────────────────┘
```

**Critical rules:**

1. **5V, not 3.3V.** The XR1's onboard LDO browns out at 3.3V and
   causes red LED boot loops. Always wire the XR1 5V pin to T3S3 USB VBUS.
2. **UART is crossed.** T3S3 TX (GPIO 43) → XR1 RX (GPIO 20); T3S3 RX
   (GPIO 44) ← XR1 TX (GPIO 21).
3. **Antennas before power.** Never power on a transmitting board with
   no antenna — the SX1262 / LR1121 final stages can be damaged by a
   reflected wave.

Pin definitions live in `include/board_config.h` (T3S3) and
`xr1-firmware/include/xr1_pins.h` (XR1). Never hardcode GPIO numbers
elsewhere.

---

## 3. Software Setup

**Prerequisites:**

- [PlatformIO](https://platformio.org/) (VS Code extension or CLI via `pip install platformio`)
- Python 3.x (used by `esptool.py` for the XR1 passthrough flash)
- Git

**Clone:**

```bash
git clone https://github.com/seaforged-dev/Juh-Mak-In-Jammer.git
cd Juh-Mak-In-Jammer
```

**Build both targets:**

```bash
# T3S3 host firmware (repo root)
pio run -e t3s3

# XR1 secondary firmware
cd xr1-firmware
pio run -e xr1
cd ..
```

Both environments use `huge_app.csv` — the T3S3 needs it for the full
protocol stack; the XR1 needs it for WiFi + BLE + opendroneid + RadioLib.

---

## 4. Flashing the T3S3 (direct USB)

```bash
pio run -e t3s3 --target upload --upload-port COMx
pio device monitor -b 115200
```

If upload fails, hold **BOOT** while pressing **RESET**, then release BOOT
to force download mode.

---

## 5. Flashing the XR1 (via T3S3 passthrough)

The XR1 has no native USB-C. Flashing uses a temporary passthrough sketch
on the T3S3 that bridges the host USB to the XR1 UART.

### Step 1 — Load passthrough on T3S3

```bash
cd tools/xr1_passthrough
pio run --target upload --upload-port COMx
cd ../..
```

### Step 2 — Put XR1 into bootloader

1. Hold the BOOT/BIND button on the XR1 (GPIO 9).
2. Unplug the T3S3 USB cable.
3. Wait ~2 seconds.
4. Plug USB back in while still holding BOOT.
5. Hold BOOT for one more second, then release.

### Step 3 — Flash XR1

```bash
python -m esptool --chip esp32c3 --port COMx --baud 460800 \
    --before default_reset --after hard_reset \
    write_flash -z --flash_mode dio --flash_freq 80m --flash_size 4MB \
    0x0     xr1-firmware/.pio/build/xr1/bootloader.bin \
    0x8000  xr1-firmware/.pio/build/xr1/partitions.bin \
    0x10000 xr1-firmware/.pio/build/xr1/firmware.bin
```

### Step 4 — Restore T3S3 firmware

```bash
pio run -e t3s3 --target upload --upload-port COMx
```

### Step 5 — Power cycle

Unplug USB, wait 2 seconds, plug back in. The T3S3 should boot normally
and print `[XR1] Connected`.

---

## 6. Verification

On a successful dual-board boot you should see:

```
================================
  JUH-MAK-IN JAMMER v3.0.0
  Component: RF
  Board: LilyGo T3S3
================================
OLED: OK
SX1262 init... OK
[XR1] Connected
[XR1-STATUS] 915.000 LORA -10 IDLE
```

- XR1 RGB LED pulses green = idle.
- Type `e1f` — expect `[ELRS] TX: ...` with incrementing packet / hop
  counts.
- Type `q` to stop.
- Type `y` — expect `[XR1-RID] WiFi+BLE+DJI started`.
- Type `yq` to stop RID.
- Type `c5` — all four emitters should report activity.
- Type `cq` to stop.

If any of those fail, see USER_GUIDE.md §5 (Troubleshooting).

---

## 7. Automated Testing Against SENTRY-RF

Connect the JJ T3S3 and the SENTRY-RF board on different COM ports. Test
scripts live in the [SENTRY-RF repository](https://github.com/Seaforged/Sentry-RF):

```bash
python run_test.py e        # ELRS FHSS detection
python run_test.py c        # CW tone
python run_test.py y        # Remote ID (XR1 WiFi + BLE)
python full_validation.py   # All modes, PASS/FAIL summary
```

Adjust COM port constants at the top of each script.

---

## 8. License

MIT License. See [LICENSE](LICENSE).
