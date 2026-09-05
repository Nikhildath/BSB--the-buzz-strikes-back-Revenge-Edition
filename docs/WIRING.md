# Complete Wiring Guide — Mosquito Trap System

## What You Need

### ESP32 Boards
- 2x ESP32 DevKit V1 (30-pin or 38-pin)
- Label them: **TRAP** (ESP32 #1) and **GATEWAY** (ESP32 #2)

### Jumper Wires
- 20x Female-Female
- 10x Male-Female

### Tools
- Soldering iron + solder
- Multimeter
- Breadboard (for testing)

---

## STEP 1: MAC Addresses (Already Done)

Both boards have manual MAC addresses hardcoded. No need to read MACs.

| Board | MAC | Firmware |
|-------|-----|----------|
| TRAP | `02:00:00:00:00:01` | trap_controller.ino |
| GATEWAY | `02:00:00:00:00:02` | ui_gateway.ino |

Just upload the firmware and they will connect.

---

## STEP 2: Copy Protocol Header

Copy `mosquito_protocol.h` to both firmware `src/` folders:

```
firmware/common/mosquito_protocol.h
    ↓ COPY TO ↓
firmware/esp32_trap_controller/src/mosquito_protocol.h
firmware/esp32_ui_gateway/src/mosquito_protocol.h
```

---

## STEP 3: Power Wiring (Do First)

### Wire Colors
- **Red** = 5V / 12V power
- **Black** = Ground (GND)
- **Orange** = 3.3V
- **Green** = Signal / Data
- **Blue** = I2C
- **Yellow** = PWM / Audio

### Power Distribution

```
12V Adapter ──► Switch ──► Fuse(1A) ──► LM2596 Buck ──► 5V
                                                  │
                                            ┌─────┴─────┐
                                            │           │
                                        ESP32#1 VIN  ESP32#2 VIN
                                        (5V pin)    (5V pin)
                                             │
                                        MAX98357A VIN
                                             │
                                        HC-SR04 VCC

5V ──► AMS1117-3.3V ──► 3.3V rail
                              │
                    ┌─────────┼─────────┐
                    │         │         │
              IR Sensor   OLED VCC   Touch Sensors
              VCC (3.3V)  VCC(3.3V)  VCC (3.3V)
```
1. 12V+ → Switch → Fuse → LM2596 IN+
2. 12V- → LM2596 IN-
3. Set LM2596 output to 5V (adjust potentiometer)
4. LM2596 OUT+ → Both ESP32 VIN pins (red wire)
5. LM2596 OUT- → Both ESP32 GND pins (black wire)
6. 5V → AMS1117 IN → 3.3V output rail
7. Connect ALL GND together (common ground)

---

## STEP 4: ESP32 #1 (TRAP CONTROLLER)

### Pin Reference

| ESP32 Pin | GPIO | Connects To | Wire Color |
|-----------|------|-------------|------------|
| D25 | 25 | MOSFET Gate → Blue LEDs | Yellow |
| D26 | 26 | IR Sensor OUT | Green |
| D27 | 27 | HC-SR04 TRIG | Blue |
| D14 | 14 | Voltage divider → HC-SR04 ECHO | Blue |
| D5  | 5  | MAX98357A BCLK | Yellow |
| D18 | 18 | MAX98357A LRC | Yellow |
| D19 | 19 | MAX98357A DIN | Yellow |
| D32 | 32 | MOSFET Gate → Zapper | Red |
| D33 | 33 | Status LED | White |
| D2 | 2 | Built-in LED | — |
| VIN | — | 5V from buck | Red |
| GND | — | Common ground | Black |

### A. Blue LED Array (GPIO 25)

```
GPIO 25 ──[1kΩ]──┬──► IRLZ44N Gate
                   │
              [10kΩ]──► GND
                   │
             IRLZ44N Source ──► GND
             IRLZ44N Drain ──┐
                              │
                    ┌─────────┘
                    │
          5V ──[220Ω]──► LED1+ ──► LED1- ──► LED2+ ──► LED2- ──► LED3+ ──► LED3- ──► LED4+ ──► LED4- ──► GND
```

**Parts:** 1x IRLZ44N, 1x 1kΩ, 1x 10kΩ, 4x 220Ω, 4x Blue LEDs

### B. IR Sensor (GPIO 26)

```
IR Sensor (FC-03)    ESP32
─────────────────    ─────
VCC  ──────────────► 3.3V
GND  ──────────────► GND
OUT  ──────────────► D26
```

Direct connection — FC-03 is 3.3V compatible.

### C. HC-SR04 with Voltage Divider (GPIO 27, 14)

```
HC-SR04          Voltage Divider          ESP32
────────         ──────────────           ─────
VCC  ──────────► 5V                       (direct)
GND  ──────────► GND                      (direct)
TRIG ──────────► D27                      (direct)
ECHO ─────┬────► [10kΩ] ──┬──► D14
           │               │
           │          [15kΩ]──► GND
```

**CRITICAL: Never connect ECHO directly to ESP32! It outputs 5V.**

The voltage divider reduces 5V → 3.0V (safe for 3.3V GPIO).

**Parts:** 1x 10kΩ, 1x 15kΩ

### D. Speaker + MAX98357A Amplifier (GPIO 5, 18, 19)

```
ESP32               MAX98357A Amplifier        Speaker
─────               ───────────────────        ───────
GPIO 5  ──────────► BCLK (Bit Clock)     ┌──────┐
GPIO 18 ──────────► LRC (Word Select)  OUT+ ┤      ├──┐
GPIO 19 ──────────► DIN (Data In)      OUT- ┤  L   │  ├──► Speaker +
                                          │      │  │
GND ──────────────► GND              GND ──┤      │  ├──► Speaker -
                                          └──────┘
5V ───────────────► VIN
GND ──────────────► GND
```

**MAX98357A I2S Amplifier — 5 wires from ESP32:**
1. GPIO 5 → BCLK (Bit Clock)
2. GPIO 18 → LRC (Left/Right Clock, word select)
3. GPIO 19 → DIN (Serial Data In)
4. 5V → VIN
5. GND → GND (common ground with ESP32)

**MAX98357A additional pins:**
- GAIN → GND = 12dB (as wired)
  - Floating = 15dB
  - VIN = 3dB

**Parts:** 1x MAX98357A, 1x Speaker 4Ω/8Ω 3W

### E. Zapper MOSFET (GPIO 32)

```
GPIO 32 ──[1kΩ]──┬──► IRF540N Gate
                   │
              [10kΩ]──► GND
                   │
             IRF540N Source ──► GND
             IRF540N Drain ──► HV Module Enable
```

**WARNING:** HV section must be physically separated from ESP32!

### F. Status LED (GPIO 33)

```
GPIO 33 ──[220Ω]──► LED (green) + ──► LED - ──► GND
```

---

## STEP 5: ESP32 #2 (UI GATEWAY)

### Pin Reference

| ESP32 Pin | GPIO | Connects To | Wire Color |
|-----------|------|-------------|------------|
| D21 | 21 | OLED SDA | Blue |
| D22 | 22 | OLED SCL | Yellow |
| D32 | 32 | Touch Sensor 1 (Emotion) | Green |
| D33 | 33 | Touch Sensor 2 (Speaker) | Green |
| D2 | 2 | Built-in LED | — |
| VIN | — | 5V from buck | Red |
| GND | — | Common ground | Black |

### A. OLED Display — Emotic Eyes (I2C)

```
SSD1306 OLED           ESP32
─────────────          ─────
VCC  ─────────────► 3.3V
GND  ─────────────► GND
SDA  ─────────────► D21
SCL  ─────────────► D22
```

**I2C Address:** 0x3C (default for SSD1306)
**Library:** U8g2 (esp32-eyes emotic eyes animation system)
**Display:** 128x64 SSD1306 OLED — shows animated emoticon eyes + status info

### B. Touch Sensor 1 — Emotion (GPIO 32)

```
TTP223 Module       ESP32
─────────────       ─────
VCC  ─────────────► 3.3V
GND  ─────────────► GND
I/O  ─────────────► D32
```

### C. Touch Sensor 2 — Speaker (GPIO 33)

```
TTP223 Module       ESP32
─────────────       ─────
VCC  ─────────────► 3.3V
GND  ─────────────► GND
I/O  ─────────────► D33
```

---

## STEP 6: Upload Firmware

### Edit WiFi Credentials

In `firmware/esp32_ui_gateway/src/ui_gateway.ino`:
```cpp
const char* STASSID = "YOUR_WIFI_NAME";
const char* STAPASS = "YOUR_WIFI_PASSWORD";
```

### Upload

```bash
# Trap Controller
cd firmware/esp32_trap_controller
pio run -t upload

# UI Gateway
cd firmware/esp32_ui_gateway
pio run -t upload
```

### Verify

Open serial monitors. You should see:
```
TRAP:    "Trap MAC set to: 02:00:00:00:00:01"
         "Peer connected" (within 5s)

GATEWAY: "Gateway MAC set to: 02:00:00:00:00:02"
         "Web server started on port 80"
```

---

## Quick Reference: All Connections

### ESP32 #1 (TRAP)
```
GPIO 25 ──[1kΩ]── MOSFET Gate ── Blue LEDs
GPIO 26 ────────── IR Sensor OUT
GPIO 27 ────────── HC-SR04 TRIG
GPIO 14 ──[10kΩ/15kΩ divider]── HC-SR04 ECHO
GPIO 5  ────────── MAX98357A BCLK
GPIO 18 ────────── MAX98357A LRC
GPIO 19 ────────── MAX98357A DIN
GPIO 32 ──[1kΩ]── MOSFET Gate ── Zapper
GPIO 33 ──[220Ω]── Status LED
```

### ESP32 #2 (GATEWAY)
```
GPIO 21 ────────── OLED SDA (emotic eyes)
GPIO 22 ────────── OLED SCL (emotic eyes)
GPIO 32 ────────── Touch 1 (Emotion)
GPIO 33 ────────── Touch 2 (Speaker)
```

### Power
```
12V ── Switch ── Fuse ── Buck ── 5V ── ESP32 VIN ×2
5V  ── AMS1117 ── 3.3V ── OLED, IR, Touch
5V  ── MAX98357A VIN
5V  ── HC-SR04 VCC
```

---

## Common Mistakes

1. **HC-SR04 ECHO to ESP32 without divider** → Fries GPIO. Always use divider.
2. **Wrong MAC addresses** → ESP-NOW won't connect. Already fixed (manual MACs).
3. **Forgot to copy protocol.h** → Compilation error. Copy to both src/ folders.
4. **MOSFET gate without pull-down** → Floating gate. Add 10kΩ to GND.
5. **VIN pin not used** → Use VIN for 5V power, not USB when running standalone.
6. **No common ground** → ESP-NOW fails. Connect ALL GND wires together.
7. **OLED not showing** → Check I2C address (0x3C), check SDA/SCL not swapped.
8. **Speaker silent** → Check amplifier 5V power, check GPIO 5/18/19 wiring.
9. **Lift always triggering** → Restart to recalibrate baseline height.

---

## Testing Checklist

- [ ] Both ESP32 boot and show MAC addresses on serial
- [ ] ESP-NOW "Peer connected" within 5 seconds
- [ ] Blue LED turns on/off via web dashboard
- [ ] Brightness slider controls LED dimming
- [ ] IR sensor triggers detection (wave hand)
- [ ] Detection count increments correctly
- [ ] HC-SR04 shows baseline height on serial
- [ ] HC-SR04 shows "LIFTED" when trap raised
- [ ] OLED shows face and cycles through 5 emotions
- [ ] Touch sensor 1 cycles emotions
- [ ] Touch sensor 2 toggles speaker
- [ ] Speaker plays mosquito buzzing sound
- [ ] Volume slider controls speaker volume
- [ ] Web dashboard loads on phone
- [ ] Dashboard updates without page refresh
- [ ] Status LED blinks (heartbeat)
