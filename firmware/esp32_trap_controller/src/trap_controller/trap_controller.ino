/*
 * ESP32 #1 — Trap Controller Firmware
 *
 * Controls: Blue LED, IR Sensor, HC-SR04, Zapper, Speaker (mosquito sound)
 * Communicates with ESP32 #2 via ESP-NOW
 */

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <driver/i2s.h>
#include "mosquito_protocol.h"

// ===== Pin Definitions =====
#define PIN_LED_BLUE      25
#define PIN_IR_SENSOR     26
#define PIN_HCSR04_TRIG   27
#define PIN_HCSR04_ECHO   14
#define PIN_I2S_BCLK      5
#define PIN_I2S_LRC       18
#define PIN_I2S_DIN       19
#define PIN_AMP_ENABLE    22   // MAX98357A SD/EN pin — pull HIGH to enable
#define PIN_ZAPPER_ENABLE 32
#define PIN_STATUS_LED    33
#define PIN_BUILDIN_LED   2

// ===== Constants =====
#define STATUS_SEND_INTERVAL_MS   2000
#define HEARTBEAT_INTERVAL_MS     5000
#define IR_DEBOUNCE_MS            2000
#define HC_SR04_TIMEOUT_US        30000

// Speaker/I2S settings (MAX98357A)
#define I2S_PORT            I2S_NUM_0
#define I2S_SAMPLE_RATE     44100
#define I2S_BUFFER_SIZE     512

// Mosquito sound frequencies (female wingbeat = 400-600Hz)
#define MOSQUITO_FREQ_BASE  450
#define MOSQUITO_FREQ_VAR   50
#define MOSQUITO_VOLUME     8000.0f

// LED PWM settings
#define LED_PWM_FREQ              5000
#define LED_PWM_RESOLUTION        8

// Lift detection
#define LIFT_CHECK_INTERVAL_MS    1000
#define LIFT_CALIBRATION_SAMPLES  10
#define LIFT_THRESHOLD_CM         5.0
#define LIFT_ALERT_COOLDOWN_MS    10000

// ===== Global State =====
struct TrapState {
  bool ledEnabled = true;
  uint8_t ledBrightness = 128;
  bool buzzEnabled = false;
  bool zapperEnabled = true;
  uint8_t mode = MODE_AUTO;
  uint16_t detectionCount = 0;
  uint32_t lastDetectionTime = 0;
  bool irDetected = false;
  float distanceCM = 0;
  uint8_t errorFlags = 0;
  uint32_t lastStatusSend = 0;
  uint32_t lastHeartbeat = 0;
  uint32_t bootTime = 0;
  bool peerConnected = false;
  uint32_t lastPeerResponse = 0;

  // Lift detection
  float baselineHeightCM = 0;
  bool liftCalibrated = false;
  bool isLifted = false;
  uint32_t lastLiftAlert = 0;
  uint32_t lastLiftCheck = 0;

  // Speaker audio
  uint8_t speakerVolume = 180;
  uint32_t lastFreqUpdate = 0;
  float currentFreq = MOSQUITO_FREQ_BASE;
  float speakerPhase = 0.0f;
} state;

// Gateway MAC address
uint8_t gatewayMAC[] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x02};
// This board's own MAC
uint8_t myMAC[] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x01};

// ===== Function Prototypes =====
void initESPNow();
void initSensors();
void initActuators();
void readIRSensor();
void readHCSR04();
void controlLED();
void controlSpeaker();
void testSpeaker();
void controlZapper();
void sendStatusUpdate();
void sendHeartbeat();
void sendDetectionEvent();
void sendLiftAlert();
void checkLiftState();
void onDataRecv(const esp_now_recv_info *info, const uint8_t *data, int len);
void onDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status);
void processCommand(const uint8_t *data, int len);
void printTrapStatus();

// ===== Setup =====
void setup() {
  Serial.begin(115200);
  Serial.println("\n=== Mosquito Trap Controller Starting ===");

  state.bootTime = millis();

  // Set custom MAC address
  WiFi.mode(WIFI_STA);
  esp_wifi_set_mac(WIFI_IF_STA, myMAC);
  Serial.print("Trap MAC set to: ");
  Serial.println(WiFi.macAddress());
  WiFi.disconnect();

  initSensors();
  initActuators();
  initESPNow();

  Serial.println("=== Trap Controller Ready ===\n");
}

// ===== Main Loop =====
void loop() {
  uint32_t now = millis();

  // Read sensors continuously
  readIRSensor();

  // HC-SR04 at reduced rate
  static uint32_t lastHCSR04Read = 0;
  if (now - lastHCSR04Read >= 5000) {
    readHCSR04();
    lastHCSR04Read = now;
  }

  // Lift detection check
  if (now - state.lastLiftCheck >= LIFT_CHECK_INTERVAL_MS) {
    checkLiftState();
    state.lastLiftCheck = now;
  }

  // Automatic mode logic
  if (state.mode == MODE_AUTO || state.mode == MODE_SILENT) {
    controlLED();
    if (state.mode == MODE_AUTO) {
      controlSpeaker();
    }
    controlZapper();
  }

  // Status LED heartbeat
  digitalWrite(PIN_STATUS_LED, (now / 1000) % 2);

  // Check peer connection
  if (state.peerConnected && (now - state.lastPeerResponse > 15000)) {
    state.peerConnected = false;
    state.errorFlags |= ERR_COMM_ERROR;
  }

  // Send periodic status
  if (now - state.lastStatusSend >= STATUS_SEND_INTERVAL_MS) {
    sendStatusUpdate();
    state.lastStatusSend = now;
  }

  // Send heartbeat
  if (now - state.lastHeartbeat >= HEARTBEAT_INTERVAL_MS) {
    sendHeartbeat();
    state.lastHeartbeat = now;
  }

  delay(10);
}

// ===== Initialization =====
void initSensors() {
  pinMode(PIN_IR_SENSOR, INPUT);
  pinMode(PIN_HCSR04_TRIG, OUTPUT);
  pinMode(PIN_HCSR04_ECHO, INPUT);
  digitalWrite(PIN_HCSR04_TRIG, LOW);

  // Calibrate baseline height (average of 10 readings)
  Serial.print("Calibrating height baseline...");
  float sum = 0;
  int validReadings = 0;
  for (int i = 0; i < LIFT_CALIBRATION_SAMPLES; i++) {
    digitalWrite(PIN_HCSR04_TRIG, LOW);
    delayMicroseconds(2);
    digitalWrite(PIN_HCSR04_TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(PIN_HCSR04_TRIG, LOW);

    long duration = pulseIn(PIN_HCSR04_ECHO, HIGH, HC_SR04_TIMEOUT_US);
    if (duration > 0) {
      sum += (duration * 0.034) / 2.0;
      validReadings++;
    }
    delay(100);
  }

  if (validReadings >= 5) {
    state.baselineHeightCM = sum / validReadings;
    state.liftCalibrated = true;
    Serial.print(" OK: ");
    Serial.print(state.baselineHeightCM);
    Serial.println(" cm");
  } else {
    state.baselineHeightCM = 0;
    state.liftCalibrated = false;
    Serial.println(" FAILED - using 0 baseline");
  }

  Serial.println("Sensors initialized");
}

void initActuators() {
  // Blue LED PWM — ESP32 Core 3.x: ledcAttach replaces ledcSetup+ledcAttachPin
  ledcAttach(PIN_LED_BLUE, LED_PWM_FREQ, LED_PWM_RESOLUTION);
  ledcWrite(PIN_LED_BLUE, 0);

  // =========================
  // MAX98357A I2S SPEAKER
  // =========================

  // Enable amplifier (SD/EN pin must be HIGH)
  pinMode(PIN_AMP_ENABLE, OUTPUT);
  digitalWrite(PIN_AMP_ENABLE, HIGH);
  delay(50);  // Allow amp to power up
  Serial.println("Amplifier enabled (SD/EN HIGH)");

  i2s_config_t i2s_config = {};
  i2s_config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
  i2s_config.sample_rate = I2S_SAMPLE_RATE;
  i2s_config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  i2s_config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  i2s_config.communication_format = I2S_COMM_FORMAT_STAND_MSB;
  i2s_config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  i2s_config.dma_buf_count = 8;
  i2s_config.dma_buf_len = 512;
  i2s_config.use_apll = false;
  i2s_config.tx_desc_auto_clear = true;
  i2s_config.fixed_mclk = 0;

  i2s_pin_config_t pin_config = {};
  pin_config.bck_io_num = PIN_I2S_BCLK;
  pin_config.ws_io_num = PIN_I2S_LRC;
  pin_config.data_out_num = PIN_I2S_DIN;
  pin_config.data_in_num = I2S_PIN_NO_CHANGE;

  esp_err_t result;

  result = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  if (result != ESP_OK) {
    Serial.print("I2S driver install FAILED: ");
    Serial.println(result);
  } else {
    Serial.println("I2S driver installed");
  }

  result = i2s_set_pin(I2S_PORT, &pin_config);
  if (result != ESP_OK) {
    Serial.print("I2S pin setup FAILED: ");
    Serial.println(result);
  } else {
    Serial.println("I2S pins configured");
  }

  i2s_zero_dma_buffer(I2S_PORT);

  // =========================
  // ZAPPER
  // =========================

  pinMode(PIN_ZAPPER_ENABLE, OUTPUT);
  digitalWrite(PIN_ZAPPER_ENABLE, LOW);

  // Status LEDs
  pinMode(PIN_STATUS_LED, OUTPUT);
  pinMode(PIN_BUILDIN_LED, OUTPUT);

  Serial.println("Actuators initialized");
}

void initESPNow() {
  Serial.print("Trap MAC: ");
  Serial.println(WiFi.macAddress());

  if (esp_now_init() != ESP_OK) {
    Serial.println("ERROR: ESP-NOW init failed");
    state.errorFlags |= ERR_COMM_ERROR;
    return;
  }

  // ESP32 Core 3.x callback signatures
  esp_now_register_recv_cb(onDataRecv);
  esp_now_register_send_cb(onDataSent);

  // Add gateway peer
  esp_now_peer_info_t peerInfo;
  memset(&peerInfo, 0, sizeof(peerInfo));
  memcpy(peerInfo.peer_addr, gatewayMAC, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("ERROR: Failed to add peer");
  } else {
    Serial.println("ESP-NOW initialized, peer added");
  }
}

// ===== Sensor Reading =====
void readIRSensor() {
  bool detected = digitalRead(PIN_IR_SENSOR) == LOW;

  if (detected && !state.irDetected) {
    uint32_t now = millis();
    if (now - state.lastDetectionTime > IR_DEBOUNCE_MS) {
      state.irDetected = true;
      state.detectionCount++;
      state.lastDetectionTime = now;

      Serial.print("DETECTION #");
      Serial.println(state.detectionCount);

      sendDetectionEvent();
    }
  } else if (!detected) {
    state.irDetected = false;
  }
}

void readHCSR04() {
  digitalWrite(PIN_HCSR04_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_HCSR04_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_HCSR04_TRIG, LOW);

  long duration = pulseIn(PIN_HCSR04_ECHO, HIGH, HC_SR04_TIMEOUT_US);

  if (duration == 0) {
    state.errorFlags |= ERR_HCSR04_TIMEOUT;
    state.distanceCM = 0;
  } else {
    state.errorFlags &= ~ERR_HCSR04_TIMEOUT;
    state.distanceCM = (duration * 0.034) / 2.0;
  }
}

// ===== Actuator Control =====
void controlLED() {
  if (state.ledEnabled) {
    ledcWrite(PIN_LED_BLUE, state.ledBrightness);
  } else {
    ledcWrite(PIN_LED_BLUE, 0);
  }
}

void controlSpeaker() {
  static int16_t samples[I2S_BUFFER_SIZE];

  // Speaker OFF
  if (!state.buzzEnabled || state.mode == MODE_SILENT) {
    memset(samples, 0, sizeof(samples));
    size_t bytes_written = 0;
    i2s_write(I2S_PORT, samples, sizeof(samples), &bytes_written, 0);
    return;
  }

  uint32_t now = millis();

  // Slowly vary mosquito frequency
  if (now - state.lastFreqUpdate >= 100) {
    state.currentFreq = MOSQUITO_FREQ_BASE +
      (sin((float)now * 0.003f) * MOSQUITO_FREQ_VAR);
    state.lastFreqUpdate = now;
  }

  // Convert 0-255 volume to amplitude
  float amplitude = ((float)state.speakerVolume / 255.0f) * MOSQUITO_VOLUME;

  // Generate continuous stereo sine wave
  for (int i = 0; i < I2S_BUFFER_SIZE; i += 2) {
    float sample = sinf(state.speakerPhase) * amplitude;
    int16_t audioSample = (int16_t)constrain(sample, -32767.0f, 32767.0f);

    samples[i] = audioSample;     // left
    samples[i + 1] = audioSample; // right

    // Advance phase
    state.speakerPhase += 2.0f * PI * state.currentFreq / (float)I2S_SAMPLE_RATE;
    if (state.speakerPhase >= 2.0f * PI) {
      state.speakerPhase -= 2.0f * PI;
    }
  }

  // Continuously feed I2S
  size_t bytes_written = 0;
  esp_err_t err = i2s_write(I2S_PORT, samples, sizeof(samples), &bytes_written, 0);
  if (err != ESP_OK) {
    Serial.printf("I2S write error: %d\n", err);
  }
}

// ===== Speaker Test =====
void testSpeaker() {
  Serial.println("=== SPEAKER TEST ===");

  // Generate 1kHz test tone
  const int testSamples = 512;
  int16_t audioBuffer[testSamples];
  for (int i = 0; i < testSamples; i++) {
    audioBuffer[i] = (int16_t)(sin(2.0 * PI * 1000.0 * i / 44100.0) * 20000);
  }

  // Play for 2 seconds
  for (int repeat = 0; repeat < 100; repeat++) {
    size_t bytesWritten;
    i2s_write(I2S_PORT, audioBuffer, sizeof(audioBuffer), &bytesWritten, portMAX_DELAY);
    if (bytesWritten != sizeof(audioBuffer)) {
      Serial.printf("Only wrote %d bytes\n", bytesWritten);
    }
    delay(10);
  }

  Serial.println("=== TEST COMPLETE ===");
}

void controlZapper() {
  if (state.zapperEnabled && state.irDetected) {
    digitalWrite(PIN_ZAPPER_ENABLE, HIGH);
    delay(2000);
    digitalWrite(PIN_ZAPPER_ENABLE, LOW);
  }
}

// ===== ESP-NOW Communication =====
void onDataRecv(const esp_now_recv_info *info, const uint8_t *data, int len) {
  if (len < 1) return;

  uint8_t msgType = data[0];

  if (msgType == MSG_COMMAND || msgType == MSG_CONFIG_UPDATE) {
    if (len == sizeof(CommandMessage)) {
      processCommand(data, len);
    }
  } else if (msgType == MSG_HEARTBEAT || msgType == MSG_ACK) {
    state.peerConnected = true;
    state.lastPeerResponse = millis();
    state.errorFlags &= ~ERR_COMM_ERROR;
  }
}

void onDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
  if (status != ESP_NOW_SEND_SUCCESS) {
    Serial.println("ESP-NOW send failed");
    state.errorFlags |= ERR_COMM_ERROR;
  }
}

void processCommand(const uint8_t *data, int len) {
  const CommandMessage *cmd = (const CommandMessage *)data;

  if (!cmd->validateChecksum()) {
    Serial.println("Bad checksum on command");
    return;
  }

  state.peerConnected = true;
  state.lastPeerResponse = millis();
  state.errorFlags &= ~ERR_COMM_ERROR;

  switch (cmd->command) {
    case CMD_LED_ON:
      state.ledEnabled = true;
      Serial.println("CMD: LED ON");
      break;
    case CMD_LED_OFF:
      state.ledEnabled = false;
      Serial.println("CMD: LED OFF");
      break;
    case CMD_LED_BRIGHTNESS:
      state.ledBrightness = cmd->param;
      Serial.print("CMD: Brightness = ");
      Serial.println(cmd->param);
      break;
    case CMD_BUZZ_ON:
      state.buzzEnabled = true;
      Serial.println("CMD: Speaker ON");
      break;
    case CMD_BUZZ_OFF:
      state.buzzEnabled = false;
      Serial.println("CMD: Speaker OFF");
      break;
    case CMD_ZAPPER_ON:
      state.zapperEnabled = true;
      Serial.println("CMD: Zapper ON");
      break;
    case CMD_ZAPPER_OFF:
      state.zapperEnabled = false;
      Serial.println("CMD: Zapper OFF");
      break;
    case CMD_SET_MODE:
      state.mode = cmd->param;
      Serial.print("CMD: Mode = ");
      Serial.println(cmd->param);
      break;
    case CMD_RESET_COUNTER:
      state.detectionCount = 0;
      Serial.println("CMD: Counter reset");
      break;
    case CMD_RESTART:
      Serial.println("CMD: Restart");
      ESP.restart();
      break;
    case CMD_SET_VOLUME:
      state.speakerVolume = cmd->param;
      Serial.print("CMD: Volume = ");
      Serial.println(cmd->param);
      break;
  }

  // Send ACK
  CommandMessage ack;
  ack.msgType = MSG_ACK;
  ack.deviceId = DEVICE_ID_TRAP;
  ack.command = cmd->command;
  ack.param = 0;
  memset(ack.reserved, 0, sizeof(ack.reserved));
  ack.calculateChecksum();

  esp_now_send(gatewayMAC, (uint8_t *)&ack, sizeof(ack));
}

void sendStatusUpdate() {
  TrapMessage msg;
  msg.msgType = MSG_STATUS_UPDATE;
  msg.deviceId = DEVICE_ID_TRAP;
  msg.timestamp = millis() / 1000;
  msg.ledState = state.ledEnabled ? 1 : 0;
  msg.ledBrightness = state.ledBrightness;
  msg.buzzState = state.buzzEnabled ? 1 : 0;
  msg.irState = state.irDetected ? 1 : 0;
  msg.distanceCM = (uint8_t)constrain(state.distanceCM, 0, 255);
  msg.operatingMode = state.mode;
  msg.detectionCount = state.detectionCount;
  msg.errorFlags = state.errorFlags;
  msg.calculateChecksum();

  esp_now_send(gatewayMAC, (uint8_t *)&msg, sizeof(msg));
}

void sendHeartbeat() {
  TrapMessage msg;
  msg.msgType = MSG_HEARTBEAT;
  msg.deviceId = DEVICE_ID_TRAP;
  msg.timestamp = millis() / 1000;
  msg.ledState = state.ledEnabled ? 1 : 0;
  msg.ledBrightness = state.ledBrightness;
  msg.buzzState = state.buzzEnabled ? 1 : 0;
  msg.irState = state.irDetected ? 1 : 0;
  msg.distanceCM = (uint8_t)constrain(state.distanceCM, 0, 255);
  msg.operatingMode = state.mode;
  msg.detectionCount = state.detectionCount;
  msg.errorFlags = state.errorFlags;
  msg.calculateChecksum();

  esp_now_send(gatewayMAC, (uint8_t *)&msg, sizeof(msg));
}

void sendDetectionEvent() {
  TrapMessage msg;
  msg.msgType = MSG_DETECTION_EVENT;
  msg.deviceId = DEVICE_ID_TRAP;
  msg.timestamp = millis() / 1000;
  msg.ledState = state.ledEnabled ? 1 : 0;
  msg.ledBrightness = state.ledBrightness;
  msg.buzzState = state.buzzEnabled ? 1 : 0;
  msg.irState = 1;
  msg.distanceCM = (uint8_t)constrain(state.distanceCM, 0, 255);
  msg.operatingMode = state.mode;
  msg.detectionCount = state.detectionCount;
  msg.errorFlags = state.errorFlags;
  msg.calculateChecksum();

  esp_now_send(gatewayMAC, (uint8_t *)&msg, sizeof(msg));
}

// ===== Lift Detection =====
void checkLiftState() {
  if (!state.liftCalibrated || state.distanceCM <= 0) return;

  float heightIncrease = state.distanceCM - state.baselineHeightCM;
  bool wasLifted = state.isLifted;

  if (heightIncrease > LIFT_THRESHOLD_CM) {
    state.isLifted = true;
    state.errorFlags |= ERR_LIFTED;

    if (!wasLifted || (millis() - state.lastLiftAlert > LIFT_ALERT_COOLDOWN_MS)) {
      Serial.print("LIFT DETECTED! Height: ");
      Serial.print(state.distanceCM);
      Serial.print("cm (baseline: ");
      Serial.print(state.baselineHeightCM);
      Serial.println("cm)");

      sendLiftAlert();
      state.lastLiftAlert = millis();
    }
  } else if (heightIncrease < (LIFT_THRESHOLD_CM * 0.5)) {
    state.isLifted = false;
    state.errorFlags &= ~ERR_LIFTED;
  }
}

void sendLiftAlert() {
  TrapMessage msg;
  msg.msgType = MSG_LIFT_ALERT;
  msg.deviceId = DEVICE_ID_TRAP;
  msg.timestamp = millis() / 1000;
  msg.ledState = state.ledEnabled ? 1 : 0;
  msg.ledBrightness = state.ledBrightness;
  msg.buzzState = state.buzzEnabled ? 1 : 0;
  msg.irState = state.irDetected ? 1 : 0;
  msg.distanceCM = (uint8_t)constrain(state.distanceCM, 0, 255);
  msg.operatingMode = state.mode;
  msg.detectionCount = state.detectionCount;
  msg.errorFlags = state.errorFlags;
  msg.calculateChecksum();

  esp_now_send(gatewayMAC, (uint8_t *)&msg, sizeof(msg));
}

void printTrapStatus() {
  Serial.println("--- Trap Status ---");
  Serial.print("LED: "); Serial.println(state.ledEnabled ? "ON" : "OFF");
  Serial.print("Brightness: "); Serial.println(state.ledBrightness);
  Serial.print("Speaker: "); Serial.println(state.buzzEnabled ? "ON" : "OFF");
  Serial.print("Zapper: "); Serial.println(state.zapperEnabled ? "ON" : "OFF");
  Serial.print("Mode: "); Serial.println(state.mode);
  Serial.print("IR: "); Serial.println(state.irDetected ? "DETECTED" : "clear");
  Serial.print("Distance: "); Serial.print(state.distanceCM); Serial.println(" cm");
  Serial.print("Detections: "); Serial.println(state.detectionCount);
  Serial.print("Errors: 0x"); Serial.println(state.errorFlags, HEX);
  Serial.print("Peer: "); Serial.println(state.peerConnected ? "connected" : "LOST");
  Serial.println("-------------------");
}
