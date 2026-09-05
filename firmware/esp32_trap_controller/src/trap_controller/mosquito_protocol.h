#ifndef MOSQUITO_PROTOCOL_H
#define MOSQUITO_PROTOCOL_H

#include <stdint.h>

// Device IDs
#define DEVICE_ID_TRAP    0x01
#define DEVICE_ID_GATEWAY 0x02

// Message Types
enum MessageType : uint8_t {
  MSG_STATUS_UPDATE    = 0x00,
  MSG_COMMAND          = 0x01,
  MSG_ACK              = 0x02,
  MSG_HEARTBEAT        = 0x03,
  MSG_DETECTION_EVENT  = 0x04,
  MSG_CONFIG_UPDATE    = 0x05,
  MSG_LIFT_ALERT       = 0x06
};

// Command Types
enum CommandCode : uint8_t {
  CMD_LED_ON           = 0x01,
  CMD_LED_OFF          = 0x02,
  CMD_LED_BRIGHTNESS   = 0x03,
  CMD_BUZZ_ON          = 0x04,
  CMD_BUZZ_OFF         = 0x05,
  CMD_ZAPPER_ON        = 0x06,
  CMD_ZAPPER_OFF       = 0x07,
  CMD_SET_MODE         = 0x08,
  CMD_RESET_COUNTER    = 0x09,
  CMD_RESTART          = 0x0A,
  CMD_SET_VOLUME       = 0x0B
};

// Operating Modes
enum OperatingMode : uint8_t {
  MODE_AUTO     = 0x00,
  MODE_MANUAL   = 0x01,
  MODE_SCHEDULE = 0x02,
  MODE_SILENT   = 0x03
};

// Error Flags
#define ERR_IR_FAULT      0x01
#define ERR_HCSR04_TIMEOUT 0x02
#define ERR_ZAPPER_OVCUR   0x04
#define ERR_LED_FAULT      0x08
#define ERR_BUZZER_FAULT   0x10
#define ERR_OVER_TEMP      0x20
#define ERR_LIFTED         0x40
#define ERR_COMM_ERROR     0x80

// Message Structure (16 bytes)
struct __attribute__((packed)) TrapMessage {
  uint8_t    msgType;
  uint8_t    deviceId;
  uint32_t   timestamp;
  uint8_t    ledState;
  uint8_t    ledBrightness;
  uint8_t    buzzState;
  uint8_t    irState;
  uint8_t    distanceCM;
  uint8_t    operatingMode;
  uint16_t   detectionCount;
  uint8_t    errorFlags;
  uint8_t    checksum;

  void calculateChecksum() {
    checksum = 0;
    uint8_t* data = (uint8_t*)this;
    for (int i = 0; i < 15; i++) {
      checksum ^= data[i];
    }
  }

  bool validateChecksum() const {
    uint8_t sum = 0;
    uint8_t* data = (uint8_t*)this;
    for (int i = 0; i < 16; i++) {
      sum ^= data[i];
    }
    return sum == 0;
  }
};

// Command Message (16 bytes)
struct __attribute__((packed)) CommandMessage {
  uint8_t    msgType;
  uint8_t    deviceId;
  uint8_t    command;
  uint8_t    param;
  uint8_t    reserved[11];
  uint8_t    checksum;

  void calculateChecksum() {
    checksum = 0;
    uint8_t* data = (uint8_t*)this;
    for (int i = 0; i < 15; i++) {
      checksum ^= data[i];
    }
  }

  bool validateChecksum() const {
    uint8_t calculated = 0;
    uint8_t* data = (uint8_t*)this;
    for (int i = 0; i < 15; i++) {
      calculated ^= data[i];
    }
    return calculated == checksum;
  }
};

#endif // MOSQUITO_PROTOCOL_H
