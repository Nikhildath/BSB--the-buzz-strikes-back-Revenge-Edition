/*
 * ESP32 #2 — UI Gateway Firmware
 * 
 * Controls: OLED Touchscreen, Web Server, ESP-NOW Gateway
 * Receives trap data and provides web dashboard + OLED display
 */

#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <esp_now.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "mosquito_protocol.h"

// ===== OLED Configuration =====
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define OLED_ADDR     0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

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
#define OLED_UPDATE_INTERVAL   500
#define TOUCH_DEBOUNCE_MS      300

// ===== Display Pages =====
enum OledPage {
  PAGE_HOME = 0,
  PAGE_TRAP_STATUS,
  PAGE_SENSOR,
  PAGE_DETECTIONS,
  PAGE_LED_CTRL,
  PAGE_BUZZ_CTRL,
  PAGE_MODE,
  PAGE_WIFI,
  PAGE_ESPNOW,
  PAGE_COUNT
};

// ===== Global State =====
struct GatewayState {
  // Trap data received from ESP32 #1
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
  
  // OLED
  OledPage currentPage = PAGE_HOME;
  bool pageChanged = true;
  uint32_t lastOledUpdate = 0;
  
  // Touch
  bool touchEmotionState = false;
  bool touchBuzzState = false;
  uint32_t lastTouchEmotion = 0;
  uint32_t lastTouchBuzz = 0;
  
  // Emotions
  uint8_t currentEmotion = 0; // 0=happy, 1=neutral, 2=angry, 3=sleeping, 4=scared
  const char* emotionNames[5] = {"Happy", "Neutral", "Angry", "Sleeping", "Scared"};
  bool isLifted = false;
  
  // WiFi
  bool wifiConnected = false;
  bool apMode = true;
  int rssi = 0;
  
  // Web
  uint32_t connectedClients = 0;
} state;

// Trap MAC address — set to YOUR Trap ESP32 MAC
// Since both boards show 00:00:00:00:00:00, use these manual addresses:
uint8_t trapMAC[] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x01};

// This board's own MAC (set before ESP-NOW init)
uint8_t myMAC[] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x02};

// ===== OLED Face Graphics =====
// Happy face (small eyes, smile)
static const uint8_t face_happy[] PROGMEM = {
  0b00111100, 0b01000010, 0b10100101, 0b10000001,
  0b10100101, 0b01000010, 0b00111100
};

// ===== Function Prototypes =====
void initOLED();
void initWiFi();
void initESPNow();
void initWebServer();
void initWebSocket();
void handleRoot();
void handleAPI();
void handleData();
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length);
void onDataRecv(const uint8_t *mac_addr, const uint8_t *data, int len);
void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);
void sendCommand(uint8_t cmd, uint8_t param);
void updateOLED();
void drawFace(int x, int y, uint8_t emotion);
void drawEyes(int x, int y, uint8_t emotion);
void drawMouth(int x, int y, uint8_t emotion);
void handleTouchSensors();
void broadcastToWebSocket();
void processEmotionChange();
void processBuzzToggle();
void handleCommand();
void drawHomePage();
void drawTrapStatusPage();
void drawSensorPage();
void drawDetectionPage();
void drawLEDPage();
void drawBuzzPage();
void drawModePage();
void drawWiFiPage();
void drawESPNowPage();

// ===== Setup =====
void setup() {
  Serial.begin(115200);
  Serial.println("\n=== Mosquito Trap UI Gateway Starting ===");
  
  initOLED();
  initWiFi();
  
  // Set custom MAC address (needed when board MAC is 00:00:00:00:00:00)
  esp_wifi_set_mac(WIFI_IF_STA, myMAC);
  Serial.print("Gateway MAC set to: ");
  Serial.println(WiFi.macAddress());
  
  initESPNow();
  initWebServer();
  initWebSocket();
  
  pinMode(PIN_TOUCH_EMOTION, INPUT);
  pinMode(PIN_TOUCH_BUZZ, INPUT);
  pinMode(PIN_BUILDIN_LED, OUTPUT);
  
  Serial.println("=== UI Gateway Ready ===\n");
}

// ===== Main Loop =====
void loop() {
  uint32_t now = millis();
  
  server.handleClient();
  webSocket.loop();
  
  // Update OLED
  if (now - state.lastOledUpdate >= OLED_UPDATE_INTERVAL) {
    handleTouchSensors();
    updateOLED();
    state.lastOledUpdate = now;
  }
  
  // Check trap online status
  if (state.trapOnline && (now - state.lastTrapMessage > HEARTBEAT_TIMEOUT_MS)) {
    state.trapOnline = false;
    state.pageChanged = true;
    broadcastToWebSocket();
  }
  
  // Update WiFi status
  state.wifiConnected = (WiFi.status() == WL_CONNECTED);
  state.rssi = WiFi.RSSI();
  
  delay(10);
}

// ===== OLED Initialization =====
void initOLED() {
  Wire.begin(21, 22);
  
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("ERROR: OLED init failed");
    return;
  }
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(20, 25);
  display.println("Mosquito Trap");
  display.setCursor(35, 40);
  display.println("Loading...");
  display.display();
  
  Serial.println("OLED initialized");
}

// ===== WiFi Initialization =====
void initWiFi() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID, AP_PASS);
  
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());
  
  // Also try to connect to home WiFi
  WiFi.begin(STASSID, STAPASS);
  
  uint32_t start = millis();
  while (millis() - start < 5000 && WiFi.status() != WL_CONNECTED) {
    delay(100);
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Connected to ");
    Serial.println(STASSID);
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    state.apMode = false;
  } else {
    Serial.println("Using AP mode only");
    state.apMode = true;
  }
  
  state.wifiConnected = true;
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
  
  // Add trap peer
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

void handleRoot();

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
  json += "\"emotion\":" + String(state.currentEmotion) + ",";
  json += "\"wifi_connected\":" + String(state.wifiConnected ? "true" : "false") + ",";
  json += "\"rssi\":" + String(state.rssi) + ",";
  json += "\"uptime\":" + String(millis() / 1000) + ",";
  json += "\"free_heap\":" + String(ESP.getFreeHeap());
  json += "}";
  server.send(200, "application/json", json);
}

void handleData() {
  handleAPI();
}

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
      Serial.printf("WebSocket [%u] disconnected\n", num);
      state.connectedClients--;
      break;
      
    case WStype_CONNECTED:
      Serial.printf("WebSocket [%u] connected\n", num);
      state.connectedClients++;
      // Send current state
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
        json += "\"emotion\":" + String(state.currentEmotion);
        json += "}";
        webSocket.sendTXT(num, json);
      }
      break;
      
    case WStype_TEXT:
      // Parse command from web dashboard
      {
        String msg = String((char*)payload);
        // Simple JSON parsing for command
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
void onDataRecv(const uint8_t *mac_addr, const uint8_t *data, int len) {
  if (len == sizeof(TrapMessage)) {
    TrapMessage* msg = (TrapMessage*)data;

    if (!msg->validateChecksum()) {
      Serial.println("Bad checksum on trap message");
      return;
    }

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

    // Auto-change emotion based on message type
    if (msg->msgType == MSG_LIFT_ALERT) {
      state.currentEmotion = 4; // Scared face when lifted
      state.isLifted = true;
      state.pageChanged = true;
      Serial.println("LIFT ALERT - Scared face activated");
    } else if (msg->msgType == MSG_DETECTION_EVENT) {
      state.currentEmotion = 2; // Angry face on detection
      state.pageChanged = true;
    }

    // Buzz state changes emotion (only if not lifted)
    if (msg->buzzState && !state.isLifted) {
      state.currentEmotion = 2; // Angry when buzzing
      state.pageChanged = true;
    }

    // Clear lifted state if height returned to normal
    if (state.isLifted && !(msg->errorFlags & ERR_LIFTED)) {
      state.isLifted = false;
      state.currentEmotion = 0; // Back to happy
      state.pageChanged = true;
    }

    // Broadcast to WebSocket clients
    broadcastToWebSocket();
  }
}

void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
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
  
  // Apply local state for immediate feedback
  switch (cmd) {
    case CMD_LED_ON: state.ledState = true; break;
    case CMD_LED_OFF: state.ledState = false; break;
    case CMD_LED_BRIGHTNESS: state.ledBrightness = param; break;
    case CMD_BUZZ_ON: 
      state.buzzState = true; 
      state.currentEmotion = 2;
      state.pageChanged = true;
      break;
    case CMD_BUZZ_OFF: 
      state.buzzState = false;
      state.currentEmotion = 0;
      state.pageChanged = true;
      break;
    case CMD_SET_MODE: state.operatingMode = param; break;
    case CMD_RESET_COUNTER: state.detectionCount = 0; break;
    case CMD_SET_VOLUME: /* volume is local to trap */ break;
  }
  
  broadcastToWebSocket();
}

// ===== Touch Sensors =====
void handleTouchSensors() {
  uint32_t now = millis();
  
  // Emotion touch sensor
  bool touch1 = digitalRead(PIN_TOUCH_EMOTION);
  if (touch1 && !state.touchEmotionState && (now - state.lastTouchEmotion > TOUCH_DEBOUNCE_MS)) {
    processEmotionChange();
    state.lastTouchEmotion = now;
  }
  state.touchEmotionState = touch1;
  
  // Buzz touch sensor
  bool touch2 = digitalRead(PIN_TOUCH_BUZZ);
  if (touch2 && !state.touchBuzzState && (now - state.lastTouchBuzz > TOUCH_DEBOUNCE_MS)) {
    processBuzzToggle();
    state.lastTouchBuzz = now;
  }
  state.touchBuzzState = touch2;
}

void processEmotionChange() {
  state.currentEmotion = (state.currentEmotion + 1) % 5; // 5 emotions now
  state.isLifted = false; // Manual override clears lift state
  state.pageChanged = true;
  Serial.print("Emotion: ");
  Serial.println(state.emotionNames[state.currentEmotion]);
}

void processBuzzToggle() {
  if (state.buzzState) {
    sendCommand(CMD_BUZZ_OFF, 0);
  } else {
    sendCommand(CMD_BUZZ_ON, 0);
  }
}

// ===== OLED Display =====
void updateOLED() {
  display.clearDisplay();
  
  switch (state.currentPage) {
    case PAGE_HOME:       drawHomePage(); break;
    case PAGE_TRAP_STATUS: drawTrapStatusPage(); break;
    case PAGE_SENSOR:     drawSensorPage(); break;
    case PAGE_DETECTIONS: drawDetectionPage(); break;
    case PAGE_LED_CTRL:   drawLEDPage(); break;
    case PAGE_BUZZ_CTRL:  drawBuzzPage(); break;
    case PAGE_MODE:       drawModePage(); break;
    case PAGE_WIFI:       drawWiFiPage(); break;
    case PAGE_ESPNOW:     drawESPNowPage(); break;
  }
  
  display.display();
  state.pageChanged = false;
}

void drawHomePage() {
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  // Title
  display.setCursor(20, 0);
  display.println("MOSQUITO TRAP");
  
  // Status line
  display.setCursor(0, 12);
  display.print("Trap: ");
  display.println(state.trapOnline ? "ONLINE" : "OFFLINE");
  
  // Draw face
  drawFace(44, 22, state.currentEmotion);
  
  // Mode
  display.setCursor(0, 55);
  display.print("Mode: ");
  const char* modes[] = {"AUTO", "MANUAL", "SCHED", "SILENT"};
  display.println(modes[state.operatingMode]);
  
  // Detection count
  display.setCursor(80, 55);
  display.print("Det: ");
  display.println(state.detectionCount);
}

void drawFace(int x, int y, uint8_t emotion) {
  // Face outline (circle)
  display.drawCircle(x + 16, y + 16, 16, SSD1306_WHITE);
  display.drawCircle(x + 17, y + 16, 16, SSD1306_WHITE);
  
  drawEyes(x, y, emotion);
  drawMouth(x, y, emotion);
}

void drawEyes(int x, int y, uint8_t emotion) {
  switch (emotion) {
    case 0: // Happy - normal eyes
      display.fillCircle(x + 9, y + 12, 3, SSD1306_WHITE);
      display.fillCircle(x + 23, y + 12, 3, SSD1306_WHITE);
      break;
    case 1: // Neutral - half-closed
      display.fillRect(x + 6, y + 11, 7, 2, SSD1306_WHITE);
      display.fillRect(x + 20, y + 11, 7, 2, SSD1306_WHITE);
      break;
    case 2: // Angry - V-shaped eyebrows
      display.fillCircle(x + 9, y + 12, 3, SSD1306_WHITE);
      display.fillCircle(x + 23, y + 12, 3, SSD1306_WHITE);
      // Angry eyebrows
      display.drawLine(x + 5, y + 7, x + 13, y + 9, SSD1306_WHITE);
      display.drawLine(x + 27, y + 7, x + 19, y + 9, SSD1306_WHITE);
      break;
    case 3: // Sleeping - closed eyes
      display.drawLine(x + 6, y + 12, x + 12, y + 12, SSD1306_WHITE);
      display.drawLine(x + 20, y + 12, x + 26, y + 12, SSD1306_WHITE);
      // Zzz
      display.setCursor(x + 28, y + 5);
      display.setTextSize(1);
      display.print("z");
      display.setCursor(x + 32, y + 0);
      display.print("z");
      break;
    case 4: // Scared - wide open eyes with raised eyebrows
      display.fillCircle(x + 9, y + 12, 4, SSD1306_WHITE);
      display.fillCircle(x + 23, y + 12, 4, SSD1306_WHITE);
      // Small pupils looking up
      display.fillCircle(x + 9, y + 10, 1, SSD1306_BLACK);
      display.fillCircle(x + 23, y + 10, 1, SSD1306_BLACK);
      // Raised eyebrows
      display.drawLine(x + 5, y + 5, x + 13, y + 5, SSD1306_WHITE);
      display.drawLine(x + 19, y + 5, x + 27, y + 5, SSD1306_WHITE);
      break;
  }
}

void drawMouth(int x, int y, uint8_t emotion) {
  switch (emotion) {
    case 0: // Happy - smile (arc approximated with lines)
      display.drawPixel(x + 10, y + 22, SSD1306_WHITE);
      display.drawPixel(x + 11, y + 23, SSD1306_WHITE);
      display.drawPixel(x + 12, y + 24, SSD1306_WHITE);
      display.drawPixel(x + 13, y + 24, SSD1306_WHITE);
      display.drawPixel(x + 14, y + 25, SSD1306_WHITE);
      display.drawPixel(x + 15, y + 25, SSD1306_WHITE);
      display.drawPixel(x + 16, y + 25, SSD1306_WHITE);
      display.drawPixel(x + 17, y + 25, SSD1306_WHITE);
      display.drawPixel(x + 18, y + 25, SSD1306_WHITE);
      display.drawPixel(x + 19, y + 25, SSD1306_WHITE);
      display.drawPixel(x + 20, y + 24, SSD1306_WHITE);
      display.drawPixel(x + 21, y + 24, SSD1306_WHITE);
      display.drawPixel(x + 22, y + 23, SSD1306_WHITE);
      display.drawPixel(x + 23, y + 22, SSD1306_WHITE);
      break;
    case 1: // Neutral - straight line
      display.drawLine(x + 10, y + 22, x + 24, y + 22, SSD1306_WHITE);
      break;
    case 2: // Angry - frown (arc approximated with lines)
      display.drawPixel(x + 10, y + 25, SSD1306_WHITE);
      display.drawPixel(x + 11, y + 24, SSD1306_WHITE);
      display.drawPixel(x + 12, y + 23, SSD1306_WHITE);
      display.drawPixel(x + 13, y + 22, SSD1306_WHITE);
      display.drawPixel(x + 14, y + 21, SSD1306_WHITE);
      display.drawPixel(x + 15, y + 20, SSD1306_WHITE);
      display.drawPixel(x + 16, y + 20, SSD1306_WHITE);
      display.drawPixel(x + 17, y + 20, SSD1306_WHITE);
      display.drawPixel(x + 18, y + 20, SSD1306_WHITE);
      display.drawPixel(x + 19, y + 20, SSD1306_WHITE);
      display.drawPixel(x + 20, y + 21, SSD1306_WHITE);
      display.drawPixel(x + 21, y + 22, SSD1306_WHITE);
      display.drawPixel(x + 22, y + 23, SSD1306_WHITE);
      display.drawPixel(x + 23, y + 25, SSD1306_WHITE);
      break;
    case 3: // Sleeping - small O
      display.drawCircle(x + 17, y + 22, 3, SSD1306_WHITE);
      break;
    case 4: // Scared - open screaming mouth
      display.drawCircle(x + 17, y + 22, 5, SSD1306_WHITE);
      display.fillCircle(x + 17, y + 22, 3, SSD1306_WHITE);
      break;
  }
}

void drawTrapStatusPage() {
  display.setTextSize(1);
  display.setCursor(10, 0);
  display.println("== TRAP STATUS ==");
  
  display.setCursor(0, 14);
  display.print("Status: ");
  display.println(state.trapOnline ? "ONLINE" : "OFFLINE");
  
  display.setCursor(0, 24);
  display.print("LED: ");
  display.println(state.ledState ? "ON" : "OFF");
  
  display.setCursor(0, 34);
  display.print("Brightness: ");
  display.println(state.ledBrightness);
  
  display.setCursor(0, 44);
  display.print("Zapper: ");
  display.println((state.errorFlags & ERR_ZAPPER_OVCUR) ? "FAULT" : "OK");
  
  display.setCursor(0, 54);
  display.print("Errors: 0x");
  display.println(state.errorFlags, HEX);
}

void drawSensorPage() {
  display.setTextSize(1);
  display.setCursor(15, 0);
  display.println("== SENSORS ==");
  
  display.setCursor(0, 14);
  display.print("IR Sensor: ");
  display.println(state.irDetected ? "DETECTED" : "Clear");
  
  display.setCursor(0, 26);
  display.print("Height: ");
  display.print(state.distanceCM, 1);
  display.println(" cm");
  
  display.setCursor(0, 38);
  display.print("Lifted: ");
  display.println(state.isLifted ? "YES!" : "No");
  
  display.setCursor(0, 50);
  display.print("HC-SR04: ");
  display.println((state.errorFlags & ERR_HCSR04_TIMEOUT) ? "TIMEOUT" : "OK");
}

void drawDetectionPage() {
  display.setTextSize(1);
  display.setCursor(5, 0);
  display.println("== DETECTIONS ==");
  
  display.setTextSize(2);
  display.setCursor(30, 20);
  display.println(state.detectionCount);
  
  display.setTextSize(1);
  display.setCursor(0, 50);
  display.print("Since boot");
}

void drawLEDPage() {
  display.setTextSize(1);
  display.setCursor(15, 0);
  display.println("== LED CTRL ==");
  
  display.setCursor(0, 16);
  display.print("State: ");
  display.println(state.ledState ? "ON" : "OFF");
  
  display.setCursor(0, 28);
  display.print("Brightness: ");
  display.println(state.ledBrightness);
  
  // Brightness bar
  display.drawRect(0, 42, 128, 10, SSD1306_WHITE);
  int barWidth = map(state.ledBrightness, 0, 255, 0, 126);
  display.fillRect(1, 43, barWidth, 8, SSD1306_WHITE);
}

void drawBuzzPage() {
  display.setTextSize(1);
  display.setCursor(15, 0);
  display.println("== BUZZ CTRL ==");
  
  display.setCursor(0, 16);
  display.print("State: ");
  display.println(state.buzzState ? "ON" : "OFF");
  
  display.setCursor(0, 28);
  display.print("Mode: ");
  display.println(state.operatingMode == MODE_SILENT ? "SILENT" : "ACTIVE");
  
  display.setCursor(0, 44);
  display.println("Touch sensor to toggle");
}

void drawModePage() {
  display.setTextSize(1);
  display.setCursor(20, 0);
  display.println("== MODE ==");
  
  const char* modes[] = {"AUTO", "MANUAL", "SCHEDULE", "SILENT"};
  for (int i = 0; i < 4; i++) {
    display.setCursor(10, 16 + (i * 10));
    display.print(i == state.operatingMode ? "> " : "  ");
    display.println(modes[i]);
  }
}

void drawWiFiPage() {
  display.setTextSize(1);
  display.setCursor(15, 0);
  display.println("== WIFI ==");
  
  display.setCursor(0, 14);
  display.print("Status: ");
  display.println(state.wifiConnected ? "Connected" : "Disconnected");
  
  display.setCursor(0, 26);
  display.print("Mode: ");
  display.println(state.apMode ? "AP" : "STA+AP");
  
  display.setCursor(0, 38);
  display.print("RSSI: ");
  display.print(state.rssi);
  display.println(" dBm");
  
  display.setCursor(0, 50);
  display.print("Clients: ");
  display.println(state.connectedClients);
}

void drawESPNowPage() {
  display.setTextSize(1);
  display.setCursor(5, 0);
  display.println("== ESP-NOW ==");
  
  display.setCursor(0, 14);
  display.print("Trap: ");
  display.println(state.trapOnline ? "Connected" : "LOST");
  
  display.setCursor(0, 26);
  display.print("Last msg: ");
  if (state.lastTrapMessage > 0) {
    display.print((millis() - state.lastTrapMessage) / 1000);
    display.println("s ago");
  } else {
    display.println("Never");
  }
  
  display.setCursor(0, 38);
  display.print("Trap TS: ");
  display.println(state.trapTimestamp);
  
  display.setCursor(0, 50);
  display.print("Heap: ");
  display.println(ESP.getFreeHeap());
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
  json += "\"emotion\":" + String(state.currentEmotion) + ",";
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
