#ifndef FUNCTION_H
#define FUNCTION_H

#include <Arduino.h>
#include "config.h"

//Define pin ethernet//
#define ETH_PHY_TYPE ETH_PHY_LAN8720
#define ETH_PHY_ADDR 1
#define ETH_PHY_MDC 23
#define ETH_PHY_MDIO 18
#define ETH_PHY_POWER -1
#define ETH_CLK_MODE ETH_CLOCK_GPIO0_IN
#define ENABLE_OCS 14

//define pin temperatue lm234
#define LM234_READ_PIN 4
#define LM234_WRITE_PIN 5

#define PULSE 35

//define pin connect to UART RAK3172
#define RAK_RX 16          // RX ESP32 → TX RAK
#define RAK_TX 17          // TX ESP32 → RX RAK

//define pin enable 3.3V for outbox and MCP2551 (CAN)
#define ENABLE_VEXT 15

//define pin enable 3.3V for outbox and MCP2551 (CAN)
#define ENABLE_VSIM_PIN 13

//define pin for led status
#define LED_ESP_ERR -1
#define LED_ESP_RUN 12

//define pin for setting button, use for go to configure mode in ESP32 (wifi station)
#define SETTING_BT 34
#define ACCU_READ_PIN 36
#define BATTERY_READ_PIN 39


//define fail reason
#define REASON_INIT_ETH_FAIL 1
#define REASON_NETWORK_FAIL 2
#define REASON_MQTT_CONNECT_FAIL 3
#define REASON_MQTT_PUBLISH_FAIL 4
#define REASON_UNKNOWN_FAIL 9

#define DEVICE_CODE "CANOPUS"
#define APSSID "CANOPUS_DEVICE"
#define APPWD "12345678"

#define DEVICE_PASSWORD "123456"

#define  FACTORY_RESET 1
#define  SETTING_REQUEST 2

// Structure to hold parsed sensor data
struct SensorFrame {
  char source[8];
  char deviceId[16];
  uint8_t type;
  int32_t value;
  bool valid;
};

void Led_show_status();
void mySerialEvent();

void Init_io();
void Init_sensor();
void Read_Setting();
bool Init_Network();
void Init_MQTT();
void Init_ADC();
void SensorDataCollection();
void ReadSensor_I2C();
void ReadSensor_ADC();
void ReadSensor_Pulse();
void ReadSensor_CAN();
void ReadSensor_MODE();
void ReadSensor_TEMPERATURE();
void DataProcessing();
bool NetworkCheck();
bool MQTTConnect();
void MQTTLoop();
bool MQTTPublish();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void Canopus_Sleep();
void Error(int);
void Restart();
void MQTTPublishRaw(const char* topic, const char* payload);

// Global variables
extern char inputBuffer[512];   // a character array to hold incoming data
extern int inputIndex;
extern bool stringComplete;     // whether the string is complete
extern float temp1;
extern float temp2;
extern float hum1;
extern float hum2;
extern float wind;
extern float rain;
extern float solar;
extern float water_level;
extern float batt_v;
extern float accu_v;

// ═══════════ NODE DATA STORAGE (for MQTT publishing) ═══════════════════════
#define MAX_LORA_NODES 15
struct NodeData {
  uint8_t  id;
  uint8_t  type;
  int16_t  s1;        // Sensor1 (temp*100 for AGRI, ADC raw for others)
  int16_t  s2;        // Sensor2 (hum*100 for AGRI, ADC raw for others)
  uint16_t vbat;      // Battery centivolts
  uint16_t seq;
  uint8_t  star;
  int32_t  drift;
  uint8_t  nodes;
  uint8_t  sf;
  int16_t  rssi;
  int8_t   snr;
  uint8_t  adv;       // Advisory 1-15
  uint32_t lastUpdate; // millis() timestamp
  bool     valid;
};
extern NodeData loraNodes[MAX_LORA_NODES];

#endif // FUNCTION_H
