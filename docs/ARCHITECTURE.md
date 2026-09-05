# Mosquito Trap System — Architecture

## System Block Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                        POWER SUPPLY                             │
│  12V DC Adapter ──► Switch ──► Fuse ──► 5V Buck ──► ESP32 ×2  │
│                                     └──► 3.3V LDO (sensors)    │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────┐    ESP-NOW    ┌─────────────────────────────┐
│       ESP32 #1              │◄─────────────►│       ESP32 #2              │
│    TRAP CONTROLLER          │   (2.4 GHz)   │   UI GATEWAY                │
│                             │                │                             │
│  ┌───────────┐              │                │  ┌───────────┐              │
│  │ Blue LED  │◄── PWM       │                │  │ OLED 128x64│◄── I2C     │
│  │ (GPIO 25)│              │                │  │ (Touch)    │              │
│  └───────────┘              │                │  └───────────┘              │
│                             │                │                             │
│  ┌───────────┐              │                │  ┌───────────┐              │
│  │ IR Sensor │──► GPIO 26   │                │  │ Touch #1  │──► GPIO 32  │
│  │ (FC-03)   │              │                │  │ (Emotion) │              │
│  └───────────┘              │                │  └───────────┘              │
│                             │                │                             │
│  ┌───────────┐              │                │  ┌───────────┐              │
│  │ HC-SR04   │──► GPIO 14   │                │  │ Touch #2  │──► GPIO 33  │
│  │ (height)  │◄── GPIO 27   │                │  │ (Speaker) │              │
│  └───────────┘              │                │  └───────────┘              │
│                             │                │                             │
│  ┌───────────┐              │                │  ┌───────────┐              │
│  │ Speaker   │◄── GPIO 12   │                │  │ Wi-Fi     │──► Web UI   │
│  │ + PAM8403 │              │                │  │ (AP+STA)  │              │
│  └───────────┘              │                │  └───────────┘              │
│                             │                │                             │
│  ┌───────────┐              │                │                             │
│  │ MOSFET    │◄── GPIO 32   │                │                             │
│  │ (Zapper)  │              │                │                             │
│  └───────────┘              │                │                             │
│                             │                │                             │
│  ┌───────────┐              │                │                             │
│  │ Status LED│◄── GPIO 33   │                │                             │
│  └───────────┘              │                │                             │
└─────────────────────────────┘                └─────────────────────────────┘
```

## ESP-NOW Communication Protocol

### Message Structure (16 bytes, packed)

```
Byte 0:      Message Type (uint8, enum)
Byte 1:      Device ID (0x01 = Trap, 0x02 = Gateway)
Byte 2-5:    Timestamp (uint32, seconds since boot)
Byte 6:      LED State (0=off, 1=on)
Byte 7:      LED Brightness (0-255)
Byte 8:      Buzz/Speaker State (0=off, 1=on)
Byte 9:      IR Sensor State (0=clear, 1=detection)
Byte 10:     HC-SR04 Distance (cm, 0-255)
Byte 11:     Operating Mode (0=auto, 1=manual, 2=schedule, 3=silent)
Byte 12-13:  Detection Count (uint16, little-endian)
Byte 14:     Error Flags (bitfield)
Byte 15:     Checksum (XOR of bytes 0-14)
```

### Message Types

| Value | Name | Direction | Description |
|-------|------|-----------|-------------|
| 0x00 | STATUS_UPDATE | Trap→Gateway | Periodic status every 2s |
| 0x01 | COMMAND | Gateway→Trap | User command from web/touchscreen |
| 0x02 | ACK | Both | Command acknowledgement |
| 0x03 | HEARTBEAT | Both | Keep-alive every 5s |
| 0x04 | DETECTION_EVENT | Trap→Gateway | Mosquito detected |
| 0x05 | CONFIG_UPDATE | Gateway→Trap | Settings change |
| 0x06 | LIFT_ALERT | Trap→Gateway | Trap has been lifted |

### Error Flags (Bitfield)

| Bit | Flag | Description |
|-----|------|-------------|
| 0 | ERR_IR_FAULT | IR sensor malfunction |
| 1 | ERR_HCSR04_TIMEOUT | HC-SR04 no echo |
| 2 | ERR_ZAPPER_OVCUR | Zapper overcurrent |
| 3 | ERR_LED_FAULT | LED array fault |
| 4 | ERR_SPEAKER_FAULT | Speaker/amplifier fault |
| 5 | ERR_OVER_TEMP | Over-temperature |
| 6 | ERR_LIFTED | Trap has been lifted from surface |
| 7 | ERR_COMM_ERROR | ESP-NOW communication lost |

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

### Connection Monitoring

- Both boards send HEARTBEAT every 5 seconds
- If no message received for 10 seconds → mark device offline
- Error flag ERR_COMM_ERROR set on communication loss
- Automatic reconnection when boards come back online
- No manual reset required

## Operating Sequence

```
1. Power On
   ├── ESP32 #1 boots → calibrates HC-SR04 baseline height (10 readings)
   ├── ESP32 #1 → sets custom MAC 02:00:00:00:00:01
   ├── ESP32 #2 boots → inits OLED, Wi-Fi AP, web server
   ├── ESP32 #2 → sets custom MAC 02:00:00:00:00:02
   └── Both exchange HEARTBEAT messages via ESP-NOW

2. Normal Operation (Automatic Mode)
   ├── Blue LED ON (configurable brightness via PWM)
   ├── Speaker plays mosquito buzzing sound (400-500Hz varying)
   ├── IR sensor continuously monitored
   │   ├── Break detected → 2-second debounce filter
   │   ├── Valid detection → increment counter
   │   ├── Send DETECTION_EVENT to ESP32 #2
   │   └── Activate zapper for 2 seconds (if enabled)
   ├── HC-SR04 reads height every 1 second
   │   ├── Height > baseline + 5cm → LIFT_ALERT sent
   │   └── OLED shows scared face when lifted
   └── Status updates sent every 2 seconds

3. Web Dashboard
   ├── User connects via browser (desktop/tablet/phone)
   ├── Real-time sensor data via WebSocket (no page refresh)
   ├── Control commands sent to ESP32 #2
   └── ESP32 #2 forwards commands to ESP32 #1 via ESP-NOW

4. Touch Sensor Input
   ├── Touch #1 → Cycle OLED emotions (Happy→Neutral→Angry→Sleeping→Scared)
   ├── Touch #2 → Toggle speaker on/off
   └── Emotions shown on OLED face with animations
```

## Lift Detection System

### How It Works

1. **Calibration**: On boot, HC-SR04 takes 10 readings to establish baseline ground height
2. **Monitoring**: Every 1 second, HC-SR04 reads current height
3. **Detection**: If height increases >5cm from baseline → trap is lifted
4. **Alert**: Sends `MSG_LIFT_ALERT` to ESP32 #2
5. **OLED**: Shows **Scared face** (wide eyes, raised eyebrows, open mouth)
6. **Web Dashboard**: Red pulsing banner "TRAP LIFTED - Tamper Alert Active"
7. **Recovery**: When height returns to normal, clears alert and face returns to happy

### Constants

| Parameter | Value | Description |
|-----------|-------|-------------|
| LIFT_CHECK_INTERVAL_MS | 1000 | Check height every 1 second |
| LIFT_THRESHOLD_CM | 5.0 | Height increase to trigger lift |
| LIFT_CALIBRATION_SAMPLES | 10 | Number of boot readings for baseline |
| LIFT_ALERT_COOLDOWN_MS | 10000 | Minimum time between lift alerts |

## Mosquito Sound Generation

### Speaker + PAM8403 Amplifier

- ESP32 GPIO 12 outputs PWM signal
- PAM8403 amplifier boosts signal to speaker
- Frequency varies between 400-500Hz for realistic mosquito wingbeat
- Volume controlled via PWM duty cycle (0-255)
- Frequency updates every 100ms using sine wave modulation

### Why This Frequency?

- Female mosquito wingbeat: 400-600Hz
- Male mosquito wingbeat: 700-900Hz
- 450Hz base frequency attracts male mosquitoes toward what they think is a female

## OLED Display Pages

| Page | Content |
|------|---------|
| Home | Trap status, face emotion, mode, detection count |
| Trap Status | LED, brightness, zapper, error codes |
| Sensor Status | IR state, height, lift status, HC-SR04 health |
| Detection Counter | Large count display |
| LED Control | State, brightness bar |
| Buzz Control | State, mode |
| Operating Mode | Current mode selector |
| WiFi Status | Connection, RSSI, clients |
| ESP-NOW Status | Peer connection, last message, heap |

## Limitations

### IR Sensor for Mosquito Detection
- Standard IR break-beam sensors detect objects >5mm wide
- Mosquitoes (1-3mm body) may not always trigger detection
- **Improvement**: Use laser/IR photodiode pair focused to narrow beam, or capacitive sensor near entrance

### Blue LED Attractant
- Blue/UV light attracts some mosquito species but is less effective than CO2
- Works as secondary attractant combined with speaker sound
- **Improvement**: Add CO2 generator (yeast + sugar) or UV LED

### HC-SR04 at Trap Dimensions
- HC-SR04 has ~3cm minimum resolution
- Used for ground height monitoring (10-50cm range) where it works well
- Not suitable for detecting individual mosquitoes inside trap
- **Improvement**: Use for container fullness detection at larger ranges

### Two ESP32 Boards
- One ESP32 with dual-core could handle everything
- Two boards provide clean separation and modularity
- Educational value for learning ESP-NOW communication
- **Verdict**: Acceptable for DIY project, not cost-optimal for production
