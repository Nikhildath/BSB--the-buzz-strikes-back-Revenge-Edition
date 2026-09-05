# Mosquito Trap & Monitoring System

Fully automated mosquito trap using two ESP32 boards, ESP-NOW communication, sensors, OLED touchscreen with animated faces, and a web-based control dashboard.

## System Overview

```
┌──────────────────┐   ESP-NOW   ┌──────────────────┐
│   ESP32 #1       │◄───────────►│   ESP32 #2       │
│   Trap Controller│  (2.4GHz)   │   UI Gateway     │
│                  │             │                  │
│  Blue LED (PWM)  │             │  OLED 128x64    │
│  IR Sensor       │             │  Touch Sensors   │
│  HC-SR04         │             │  Web Server      │
│  Speaker+Amp     │             │  WebSocket       │
│  Zapper (MOSFET) │             │  Wi-Fi AP+STA   │
└──────────────────┘             └──────────────────┘
```

## Features

- **Blue LED attractant** with adjustable PWM brightness
- **IR sensor** for mosquito detection with 2s debounce
- **HC-SR04** ultrasonic for height monitoring and **lift detection**
- **Speaker + PAM8403 amplifier** for mosquito buzzing sound (400-500Hz varying)
- **Zapper control** with safety MOSFET driver
- **OLED touchscreen** with 5 animated face emotions
- **Touch sensors** for emotion cycling and buzz toggle
- **Lift detection** — HC-SR04 monitors ground height, scared face when trap is lifted
- **Real-time web dashboard** with WebSocket updates (no page refresh)
- **ESP-NOW** low-latency inter-board communication with manual MAC addresses
- **Multiple operating modes**: Auto, Manual, Schedule, Silent
- **Detection counter** with timestamp logging
- **Volume control** for speaker output
- **Error monitoring** and status reporting

## OLED Emotions

| # | Face | When |
|---|------|------|
| 0 | Happy | Normal operation |
| 1 | Neutral | Manual mode |
| 2 | Angry | Mosquito detected / buzzing active |
| 3 | Sleeping | Silent/disabled mode |
| 4 | Scared | Trap has been lifted |

Touch sensor 1 cycles through all 5 emotions.
Touch sensor 2 toggles speaker on/off.

## Quick Start

### Prerequisites
- [PlatformIO](https://platformio.org/) or Arduino IDE
- 2x ESP32 DevKit V1 boards
- All components listed in `docs/HARDWARE.md`

### Step 1: MAC Addresses (Already Set)

Both ESP32 boards have manual MAC addresses hardcoded. No need to read MACs.

| Board | MAC Address |
|-------|-------------|
| ESP32 #1 (Trap) | `02:00:00:00:00:01` |
| ESP32 #2 (Gateway) | `02:00:00:00:00:02` |

### Step 2: Update WiFi Credentials

In `firmware/esp32_ui_gateway/src/ui_gateway.ino`:
```cpp
const char* STASSID = "YOUR_WIFI_SSID";
const char* STAPASS = "YOUR_WIFI_PASS";
```

### Step 3: Copy Protocol Header

Copy `firmware/common/mosquito_protocol.h` to both:
```
firmware/esp32_trap_controller/src/mosquito_protocol.h
firmware/esp32_ui_gateway/src/mosquito_protocol.h
```

### Step 4: Upload Firmware

```bash
# ESP32 #1 (Trap Controller)
cd firmware/esp32_trap_controller
pio run -t upload

# ESP32 #2 (UI Gateway)
cd firmware/esp32_ui_gateway
pio run -t upload
```

### Step 5: Access Dashboard

1. Connect to `MosquitoTrap` WiFi (password: `trap1234`)
2. Open browser to `http://192.168.4.1`

## Project Structure

```
mosquito-trap/
├── firmware/
│   ├── common/
│   │   └── mosquito_protocol.h        # Shared ESP-NOW protocol
│   ├── esp32_trap_controller/
│   │   └── src/
│   │       ├── trap_controller.ino     # Trap firmware
│   │       └── mosquito_protocol.h     # Copy here
│   │   └── platformio.ini
│   └── esp32_ui_gateway/
│       └── src/
│           ├── ui_gateway.ino          # Gateway firmware
│           └── mosquito_protocol.h     # Copy here
│       └── platformio.ini
├── web/
│   ├── index.html                      # Dashboard HTML
│   ├── css/style.css                  # Styles
│   └── js/app.js                      # Client JavaScript
├── docs/
│   ├── ARCHITECTURE.md                # System design
│   ├── HARDWARE.md                    # BOM and wiring
│   ├── WIRING.md                      # Step-by-step wiring guide
│   └── TESTING.md                     # Test procedures
└── README.md
```

## ESP-NOW Protocol

Messages are 16-byte packed structs with XOR checksum validation.

| Type | Value | Direction | Description |
|------|-------|-----------|-------------|
| STATUS_UPDATE | 0x00 | Trap→GW | Periodic status (every 2s) |
| COMMAND | 0x01 | GW→Trap | User command |
| ACK | 0x02 | Both | Acknowledgement |
| HEARTBEAT | 0x03 | Both | Keep-alive (every 5s) |
| DETECTION_EVENT | 0x04 | Trap→GW | Mosquito detected |
| CONFIG_UPDATE | 0x05 | GW→Trap | Settings change |
| LIFT_ALERT | 0x06 | Trap→GW | Trap has been lifted |

### Commands

| Command | Value | Param | Description |
|---------|-------|-------|-------------|
| CMD_LED_ON | 0x01 | — | Turn blue LED on |
| CMD_LED_OFF | 0x02 | — | Turn blue LED off |
| CMD_LED_BRIGHTNESS | 0x03 | 0-255 | Set LED brightness |
| CMD_BUZZ_ON | 0x04 | — | Start mosquito sound |
| CMD_BUZZ_OFF | 0x05 | — | Stop mosquito sound |
| CMD_ZAPPER_ON | 0x06 | — | Arm zapper |
| CMD_ZAPPER_OFF | 0x07 | — | Disarm zapper |
| CMD_SET_MODE | 0x08 | 0-3 | Set operating mode |
| CMD_RESET_COUNTER | 0x09 | — | Reset detection count |
| CMD_RESTART | 0x0A | — | Restart ESP32 |
| CMD_SET_VOLUME | 0x0B | 0-255 | Set speaker volume |

## Safety Notes

- High-voltage zapper section must be physically isolated
- Use fuse protection on all power rails
- HV capacitor needs bleeder resistor for safe discharge
- Never expose conductive parts to user access
- ESP32 low-voltage section is isolated from HV by design

## License

MIT
