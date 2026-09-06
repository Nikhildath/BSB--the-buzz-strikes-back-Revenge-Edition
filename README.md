<img width="1280" height="640" alt="BSB Banner" src="https://github.com/user-attachments/assets/8920b256-2ba8-4988-b824-5351134eb4bd" />



# BSB - Buzz Strikes Back


## Basic Details
### Team Name: BSB


### Team Members
- Team Lead: Nikhil Dath
- Member 2: Ajay Krishna KS

### Project Description
A fully automated mosquito trap that uses two ESP32 boards communicating via ESP-NOW. It lures mosquitoes with blue LED light, detects them with an IR sensor, plays back their own annoying buzzing sound through a speaker, and zaps them with a high-voltage grid. An OLED display shows animated emotic faces that react to trap events, and a real-time web dashboard lets you monitor everything from your phone.

### The Problem (that doesn't exist)
Mosquitoes go around all day making that annoying buzzing sound, but they never have to listen to it themselves. This is deeply unfair. How are they supposed to know how annoying they are if they never experience it from the other side?

### The Solution (that nobody asked for)
We built a trap that records the mosquito's own buzzing frequency (400-600Hz female wingbeat) and blasts it back at them through a speaker with a MAX98357A amplifier. The trap lures them in with blue LED light, detects them with an IR sensor, plays their own annoying sound back at them, and then zaps them with a high-voltage grid. The OLED display even shows emotic faces that get scared when someone lifts the trap. Science has gone too far.


## Technical Details
### Technologies/Components Used
For Software:
- Languages used: C++ (Arduino/ESP-IDF), HTML, CSS, JavaScript
- Frameworks used: Arduino Core for ESP32 (v3.3.11), PlatformIO
- Libraries used: ESP-NOW, WiFi, WebSockets, U8g2 (OLED), ESP32-Eyes (emotic display), I2S (audio)
- Tools used: PlatformIO, Arduino IDE, Git

For Hardware:
- ESP32 DevKit V1 (WROOM-32) x2
- OLED Display 0.96" I2C 128x64 (SSD1306) with U8g2 library
- MAX98357A I2S Amplifier Module (3W)
- Speaker 4-ohm 3W
- IR Break-Beam Sensor (FC-03)
- HC-SR04 Ultrasonic Sensor
- Blue LED 470nm (x4)
- IRLZ44N Logic-Level MOSFET
- IRF540N N-Channel MOSFET
- TTP223 Capacitive Touch Sensors (x2)
- HV Boost Converter Module
- 12V DC Power Adapter (2A)
- LM2596 Buck Converter (12V to 5V)
- AMS1117-3.3V LDO (5V to 3.3V)

### Implementation
For Software:
# Installation
```bash
# Clone the repository
git clone https://github.com/Nikhildath/BSB--the-buzz-strikes-back-Revenge-Edition.git
cd BSB--the-buzz-strikes-back-Revenge-Edition

# Copy protocol header to both firmware directories
cp firmware/common/mosquito_protocol.h firmware/esp32_trap_controller/src/
cp firmware/common/mosquito_protocol.h firmware/esp32_ui_gateway/src/

# Install PlatformIO (if not already installed)
pip install platformio
```

# Run
```bash
# Upload to ESP32 #1 (Trap Controller)
cd firmware/esp32_trap_controller
pio run -t upload

# Upload to ESP32 #2 (UI Gateway)
cd firmware/esp32_ui_gateway
pio run -t upload

# Or use Arduino IDE:
# Open firmware/esp32_ui_gateway/src/ui_gateway/ui_gateway.ino
# Select board: ESP32 Dev Module
# Upload
```

### Project Documentation
For Software:

# Screenshots (Add at least 3)
![Screenshot1](Add screenshot 1 here with proper name)
*Web dashboard showing real-time trap status, emotion face, and controls*

![Screenshot2](Add screenshot 2 here with proper name)
*OLED display showing animated emotic eyes (Happy face)*

![Screenshot3](Add screenshot 3 here with proper name)
*Serial Monitor showing distance readings and lift detection*

# Diagrams
![Workflow](Add your workflow/architecture diagram here)
*System architecture showing ESP32 #1 (Trap) communicating with ESP32 #2 (Gateway) via ESP-NOW, with sensor inputs, actuator outputs, and web dashboard*

For Hardware:

# Schematic & Circuit
![Circuit](Add your circuit diagram here)
*Complete wiring diagram showing ESP32 connections to all sensors, amplifier, MOSFETs, and OLED*

![Schematic](Add your schematic diagram here)
*Schematic showing power distribution (12V to 5V to 3.3V) and signal routing*

# Build Photos
![Build](WhatsApp%20Image%202026-09-06%20at%2012.35.54%20AM.jpeg)
*Breadboard prototype: ESP32 with OLED display showing emotic eyes, blue LED attractant, HC-SR04 ultrasonic sensor, and MAX98357A amplifier — all wired up and running*

![Team](WhatsApp%20Image%202026-09-06%20at%2012.35.55%20AM.jpeg)
*Team BSB at work: Nikhil and Ajay debugging firmware with multiple laptops, breadboard circuits, and the trap hardware spread across the desk — chaos in its purest form*

### Project Demo
# Video
https://github.com/Nikhildath/BSB--the-buzz-strikes-back-Revenge-Edition/raw/main/WhatsApp%20Video%202026-09-06%20at%206.43.47%20AM.mp4

*Video demonstrating the trap in action: blue LED luring, mosquito detection, speaker buzzing, zapper firing, OLED face reactions, and web dashboard monitoring*

# Additional Demos
- Web dashboard accessible at `http://192.168.4.1` after connecting to `MosquitoTrap` WiFi (password: `trap1234`)
- Live WebSocket updates showing sensor readings in real-time

## Team Contributions
- Nikhil Dath: System architecture, ESP-NOW protocol design, firmware development for both ESP32 boards, web dashboard, ESP32-Eyes emotic display integration, I2S audio system, documentation
- Ajay Krishna KS: Hardware assembly, circuit design, sensor integration, testing and debugging


---
Made with &#10084;&#65039; at TinkerHub Useless Projects 

![Static Badge](https://img.shields.io/badge/TinkerHub-24?color=%23000000&link=https%3A%2F%2Fwww.tinkerhub.org%2F)
![Static Badge](https://img.shields.io/badge/UselessProjects--26-26?link=https%3A%2F%2Ftinkerhub.org%2Fevents%2F1M8ORET9A1%2Fuseless-projects-3.0)
