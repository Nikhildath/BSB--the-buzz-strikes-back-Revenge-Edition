# Testing Procedures

## Test 1: ESP-NOW Communication

**Setup:** Both ESP32s powered via USB, serial monitors open

**Procedure:**
1. Power on ESP32 #1 → verify "Trap MAC set to: 02:00:00:00:00:01"
2. Power on ESP32 #2 → verify "Gateway MAC set to: 02:00:00:00:00:02"
3. Verify "ESP-NOW initialized" on both
4. Within 5s, verify "Peer connected" on trap serial
5. Verify status messages appear every 2s on gateway serial
6. Verify heartbeat every 5s

**Pass:** 100+ messages exchanged without "Bad checksum" errors

---

## Test 2: IR Detection

**Setup:** IR sensor on GPIO 26

**Procedure:**
1. Wave hand over IR sensor
2. Verify "DETECTION #1" on serial
3. Wait 2 seconds, wave again → verify "DETECTION #2"
4. Wave rapidly → verify debounce prevents multiples
5. Check detection count on web dashboard

**Pass:** 0 false triggers, count matches manual count

---

## Test 3: HC-SR04 Height Measurement

**Setup:** HC-SR04 with voltage divider on echo pin

**Procedure:**
1. Place trap on flat surface
2. Verify baseline calibration on serial ("Calibrating... OK: XX cm")
3. Check distance reading updates every 5s
4. Lift trap 10cm → verify distance increases
5. Verify "LIFTED!" appears on serial and web dashboard

**Pass:** Readings within ±2cm, lift detection triggers at >5cm increase

---

## Test 4: Blue LED Control

**Setup:** LED array via MOSFET on GPIO 25

**Procedure:**
1. Click LED ON on web dashboard → verify LED illuminates
2. Click LED OFF → verify LED off
3. Adjust brightness slider 0→255 → verify smooth dimming
4. Test brightness at 0, 64, 128, 192, 255

**Pass:** All levels correct, no flicker

---

## Test 5: Speaker/Audio System

**Setup:** Speaker + MAX98357A amplifier on GPIO 5/18/19/22

**Procedure:**
1. Click Buzz ON → verify mosquito buzzing sound
2. Verify frequency varies slightly (realistic sound)
3. Click Buzz OFF → verify silence
4. Test touch sensor 2 toggle
5. Adjust volume slider → verify volume changes
6. Verify Silent mode disables speaker
7. Verify Auto mode enables speaker with detection

**Pass:** Sound audible, volume control works

---

## Test 6: Web Dashboard

**Setup:** ESP32 #2 running, browser on same network

**Procedure:**
1. Connect to "MosquitoTrap" WiFi
2. Open `http://192.168.4.1`
3. Verify dashboard loads with all cards
4. Verify real-time updates (no page refresh)
5. Click LED ON/OFF → verify physical response
6. Click Buzz ON/OFF → verify speaker response
7. Adjust brightness and volume sliders
8. Change operating mode (Auto/Manual/Schedule/Silent)
9. Reset detection counter
10. Check WebSocket "Connected" badge
11. Test on mobile viewport

**Pass:** All controls work, responsive on mobile

---

## Test 7: OLED Touchscreen

**Setup:** OLED on I2C, touch sensors on GPIO 32/33

**Procedure:**
1. Verify OLED shows home page with face
2. Press touch 1 → verify emotion cycles: Happy→Neutral→Angry→Sleeping→Scared
3. Verify face animation changes for each emotion
4. Press touch 2 → verify speaker toggles on/off
5. Navigate through all OLED pages
6. Verify status info updates in real-time

**Pass:** All 5 emotions display, touch responsive <300ms

---

## Test 8: Lift Detection

**Setup:** Complete system running

**Procedure:**
1. Verify trap sitting on surface (baseline calibrated)
2. Lift trap 10cm off surface
3. Verify OLED shows **Scared face** (wide eyes, open mouth)
4. Verify web dashboard shows red "TRAP LIFTED" banner
5. Verify lift alert in detection log
6. Place trap back on surface
7. Verify face returns to Happy
8. Verify lift banner disappears

**Pass:** Lift detected within 1 second, clears when returned

---

## Test 9: Communication Failure Recovery

**Setup:** Both boards running

**Procedure:**
1. Power off ESP32 #1
2. Verify ESP32 #2 shows "OFFLINE" within 10 seconds
3. Verify "Communication Error" flag on web
4. Power on ESP32 #1
5. Verify communication restores automatically
6. Verify error flag clears
7. Repeat 5 times

**Pass:** 100% recovery rate, no manual reset needed

---

## Test 10: Power Failure/Restart

**Setup:** Complete system running

**Procedure:**
1. Power off both boards
2. Power on both
3. Verify both initialize (check serial)
4. Verify ESP-NOW connection establishes
5. Verify web dashboard reconnects
6. Verify HC-SR04 recalibrates baseline

**Pass:** System fully operational after restart

---

## Test 11: Safety Systems

**Setup:** Zapper module connected

**Procedure:**
1. Verify zapper DISABLED by default
2. Arm zapper via web dashboard
3. Trigger IR sensor → verify zapper activates for 2s
4. Disarm zapper → verify no activation
5. Check HV section is physically isolated
6. Verify fuse intact

**Pass:** Zero unintended activations

---

## Troubleshooting

| Symptom | Likely Cause | Solution |
|---------|-------------|----------|
| No ESP-NOW | MAC not set | Check serial for MAC output |
| Bad checksum | Data corruption | Check wiring, restart both boards |
| OLED blank | I2C wrong | Check address 0x3C, SDA/SCL swapped |
| HC-SR04 timeout | Divider wrong | Check 10kΩ/15kΩ divider on echo |
| LED flickers | PWM too low | Already set to 5000Hz |
| Web won't load | Wrong IP | Check serial for AP IP (192.168.4.1) |
| Speaker silent | Amp not powered | Check 5V to MAX98357A VIN |
| Speaker silent | Wrong GPIO | Check GPIO 5/18/19 wiring |
| Speaker distorted | Volume too high | Reduce volume below 200 |
| Lift not detecting | Threshold too high | Check LIFT_THRESHOLD_CM (5.0) |
| Lift always on | Baseline bad | Restart to recalibrate |
| Zapper always on | Floating gate | Add 10kΩ pull-down on gate |

---

## Quick Verification Script

Check serial output for this sequence:
```
=== Mosquito Trap Controller Starting ===
Trap MAC set to: 02:00:00:00:00:01
Calibrating height baseline... OK: XX cm
Sensors initialized
Actuators initialized (speaker mode)
ESP-NOW initialized, peer added
=== Trap Controller Ready ===
```

And on gateway:
```
=== Mosquito Trap UI Gateway Starting ===
Gateway MAC set to: 02:00:00:00:00:02
OLED initialized
AP IP: 192.168.4.1
ESP-NOW initialized
Web server started on port 80
WebSocket started on port 81
=== UI Gateway Ready ===
```
