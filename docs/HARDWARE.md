# Hardware — BOM, Pin Assignments, Wiring Diagrams

## Bill of Materials (BOM)

### Core Components

| # | Component | Qty | Notes | Est. Cost |
|---|-----------|-----|-------|-----------|
| 1 | ESP32 DevKit V1 (WROOM-32) | 2 | 30-pin or 38-pin version | $8×2 |
| 2 | OLED Display 0.96" I2C 128x64 | 1 | SSD1306, I2C address 0x3C | $3 |
| 3 | IR Break-Beam Sensor (FC-03) | 1 | Digital output, 3.3V compatible | $2 |
| 4 | HC-SR04 Ultrasonic Sensor | 1 | 2cm-400cm range | $2 |
| 5 | Blue LED (5mm, high brightness) | 4 | 470nm wavelength, 15° angle | $1 |
| 6 | IRLZ44N Logic-Level MOSFET | 1 | For blue LED array PWM | $1 |
| 7 | IRF540N N-Channel MOSFET | 1 | For zapper grid switching | $1 |
| 8 | PAM8403 Mini Amplifier Module | 1 | 5V audio amplifier, 3W | $2 |
| 9 | Speaker (8Ω 3W) | 1 | For mosquito buzzing audio | $2 |
| 10 | TTP223 Capacitive Touch Sensor | 2 | Touch buttons (emotion + speaker) | $1×2 |
| 11 | 12V DC Power Adapter (2A) | 1 | Main power supply | $5 |
| 12 | LM2596 Buck Converter Module | 1 | 12V → 5V step-down | $2 |
| 13 | AMS1117-3.3V LDO Module | 1 | 5V → 3.3V for sensors | $1 |

### Passive Components

| # | Component | Qty | Value | Notes |
|---|-----------|-----|-------|-------|
| 14 | Resistor (1/4W) | 4 | 220Ω | LED current limiting |
| 15 | Resistor (1/4W) | 1 | 10kΩ | HC-SR04 voltage divider (top) |
| 16 | Resistor (1/4W) | 1 | 15kΩ | HC-SR04 voltage divider (bottom) |
| 17 | Resistor (1/4W) | 2 | 1kΩ | MOSFET gate resistors |
| 18 | Resistor (1/4W) | 2 | 10kΩ | MOSFET gate pull-downs |
| 19 | Capacitor (ceramic) | 4 | 100nF | Decoupling capacitors |
| 20 | Capacitor (electrolytic) | 2 | 100µF/25V | Power filtering |
| 21 | 1N4007 Diode | 2 | — | Flyback protection |
| 22 | LED (green, 5mm) | 1 | — | Power/status indicator |

### Connectors & Misc

| # | Component | Qty | Notes |
|---|-----------|-----|-------|
| 23 | Female-Female jumper wires | 20 | For prototype wiring |
| 24 | Male-Female jumper wires | 10 | For sensor connections |
| 25 | PCB prototype board | 2 | 7×9cm perforated board |
| 26 | Screw terminal block (2-pin) | 3 | Power input, HV output |
| 27 | Toggle switch | 1 | Main power ON/OFF |
| 28 | Fuse holder + 1A fuse | 1 | Overcurrent protection |
| 29 | Heat shrink tubing | 1m | Insulation |
| 30 | Enclosure (plastic box) | 1 | 15×10×5cm minimum |

### Safety Components (for Zapper)

| # | Component | Qty | Notes |
|---|-----------|-----|-------|
| 31 | HV transformer module | 1 | 12V → 2kV-4kV (commercial module) |
| 32 | HV capacitor (0.01µF/3kV) | 1 | Energy storage |
| 33 | HV rectifier diode | 2 | 1N4007 × 2 in series |
| 34 | HV fuse (100mA) | 1 | Overcurrent protection |
| 35 | Safety cage / mesh | 1 | Prevents user contact with HV |

**Total Estimated Cost: $45-60**

---

## ESP32 Pin Assignments

### ESP32 #1 — Trap Controller

| GPIO | Function | Direction | Component |
|------|----------|-----------|-----------|
| 25 | LED PWM | Output | Blue LED array (via IRLZ44N MOSFET) |
| 26 | IR Sensor | Input | FC-03 digital out |
| 27 | HC-SR04 Trig | Output | Ultrasonic trigger |
| 14 | HC-SR04 Echo | Input | Ultrasonic echo (via voltage divider) |
| 12 | Speaker PWM | Output | To PAM8403 amplifier input |
| 32 | Zapper Enable | Output | IRF540N MOSFET gate (via 1kΩ) |
| 33 | Status LED | Output | Green LED (heartbeat indicator) |
| 2 | Built-in LED | Output | Debug indicator |
| VIN | 5V Input | Input | From buck converter |
| GND | Ground | — | Common ground |

### ESP32 #2 — UI Gateway

| GPIO | Function | Direction | Component |
|------|----------|-----------|-----------|
| 21 | I2C SDA | Bidir | OLED SSD1306 |
| 22 | I2C SCL | Bidir | OLED SSD1306 |
| 32 | Touch #1 | Input | TTP223 (Emotion cycle) |
| 33 | Touch #2 | Input | TTP223 (Speaker toggle) |
| 2 | Built-in LED | Output | Debug indicator |
| VIN | 5V Input | Input | From buck converter |
| GND | Ground | — | Common ground |

---

## Wiring Diagrams

### HC-SR04 Voltage Divider (3.3V Safe)

**CRITICAL: Never connect HC-SR04 ECHO directly to ESP32! It outputs5V.**

```
HC-SR04 Echo ──┬──[10kΩ]──┬──► ESP32 GPIO 14
                │           │
                └──[15kΩ]──┘
                            │
                           GND
```

Division ratio: 15k/(10k+15k) = 0.6
Output: 5V × 0.6 = 3.0V (safe for 3.3V GPIO)

### Blue LED Array with MOSFET

```
5V ──[220Ω]──► LED1+ ──► LED1- ──► LED2+ ──► LED2- ──► LED3+ ──► LED3- ──► LED4+ ──► LED4- ──┐
                                                                                                │
                                                                                          Drain (IRLZ44N)
                                                                                                │
                                                                                          Source ──► GND
                                                                                                │
                                                                                          Gate ──[1kΩ]──► GPIO 25
                                                                                                │
                                                                                          Gate ──[10kΩ]──► GND
```

### Speaker + PAM8403 Amplifier

```
ESP32               PAM8403 Amplifier        Speaker
─────               ─────────────────        ───────
GPIO 12 ──────────► AUDIO IN+               ┌──────┐
                                       OUT+ ─┤      ├──┐
GND ──────────────► AUDIO IN-          OUT- ─┤  L   │  ├──► Speaker +
                                             │      │  │
5V ───────────────► VCC                GND ──┤      │  ├──► Speaker -
                                             └──────┘
GND ──────────────► GND
```

**3 wires from ESP32 to amplifier:**
1. GPIO 12 → AUDIO IN+
2. GND → AUDIO IN- AND GND
3. 5V → VCC

### Zapper MOSFET Driver

```
12V ──► HV Module Input (+)
GND ──► HV Module Input (-)
HV Module Output ──► Zapper Grid

GPIO 32 ──[1kΩ]──┬──► Gate (IRF540N)
                   │
              [10kΩ]──► GND
                   │
               Source ──► GND
               Drain  ──► HV Module Enable
```

### TTP223 Touch Sensor

```
TTP223 VCC ──► 3.3V
TTP223 GND ──► GND
TTP223 I/O ──► ESP32 GPIO 32 (or 33)
TTP223 SIG ──► Not connected
TTP223 BLG ──► Not connected
```

### I2C OLED Display

```
SSD1306 VCC ──► 3.3V
SSD1306 GND ──► GND
SSD1306 SDA ──► ESP32 GPIO 21
SSD1306 SCL ──► ESP32 GPIO 22
```

Most OLED modules have 4.7kΩ pull-ups on SDA/SCL. If not, add:
```
GPIO 21 ──[4.7kΩ]──► 3.3V
GPIO 22 ──[4.7kΩ]──► 3.3V
```

### Status LED

```
GPIO 33 ──[220Ω]──► LED (green) + ──► LED - ──► GND
```

---

## Power Architecture

```
12V DC Adapter ──► Toggle Switch ──► Fuse (1A) ──► LM2596 Buck ──► 5V Rail
                                                          │
                                                     5V ──┴──► ESP32 #1 VIN
                                                     5V ──┴──► ESP32 #2 VIN
                                                     5V ──┴──► PAM8403 VCC
                                                     5V ──┴──► HC-SR04 VCC

5V ──► AMS1117-3.3V ──► 3.3V Rail
                              │
                     3.3V ──┴──► IR Sensor VCC
                     3.3V ──┴──► OLED VCC
                     3.3V ──┴──► Touch Sensors VCC
```

---

## Safety Design

### High-Voltage Isolation

1. **Physical separation**: HV components in separate compartment
2. **Insulated enclosure**: All HV wiring in heat-shrink tubing
3. **Safety cage**: Metal mesh prevents contact with HV grid
4. **Fusing**: 100mA HV fuse on output, 1A fuse on 12V input
5. **Bleeder resistor**: 10MΩ across HV capacitor for discharge
6. **Interlock switch**: Power cut when enclosure opened (optional)
7. **Warning labels**: Clear HV warning on enclosure
8. **No exposed conductors**: All HV connections soldered and insulated
9. **Current limiting**: HV module inherently current-limited to <20mA
10. **Capacitor rating**: HV cap rated 3kV for safety margin
