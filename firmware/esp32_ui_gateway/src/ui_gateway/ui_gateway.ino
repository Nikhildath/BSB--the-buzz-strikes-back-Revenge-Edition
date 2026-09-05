/*
 * ESP32 #2 — UI Gateway Firmware
 * 
 * Controls: OLED Emotic Eyes Display, Web Server, ESP-NOW Gateway
 * Receives trap data and provides web dashboard + animated OLED eyes
 *
 * ╔══════════════════════════════════════════════════════════╗
 * ║  WIRING — ESP32 UI Gateway                              ║
 * ╠══════════════════════════════════════════════════════════╣
 * ║  Component          ESP32 Pin   Notes                    ║
 * ║  ─────────────────  ──────────  ───────────────────────  ║
 * ║  OLED SDA           GPIO 21     I2C Data                 ║
 * ║  OLED SCL           GPIO 22     I2C Clock                ║
 * ║  OLED VCC           3.3V                                   ║
 * ║  OLED GND           GND                                     ║
 * ║                                                          ║
 * ║  Touch: Emotion     GPIO 32     Capacitive touch pad     ║
 * ║  Touch: Buzz        GPIO 33     Capacitive touch pad     ║
 * ║                                                          ║
 * ║  Built-in LED       GPIO 2      Onboard status LED       ║
 * ║                                                          ║
 * ║  ESP-NOW            Wireless    To ESP32 #1 Trap Ctrl    ║
 * ║  WiFi AP            Wireless    SSID: MosquitoTrap       ║
 * ╚══════════════════════════════════════════════════════════╝
 *
 * Emotions mapped to ESP32-Eyes presets:
 *   0=Happy, 1=Neutral, 2=Angry, 3=Sleepy, 4=Scared
 */

#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <esp_now.h>
#include <Wire.h>
#include "Face.h"
#include "mosquito_protocol.h"

// ===== Pin Definitions =====
#define PIN_TOUCH_EMOTION  32
#define PIN_TOUCH_BUZZ     33
#define PIN_BUILDIN_LED    2

// ===== WiFi Configuration =====
const char* AP_SSID = "MosquitoTrap";
const char* AP_PASS = "trap1234";
const char* STASSID = "YOUR_WIFI_SSID";  // Change this
const char* STAPASS = "YOUR_WIFI_PASS";  // Change this

// ===== Web Server =====
WebServer server(80);
WebSocketsServer webSocket(81);

// ===== Constants =====
#define HEARTBEAT_TIMEOUT_MS   10000
#define FACE_UPDATE_INTERVAL   100
#define OLED_PAGE_INTERVAL     500
#define TOUCH_DEBOUNCE_MS      300

// ===== Face / Emotion =====
Face *face;
eEmotions currentEmotion = eEmotions::Normal;

// ===== Global State =====
struct GatewayState {
  bool trapOnline = false;
  uint32_t lastTrapMessage = 0;
  bool ledState = false;
  uint8_t ledBrightness = 128;
  bool buzzState = false;
  bool irDetected = false;
  float distanceCM = 0;
  uint8_t operatingMode = 0;
  uint16_t detectionCount = 0;
  uint8_t errorFlags = 0;
  uint32_t trapTimestamp = 0;

  uint32_t lastOledUpdate = 0;
  uint32_t lastFaceUpdate = 0;

  bool touchEmotionState = false;
  bool touchBuzzState = false;
  uint32_t lastTouchEmotion = 0;
  uint32_t lastTouchBuzz = 0;

  uint8_t currentEmotionIdx = 0; // 0=Happy,1=Neutral,2=Angry,3=Sleepy,4=Scared
  const char* emotionNames[5] = {"Happy", "Neutral", "Angry", "Sleepy", "Scared"};
  bool isLifted = false;

  bool wifiConnected = false;
  bool apMode = true;
  int rssi = 0;
  uint32_t connectedClients = 0;
} state;

// Trap MAC address
uint8_t trapMAC[] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x01};
uint8_t myMAC[] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x02};

// ===== Function Prototypes =====
void initFace();
void initWiFi();
void initESPNow();
void initWebServer();
void initWebSocket();
void onWiFiEvent(WiFiEvent_t event);
void handleRoot();
void handleAPI();
void handleData();
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length);
void onDataRecv(const esp_now_recv_info *info, const uint8_t *data, int len);
void onDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status);
void sendCommand(uint8_t cmd, uint8_t param);
void setEmotion(uint8_t emotionIdx);
void handleTouchSensors();
void broadcastToWebSocket();
void processEmotionChange();
void processBuzzToggle();
void handleCommand();

// ===== Setup =====
void setup() {
  Serial.begin(115200);
  Serial.println("\n=== Mosquito Trap UI Gateway Starting ===");

  initFace();
  initWiFi();

  esp_wifi_set_mac(WIFI_IF_STA, myMAC);
  Serial.print("Gateway MAC set to: ");
  Serial.println(WiFi.macAddress());

  initESPNow();
  initWebServer();
  initWebSocket();

  pinMode(PIN_TOUCH_EMOTION, INPUT);
  pinMode(PIN_TOUCH_BUZZ, INPUT);
  pinMode(PIN_BUILDIN_LED, OUTPUT);

  // Start with Happy eyes
  face->Expression.GoTo_Happy();
  currentEmotion = eEmotions::Happy;

  // Print web server URLs
  Serial.println("=== Web Server Auto-Started ===");
  Serial.println("AP:      http://" + WiFi.softAPIP().toString());
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("STA:     http://" + WiFi.localIP().toString());
  }
  Serial.println("WebSocket: ws://" + WiFi.softAPIP().toString() + ":81");
  Serial.println("=== UI Gateway Ready ===\n");
}

// ===== Main Loop =====
void loop() {
  uint32_t now = millis();

  server.handleClient();
  webSocket.loop();

  // Update face animations frequently — Face class handles drawing + display
  if (now - state.lastFaceUpdate >= FACE_UPDATE_INTERVAL) {
    face->Update();
    state.lastFaceUpdate = now;
  }

  // Handle touch sensors
  if (now - state.lastOledUpdate >= OLED_PAGE_INTERVAL) {
    handleTouchSensors();
    state.lastOledUpdate = now;
  }

  // Check trap online status
  if (state.trapOnline && (now - state.lastTrapMessage > HEARTBEAT_TIMEOUT_MS)) {
    state.trapOnline = false;
    broadcastToWebSocket();
  }

  state.wifiConnected = (WiFi.status() == WL_CONNECTED);
  state.rssi = WiFi.RSSI();

  delay(5);
}

// ===== Face Initialization =====
void initFace() {
  face = new Face(128, 64, 40);
  face->RandomBlink = true;
  face->Blink.Timer.SetIntervalMillis(3000);
  face->RandomBehavior = false;
  face->RandomLook = true;
  Serial.println("Face (ESP32-Eyes) initialized");
}

// ===== WiFi Initialization =====
void initWiFi() {
  WiFi.mode(WIFI_AP_STA);

  // Auto-start web server when WiFi connects
  WiFi.onEvent(onWiFiEvent);

  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());

  WiFi.begin(STASSID, STAPASS);
  Serial.print("Connecting to WiFi");
  uint32_t start = millis();
  while (millis() - start < 5000 && WiFi.status() != WL_CONNECTED) {
    delay(100);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Connected to ");
    Serial.println(STASSID);
    state.apMode = false;
  } else {
    Serial.println("Using AP mode only");
    state.apMode = true;
  }
  state.wifiConnected = true;
}

// ===== WiFi Event Handler — auto-manages web server =====
void onWiFiEvent(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.print("WiFi connected, IP: ");
      Serial.println(WiFi.localIP());
      state.wifiConnected = true;
      state.apMode = false;
      // Server is already running, just log it
      Serial.println("Web server auto-available on http://" + WiFi.localIP().toString());
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial.println("WiFi lost, reconnecting...");
      state.wifiConnected = false;
      WiFi.reconnect();
      break;
    case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
      Serial.println("Client connected to AP");
      state.connectedClients++;
      break;
    case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
      Serial.println("Client disconnected from AP");
      if (state.connectedClients > 0) state.connectedClients--;
      break;
    default: break;
  }
}

// ===== ESP-NOW Initialization =====
void initESPNow() {
  Serial.print("Gateway MAC: ");
  Serial.println(WiFi.macAddress());

  if (esp_now_init() != ESP_OK) {
    Serial.println("ERROR: ESP-NOW init failed");
    return;
  }

  esp_now_register_recv_cb(onDataRecv);
  esp_now_register_send_cb(onDataSent);

  esp_now_peer_info_t peerInfo;
  memset(&peerInfo, 0, sizeof(peerInfo));
  memcpy(peerInfo.peer_addr, trapMAC, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("ERROR: Failed to add trap peer");
  } else {
    Serial.println("ESP-NOW initialized");
  }
}

// ===== Web Server =====
void initWebServer() {
  server.on("/", handleRoot);
  server.on("/api/status", handleAPI);
  server.on("/api/data", handleData);
  server.on("/api/command", HTTP_POST, handleCommand);
  server.onNotFound(handleRoot);
  server.begin();
  Serial.println("Web server started on port 80");
}

void initWebSocket() {
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  Serial.println("WebSocket started on port 81");
}

void handleAPI() {
  String json = "{";
  json += "\"trap_online\":" + String(state.trapOnline ? "true" : "false") + ",";
  json += "\"led_state\":" + String(state.ledState ? "true" : "false") + ",";
  json += "\"led_brightness\":" + String(state.ledBrightness) + ",";
  json += "\"buzz_state\":" + String(state.buzzState ? "true" : "false") + ",";
  json += "\"ir_detected\":" + String(state.irDetected ? "true" : "false") + ",";
  json += "\"distance_cm\":" + String(state.distanceCM, 1) + ",";
  json += "\"mode\":" + String(state.operatingMode) + ",";
  json += "\"detection_count\":" + String(state.detectionCount) + ",";
  json += "\"error_flags\":" + String(state.errorFlags) + ",";
  json += "\"emotion\":" + String(state.currentEmotionIdx) + ",";
  json += "\"wifi_connected\":" + String(state.wifiConnected ? "true" : "false") + ",";
  json += "\"rssi\":" + String(state.rssi) + ",";
  json += "\"uptime\":" + String(millis() / 1000) + ",";
  json += "\"free_heap\":" + String(ESP.getFreeHeap());
  json += "}";
  server.send(200, "application/json", json);
}

void handleData() { handleAPI(); }

void handleCommand() {
  if (server.hasArg("cmd")) {
    uint8_t cmd = server.arg("cmd").toInt();
    uint8_t param = server.hasArg("param") ? server.arg("param").toInt() : 0;
    sendCommand(cmd, param);
    server.send(200, "application/json", "{\"ok\":true}");
  } else {
    server.send(400, "application/json", "{\"error\":\"missing cmd\"}");
  }
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      state.connectedClients--;
      break;
    case WStype_CONNECTED:
      state.connectedClients++;
      {
        String json = "{";
        json += "\"type\":\"status\",";
        json += "\"trap_online\":" + String(state.trapOnline ? "true" : "false") + ",";
        json += "\"led_state\":" + String(state.ledState ? "true" : "false") + ",";
        json += "\"led_brightness\":" + String(state.ledBrightness) + ",";
        json += "\"buzz_state\":" + String(state.buzzState ? "true" : "false") + ",";
        json += "\"ir_detected\":" + String(state.irDetected ? "true" : "false") + ",";
        json += "\"distance_cm\":" + String(state.distanceCM, 1) + ",";
        json += "\"mode\":" + String(state.operatingMode) + ",";
        json += "\"detection_count\":" + String(state.detectionCount) + ",";
        json += "\"error_flags\":" + String(state.errorFlags) + ",";
        json += "\"emotion\":" + String(state.currentEmotionIdx);
        json += "}";
        webSocket.sendTXT(num, json);
      }
      break;
    case WStype_TEXT:
      {
        String msg = String((char*)payload);
        int cmdIdx = msg.indexOf("\"cmd\":");
        int paramIdx = msg.indexOf("\"param\":");
        if (cmdIdx >= 0) {
          uint8_t cmd = msg.substring(cmdIdx + 6, cmdIdx + 8).toInt();
          uint8_t param = paramIdx >= 0 ? msg.substring(paramIdx + 8, paramIdx + 10).toInt() : 0;
          sendCommand(cmd, param);
        }
      }
      break;
  }
}

// ===== ESP-NOW Callbacks =====
void onDataRecv(const esp_now_recv_info *info, const uint8_t *data, int len) {
  if (len == sizeof(TrapMessage)) {
    TrapMessage* msg = (TrapMessage*)data;
    if (!msg->validateChecksum()) return;

    state.trapOnline = true;
    state.lastTrapMessage = millis();
    state.ledState = msg->ledState;
    state.ledBrightness = msg->ledBrightness;
    state.buzzState = msg->buzzState;
    state.irDetected = msg->irState;
    state.distanceCM = msg->distanceCM;
    state.operatingMode = msg->operatingMode;
    state.detectionCount = msg->detectionCount;
    state.errorFlags = msg->errorFlags;
    state.trapTimestamp = msg->timestamp;

    if (msg->msgType == MSG_LIFT_ALERT) {
      setEmotion(4); // Scared
      state.isLifted = true;
      Serial.println("LIFT ALERT - Scared face");
    } else if (msg->msgType == MSG_DETECTION_EVENT) {
      setEmotion(2); // Angry
    }

    if (msg->buzzState && !state.isLifted) {
      setEmotion(2); // Angry when buzzing
    }

    if (state.isLifted && !(msg->errorFlags & ERR_LIFTED)) {
      state.isLifted = false;
      setEmotion(0); // Happy
    }

    broadcastToWebSocket();
  }
}

void onDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
  if (status != ESP_NOW_SEND_SUCCESS) {
    Serial.println("ESP-NOW send failed");
  }
}

void sendCommand(uint8_t cmd, uint8_t param) {
  CommandMessage msg;
  msg.msgType = MSG_COMMAND;
  msg.deviceId = DEVICE_ID_GATEWAY;
  msg.command = cmd;
  msg.param = param;
  memset(msg.reserved, 0, sizeof(msg.reserved));
  msg.calculateChecksum();
  esp_now_send(trapMAC, (uint8_t*)&msg, sizeof(msg));

  switch (cmd) {
    case CMD_LED_ON: state.ledState = true; break;
    case CMD_LED_OFF: state.ledState = false; break;
    case CMD_LED_BRIGHTNESS: state.ledBrightness = param; break;
    case CMD_BUZZ_ON:
      state.buzzState = true;
      setEmotion(2);
      break;
    case CMD_BUZZ_OFF:
      state.buzzState = false;
      setEmotion(0);
      break;
    case CMD_SET_MODE: state.operatingMode = param; break;
    case CMD_RESET_COUNTER: state.detectionCount = 0; break;
    case CMD_SET_VOLUME: break;
  }
  broadcastToWebSocket();
}

// ===== Touch Sensors =====
void handleTouchSensors() {
  uint32_t now = millis();

  bool touch1 = digitalRead(PIN_TOUCH_EMOTION);
  if (touch1 && !state.touchEmotionState && (now - state.lastTouchEmotion > TOUCH_DEBOUNCE_MS)) {
    processEmotionChange();
    state.lastTouchEmotion = now;
  }
  state.touchEmotionState = touch1;

  bool touch2 = digitalRead(PIN_TOUCH_BUZZ);
  if (touch2 && !state.touchBuzzState && (now - state.lastTouchBuzz > TOUCH_DEBOUNCE_MS)) {
    processBuzzToggle();
    state.lastTouchBuzz = now;
  }
  state.touchBuzzState = touch2;
}

// ===== Emotion Control =====
void setEmotion(uint8_t emotionIdx) {
  if (emotionIdx > 4) emotionIdx = 0;
  state.currentEmotionIdx = emotionIdx;

  switch (emotionIdx) {
    case 0: face->Expression.GoTo_Happy();    currentEmotion = eEmotions::Happy;    break;
    case 1: face->Expression.GoTo_Normal();   currentEmotion = eEmotions::Normal;   break;
    case 2: face->Expression.GoTo_Angry();    currentEmotion = eEmotions::Angry;    break;
    case 3: face->Expression.GoTo_Sleepy();   currentEmotion = eEmotions::Sleepy;   break;
    case 4: face->Expression.GoTo_Scared();   currentEmotion = eEmotions::Scared;   break;
  }
  Serial.print("Emotion: ");
  Serial.println(state.emotionNames[emotionIdx]);
}

void processEmotionChange() {
  state.currentEmotionIdx = (state.currentEmotionIdx + 1) % 5;
  state.isLifted = false;
  setEmotion(state.currentEmotionIdx);
}

void processBuzzToggle() {
  if (state.buzzState) {
    sendCommand(CMD_BUZZ_OFF, 0);
  } else {
    sendCommand(CMD_BUZZ_ON, 0);
  }
}

// ===== WebSocket Broadcast =====
void broadcastToWebSocket() {
  String json = "{";
  json += "\"type\":\"update\",";
  json += "\"trap_online\":" + String(state.trapOnline ? "true" : "false") + ",";
  json += "\"led_state\":" + String(state.ledState ? "true" : "false") + ",";
  json += "\"led_brightness\":" + String(state.ledBrightness) + ",";
  json += "\"buzz_state\":" + String(state.buzzState ? "true" : "false") + ",";
  json += "\"ir_detected\":" + String(state.irDetected ? "true" : "false") + ",";
  json += "\"distance_cm\":" + String(state.distanceCM, 1) + ",";
  json += "\"mode\":" + String(state.operatingMode) + ",";
  json += "\"detection_count\":" + String(state.detectionCount) + ",";
  json += "\"error_flags\":" + String(state.errorFlags) + ",";
  json += "\"emotion\":" + String(state.currentEmotionIdx) + ",";
  json += "\"is_lifted\":" + String(state.isLifted ? "true" : "false") + ",";
  json += "\"uptime\":" + String(millis() / 1000);
  json += "}";
  webSocket.broadcastTXT(json);
}

// ===== Embedded Web Page =====
#include "index_html.h"

void handleRoot() {
  server.send_P(200, "text/html", INDEX_HTML);
}
