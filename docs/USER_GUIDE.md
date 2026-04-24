# JUH-MAK-IN JAMMER — User Guide (v3.0)

Operator's manual for the Juh-Mak-In Jammer (JJ) counter-UAS RF test signal
emulator.

---

## 1. Overview

JJ is a **counter-UAS RF test signal emulator**. It produces physically
accurate drone RF signatures across sub-GHz, 2.4 GHz, WiFi, and BLE, so
passive detection systems (e.g. [SENTRY-RF](https://github.com/Seaforged/Sentry-RF))
can be exercised in a controlled lab environment without real aircraft.

**Dual-board architecture (v3.0):**

| Emitter | Hardware | Coverage |
|---|---|---|
| T3S3 | LilyGo T3-S3 V1.3 (ESP32-S3 + SX1262) | Sub-GHz LoRa/FSK: ELRS 900, Crossfire, SiK/MAVLink, mLRS, LoRaWAN, Meshtastic, Helium |
| XR1 | RadioMaster XR1 Nano (ESP32-C3 + LR1121) | 2.4 GHz LoRa/FSK: ELRS 2.4, Ghost, FrSky D16, FlySky AFHDS-2A, DJI energy |
| XR1 | ESP32-C3 WiFi + BLE | Remote ID: ASTM F3411 ODID (WiFi beacon + BLE 4 legacy), DJI DroneID |

The T3S3 is the host; all operator commands go to the T3S3 USB-C port. The
T3S3 drives the XR1 over a 115200 baud UART link (see BUILD_GUIDE.md §2 for
wiring).

**JJ is NOT a jammer.** It does not disrupt communications. It is a calibrated
RF signal generator for controlled detection-system testing. Use shielded
enclosures or inline attenuators whenever possible.

---

## 2. Getting Started

1. Wire the XR1 to the T3S3 per BUILD_GUIDE.md §2 (5V, GND, crossed UART).
2. Connect the T3S3 to your PC via USB-C.
3. Open a serial terminal at **115200 baud, 8N1**.
4. On boot the T3S3 prints:

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

   If you see `[XR1] No response -- check wiring`, see Troubleshooting §5.

5. Type `h` or `?` and press Enter for the full help menu.
6. Type a command letter (e.g. `e1f`) to start a mode.
7. Type `q` to stop the active mode, or `cq` / `xq` / `yq` for combined /
   XR1 / RID modes.

---

## 3. Serial Command Reference

All commands are single-line, newline-terminated, case-sensitive.

### 3.1 Sub-GHz Drone Protocols (T3S3 / SX1262)

| Cmd | Protocol | Rates / Options | Fidelity |
|---|---|---|---|
| `e` | ExpressLRS FHSS | `e1`–`e6` rate, append `f`/`a`/`u`/`i` domain, append `b` for bind | `[PACKET-AUTH + CRC-14]` |
| `g` | TBS Crossfire FSK | `g` or `g9` = 915 MHz, `g8` = 868 MHz | `[CRSF-FRAME]` |
| `gl` | Crossfire LoRa 50Hz | `gl` / `gl9` = 915, `gl8` = 868 | `[FOOTPRINT]` |
| `k` | SiK Radio (MAVLink) | `k1` = 64 kbps, `k2` = 125 kbps, `k3` = 250 kbps | `[MAVLink-FRAME]` |
| `l` | mLRS | `l1` = 19 Hz LoRa, `l2` = 31 Hz LoRa, `l3` = 50 Hz FSK | `[FOOTPRINT]` |
| `u` | Custom LoRa | `u?` show, `u` start, `uf/us/ub/ur/uh/up/uw` configure | user-defined |

**ELRS air rates** (`e1`..`e6`):

| Cmd | Rate | SF | Hop interval | DVDA | Use case |
|---|---|---|---|---|---|
| `e1` | 200 Hz | SF6 | 4 | No | Racing / freestyle |
| `e2` | 100 Hz | SF7 | 4 | No | Balanced |
| `e3` | 50 Hz  | SF8 | 4 | No | Long range |
| `e4` | 25 Hz  | SF9 | 4 | No | Ultra long range |
| `e5` | 250 Hz | SF6 | 2 | Yes | DVDA fast |
| `e6` | 500 Hz | SF5 | 2 | Yes | DVDA fastest |

ELRS domains: `f` = FCC915, `a` = AU915, `u` = EU868, `i` = IN866.
Example: `e1fb` = ELRS 200 Hz FCC915 in binding state.

### 3.2 2.4 GHz Protocols (XR1 / LR1121)

| Cmd | Protocol | Parameters | Fidelity |
|---|---|---|---|
| `x1` | ELRS 2.4 500 Hz | SF5 / BW 812.5 kHz / CR 4/6, per-nonce CRC-14 | `[PACKET-AUTH + CRC-14 per-nonce]` |
| `x2` | ELRS 2.4 250 Hz | SF6 / BW 812.5 kHz / CR 4/6, per-nonce CRC-14 | `[PACKET-AUTH + CRC-14 per-nonce]` |
| `x3` | ELRS 2.4 150 Hz | SF7 / BW 812.5 kHz / CR 4/6 | `[PACKET-AUTH + CRC-14 per-nonce]` |
| `x4` | ELRS 2.4 50 Hz  | SF9 / BW 812.5 kHz / CR 4/8 | `[PACKET-AUTH + CRC-14 per-nonce]` |
| `x5` | ImmersionRC Ghost | 2406–2475 MHz, GFSK, proprietary frame | `[APPROXIMATE]` |
| `x6` | FrSky D16 | 2.4 GHz GFSK, 47-channel hop | `[FOOTPRINT]` |
| `x7` | FlySky AFHDS-2A | 2.4 GHz GFSK, 16-channel hop | `[FOOTPRINT]` |
| `x8` | DJI energy | OFDM-class energy approximation (LoRa-shaped) | `[FOOTPRINT]` |
| `x9` | Generic 2.4 | `x9 L <sf> <bw> <cr> <count> <startMHz> <spacing> <dwellMs> <pwr>` or `x9 F <brKbps> <devKhz> 0 <count> <startMHz> <spacing> <dwellMs> <pwr>` | user-defined |
| `xq` | Stop XR1 | leaves sub-GHz mode running if active | — |

### 3.3 Remote ID (XR1 WiFi / BLE)

| Cmd | Transport | Spec | Fidelity |
|---|---|---|---|
| `y` / `ya` | All 3 transports | WiFi ODID + BLE ODID + DJI DroneID | — |
| `y1` | WiFi ODID beacon | ASTM F3411, OUI FA:0B:BC, Message Pack | `[PASS]` (opendroneid-core-c) |
| `y2` | BLE 4 ODID | UUID 0xFFFA, legacy advertisement | `[BLE4-23B, PARTIAL]` (23-byte truncation) |
| `y3` | DJI DroneID | OUI 26:37:12, Flight Telemetry | `[PARTIAL]` (dynamic telemetry, attitude drift) |
| `y4` | WiFi NaN | (NAN API not exposed on ESP32-C3) | `[STUB]` |
| `yq` | Stop all RID | — | — |

### 3.4 Combined Multi-Emitter Scenarios

Each scenario fires several emitters concurrently to simulate realistic mixed
threat environments.

| Cmd | Scenario | Emitters |
|---|---|---|
| `c1` | Racing Drone | ELRS 200 Hz sub-GHz + ELRS 500 Hz 2.4 + WiFi ODID + BLE ODID |
| `c2` | DJI Consumer | DJI energy 2.4 + DJI DroneID + BLE ODID (no sub-GHz) |
| `c3` | Long Range FPV | Crossfire 915 MHz + WiFi ODID + BLE ODID |
| `c4` | Dual-Band ELRS | ELRS sub-GHz + ELRS 2.4 (no RID) |
| `c5` | Everything | ELRS dual-band + WiFi ODID + BLE ODID + DJI DroneID |
| `cq` | Stop combined | — |

### 3.5 Infrastructure / False Positive Tests

Non-drone LoRa / WiFi traffic that a detector must correctly **reject**.

| Cmd | Source | Pattern |
|---|---|---|
| `i` | LoRaWAN US915 | Single node, 8 SB2 channels, 30–60 s interval |
| `f1` | Meshtastic | Fixed freq, sync 0x2B, 16-symbol preamble, periodic beacon |
| `f2` | Helium PoC | 5 hotspots, rotating SB2 channels |
| `f3` | LoRaWAN EU868 | 868.1 / 868.3 / 868.5 MHz |
| `m` | Mixed FP | LoRaWAN + ELRS interleaved |

### 3.6 Special Modes

| Cmd | Mode | Notes |
|---|---|---|
| `c` (alone) | CW Tone | Continuous wave |
| `b` | Band sweep | `d` cycles dwell, `s` cycles step (sweep only) |
| `t` | Power ramp | Drone-approach simulation |
| `r` | Remote ID (T3S3 legacy) | WiFi + BLE ASTM F3411 from T3S3 WiFi/BLE |
| `x` (alone) | Dual-core combined | RID (Core 0) + ELRS (Core 1) |
| `w` | Drone swarm | 1–16 virtual drones via WiFi beacons |
| `n` | Cycle swarm count | Sizes: 1 / 4 / 8 / 16 |

### 3.7 Controls

| Cmd | Action |
|---|---|
| `q` | Stop current T3S3 mode |
| `xq` | Stop XR1 2.4 GHz mode (leaves sub-GHz running) |
| `yq` | Stop XR1 RID |
| `cq` | Stop combined scenario |
| `p` | Cycle TX power (-9 / 0 / 5 / 10 / 14 / 17 / 20 / 22 dBm) |
| `h` / `?` | Print help |

---

## 4. Protocol Fidelity Ratings

Each emulated protocol is rated at five layers: **energy, modulation,
protocol-ID, content, behavior**.

| Rating | Meaning |
|---|---|
| **PASS** | Would fool a commercial detector at all five layers (upstream-grounded packet structure, CRC, and behavior). |
| **PARTIAL** | RF envelope correct + some authentic content, with gaps at content or behavior layers. |
| **FOOTPRINT** | Correct energy + modulation class, but no authentic packet framing. |
| **STUB** | Placeholder — unavailable due to hardware or API limitations. |
| **APPROXIMATE** | Modulation and energy plausible, but underlying protocol is proprietary and not reverse-engineered in this firmware. |

**v3.0 Scorecard:**

| Protocol | Rating | Notes |
|---|---|---|
| ELRS 900 MHz | PASS | CRC-14 upstream-grounded, hop sequence via FCC915 LCG |
| ELRS 2.4 GHz | PASS | Per-nonce CRC-14 recompute on XR1 |
| Crossfire FSK | PARTIAL | CRSF RC_CHANNELS_PACKED frame, 42.48 kHz deviation |
| Crossfire LoRa | FOOTPRINT | SF / BW / CR estimated from SX127x defaults |
| SiK / MAVLink | PARTIAL | Mixed HEARTBEAT + SYS_STATUS, X.25 CRC + CRC_EXTRA |
| mLRS | FOOTPRINT | 43 channels grounded from olliw42/mLRS, content approximate |
| DJI DroneID | PARTIAL | Dynamic telemetry, home-lock, attitude drift |
| ODID WiFi | PASS | opendroneid-core-c encoding |
| ODID BLE | PARTIAL | BLE 4 23-byte truncation |
| Ghost / FrSky / FlySky | FOOTPRINT | Energy + GFSK class only |
| DJI energy (`x8`) | FOOTPRINT | LoRa-shaped energy approximation |
| WiFi NaN (`y4`) | STUB | ESP32-C3 does not expose NAN API |

See [`docs/JJ_Protocol_Emulation_Reference_v2.md`](JJ_Protocol_Emulation_Reference_v2.md)
for the full parameter reference with upstream citations.

---

## 5. Troubleshooting

**`[XR1] No response -- check wiring`**
- Confirm 5V (not 3.3V) is wired from T3S3 VBUS to XR1 5V pin.
- Confirm UART is crossed: T3S3 GPIO 43 (TX) → XR1 GPIO 20 (RX); T3S3 GPIO 44
  (RX) ← XR1 GPIO 21 (TX).
- Power-cycle both boards. The XR1 prints `XR1 READY` over its own USB
  console on a successful boot.

**Red LED solid / blinking at 1 Hz on XR1 boot**
- Almost always a 3.3V brownout. Switch to 5V VBUS.
- If on 5V and still red, the LR1121 SPI wiring is wrong or the stock ELRS
  firmware is still writing custom LR1121 code — reflash Semtech transceiver
  image via ELRS web UI (see `xr1-firmware/CLAUDE.md`).

**No hop count increasing in serial output**
- The mode's state machine is not being ticked. Make sure you did not enter
  the mode inside a `q`-interrupted context; press `q` twice and retry.

**COM port not detected on Windows**
- Install the CP2102 / CP210x VCP driver from Silicon Labs.
- Check `ARDUINO_USB_CDC_ON_BOOT=1` is set in `platformio.ini` (it is by
  default for this project).

**`[XR1] health check miss N/3` during an active session**
- The T3S3 polls the XR1 every 10 seconds during XR1-backed modes. After 3
  missed PINGs the active mode is aborted and the menu returns to idle. This
  protects the UI from claiming the emitter is live when it has crashed or
  reset.

**ELRS `e1f` starts but reports `start failed`**
- The SX1262 failed `radio.begin()` at boot — check the `[BOOT] SX1262 init
  FAILED` line. Sub-GHz modes are disabled in that state; re-seat the SMA
  cable, then reset.

---

## 6. See Also

- [BUILD_GUIDE.md](../BUILD_GUIDE.md) — hardware bring-up, BoM, wiring, flashing
- [`docs/JJ_Protocol_Emulation_Reference_v2.md`](JJ_Protocol_Emulation_Reference_v2.md)
  — authoritative protocol parameters
- [`docs/JJ_v3_Consolidated_Roadmap.md`](JJ_v3_Consolidated_Roadmap.md) —
  phase history and architecture
- [SENTRY-RF](https://github.com/Seaforged/Sentry-RF) — companion passive
  detector
