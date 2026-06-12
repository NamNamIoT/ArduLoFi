#include "function.h"
#include "Arduino.h"
#include <ETH.h>
#include <Wire.h>
#include <ArtronShop_SHT3x.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include <PubSubClient.h>
#include <WiFiClient.h>
#include "Adafruit_SHT31.h"
#include <TridentTD_EasyFreeRTOS32.h>
#define MSG_BUFFER_SIZE 256
char messageBuffer[MSG_BUFFER_SIZE];

float temp1 = 1;
float temp2 = 2;
float hum1 = 1;
float hum2 = 2;
float wind = 0;
float rain = 0.0f;
float solar = 0.0f;
float water_level = 0.0f;
float batt_v = 0.0f;
float accu_v = 0.0f;
// Global objects
ArtronShop_SHT3x sht3x(0x44, &Wire);
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
WiFiMulti wifiMulti;
const adc1_channel_t ADC_BATT = ADC1_CHANNEL_3; // GPIO39 = sensor VN
const adc1_channel_t ADC_ACCU = ADC1_CHANNEL_0; // GPIO36 = sensor VP
esp_adc_cal_characteristics_t adc_chars;

void Init_io() {
  pinMode(LED_ESP_RUN, OUTPUT);
  if (LED_ESP_ERR >= 0) pinMode(LED_ESP_ERR, OUTPUT);
  digitalWrite(LED_ESP_RUN, HIGH);
  if (LED_ESP_ERR >= 0) digitalWrite(LED_ESP_ERR, HIGH);
}

bool Init_Network() {
  pinMode(ENABLE_OCS, OUTPUT);
  digitalWrite(ENABLE_OCS, HIGH);

  // WiFi credentials are added to wifiMulti in Read_Setting() from hardcoded config

  if (ETH.begin()) {
    return true;
  }
  return false;
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  char message[256];
  if (length >= sizeof(message)) length = sizeof(message) - 1;
  memcpy(message, payload, length);
  message[length] = '\0';
  
  Serial.printf("\n[MQTT] Received on %s: %s", topic, message);

  if (strcmp(topic, MQTT_TOPIC_URGENT) == 0) {
    // Urgent command — forward immediately via LoRa
    char urgentFrame[280];
    snprintf(urgentFrame, sizeof(urgentFrame), "*URGENT,%s#", message);
    Serial.printf("\n[MQTT] URGENT forwarding to RAK: %s", urgentFrame);
    Serial2.println(urgentFrame);
  }
  else if (strcmp(topic, MQTT_TOPIC_CMD) == 0) {
    // Normal command — queue for node uplink
    char cmdFrame[280];
    snprintf(cmdFrame, sizeof(cmdFrame), "*CMD,%s#", message);
    Serial.printf("\n[MQTT] Forwarding to RAK: %s", cmdFrame);
    Serial2.println(cmdFrame);
  }
  else if (strcmp(topic, "CANOPUS/CONFIG") == 0) {
    // Network config — forward directly to Gateway as plain command
    // Trim trailing spaces/newlines
    int len = strlen(message);
    while (len > 0 && (message[len - 1] == ' ' || message[len - 1] == '\r' || message[len - 1] == '\n')) {
      message[--len] = '\0';
    }
    char* startPtr = message;
    while (*startPtr == ' ') startPtr++;
    
    Serial2.println(startPtr);
    Serial.printf("\n[MQTT] CONFIG forwarding to RAK: %s", startPtr);
  }
}

void Init_MQTT() {
  Serial.printf("\n[MQTT] Config hardcoded: %s:%d user=%s",
                MQTT_SERVER, MQTT_PORT, MQTT_USER);
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setKeepAlive(60);
}

void MQTTLoop() {
  mqttClient.loop();
}

void Init_sensor() {
  // Placeholder for future sensor initialization
}

void Init_ADC() {
  // Configuration ADC
  adc1_config_width(ADC_WIDTH_BIT_12);                  
  adc1_config_channel_atten(ADC_BATT, ADC_ATTEN_DB_11); 
  adc1_config_channel_atten(ADC_ACCU, ADC_ATTEN_DB_11); 
  // Calibration ADC
  esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100 , &adc_chars);
}

void SensorDataCollection() {
  Serial.printf("\n[Sensor_Task]SensorDataCollection");
  // ReadSensor_I2C();
  ReadSensor_ADC();
  ReadSensor_CAN();
  ReadSensor_MODE();

  // Dynamic fluctuations for solar, rain, and wind to make the UI simulator feel alive
  static float solarAngle = 0.0f;
  solarAngle += 0.05f;
  if (solarAngle > 3.14159f * 2.0f) solarAngle = 0.0f;
  
  // Solar: fluctuates between 150 and 850 W/m^2
  solar = 500.0f + 350.0f * sin(solarAngle) + ((float)(random(-10, 10)) / 2.0f);
  if (solar < 0.0f) solar = 0.0f;

  // Rain: accumulate rain slowly when solar is low, otherwise dry up
  if (solar < 300.0f) {
    rain += 0.05f * ((float)random(0, 5) / 5.0f);
    if (rain > 100.0f) rain = 0.0f; // wrap around
  } else {
    if (rain > 0.0f) rain -= 0.02f;
    if (rain < 0.0f) rain = 0.0f;
  }

  // Wind: simulate soft wind if not active
  if (wind <= 0.1f) {
    wind = 2.0f + 1.5f * sin(solarAngle * 2.0f) + ((float)random(0, 10) / 10.0f);
  }

  // Water level: fluctuates between 40.0% and 85.0%
  static float waterAngle = 0.0f;
  waterAngle += 0.01f;
  if (waterAngle > 3.14159f * 2.0f) waterAngle = 0.0f;
  water_level = 62.5f + 20.0f * sin(waterAngle) + ((float)(random(-5, 5)) / 10.0f);
}

void ReadSensor_I2C() {
  if (sht3x.measure()) {
    Serial.printf("\n[Sensor_Task]Temperature: %.1f *C\tHumidity: %.1f %%RH\n",
                  sht3x.temperature(), sht3x.humidity());
  } else {
    Serial.println("\n[Sensor_Task]SHT3x read error");
  }
}

void ReadSensor_ADC() {
  // ADC raw reading
  int adc_batt_raw = 0, adc_accu_raw = 0;
  // Guard: adc1_get_raw panics if ADC1 was not configured — check channel validity
  if (ADC_BATT < ADC1_CHANNEL_MAX) adc_batt_raw = adc1_get_raw(ADC_BATT);
  if (ADC_ACCU < ADC1_CHANNEL_MAX) adc_accu_raw = adc1_get_raw(ADC_ACCU);
  // Battery (GPIO39): divider ratio → max ~7.1V mapping to 3.3V ADC
  float batt_mv = (float)adc_batt_raw * 7100.0f / 4095.0f;
  // Accumulator (GPIO36): divider ratio 57× → max ~18.8V mapping to 3.3V ADC
  float accu_mv = (float)adc_accu_raw * 3300.0f / 4095.0f * 57.0f / 10.0f;
  batt_v = batt_mv / 1000.0f;
  accu_v = accu_mv / 1000.0f;
  Serial.printf("\n[Sensor_Task]V_ADC_Batt = %.0fmV (%.2fV)  //  V_ADC_Accu = %.0fmV (%.2fV)",
                batt_mv, batt_v, accu_mv, accu_v);
}

void ReadSensor_Pulse() {
}

void ReadSensor_CAN() {
  // Implementation pending details
}

void ReadSensor_MODE() {
  // ARM/DISARM implementation
}

void DataProcessing() {
  // Placeholder
}

bool NetworkCheck() {
  if (ETH.connected()) {
    Serial.printf("\n[Network_Task]Ethernet connected");
    return true;
  }

  if (wifiMulti.run() == WL_CONNECTED) {
    Serial.printf("\n[Network_Task]WiFi connected");
    // Report live SSID so dashboard can display actual connected network
    Serial.printf("\n[Network_Task]WiFi SSID=%s IP=%s#",
                  WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
    return true;
  }
  // char* myssid = GetSSID();
  // char* mypassword = GetPassword();
  // Serial.printf("\nSSID: %s\nPass: %s\n", myssid, mypassword);
  Serial.printf("\n[Network_Task]No network connection");
  return false;
}

int state_mqtt_connect = 0; 
//0 is none
//1 is connecting
//2 is connected
//3 is connect fail
const uint32_t timeout_connect_mqtt = 15000;  // 15 seconds
uint32_t startAttemptTime;
bool MQTTConnect() {
  // If already connected, just return true
  if (mqttClient.connected()) {
    state_mqtt_connect = 2;
    return true;
  }

  // If not connected and timeout hasn't occurred, keep the connection attempt going
  if (state_mqtt_connect == 1 && (millis() - startAttemptTime < timeout_connect_mqtt)) {
    if (mqttClient.connect(String(startAttemptTime).c_str(), MQTT_USER, MQTT_PASS)) {
      Serial.printf("\n[MQTT_Task]Connected");
      mqttClient.subscribe(MQTT_TOPIC_CMD);
      mqttClient.subscribe(MQTT_TOPIC_URGENT);
      mqttClient.subscribe("CANOPUS/CONFIG");
      state_mqtt_connect = 2;
      return true;
    }
    return false; // Still attempting, don't reset
  }

  // Start new connection attempt if: not connecting, failed before, or timeout reached
  if (state_mqtt_connect == 0 || state_mqtt_connect == 3 || (millis() - startAttemptTime >= timeout_connect_mqtt)) {
    startAttemptTime = millis();
    state_mqtt_connect = 1;
    Serial.printf("\n[MQTT_Task]Connecting to MQTT...");
    
    if (mqttClient.connect(String(startAttemptTime).c_str(), MQTT_USER, MQTT_PASS)) {
      Serial.printf("\n[MQTT_Task]Connected");
      mqttClient.subscribe(MQTT_TOPIC_CMD);
      mqttClient.subscribe(MQTT_TOPIC_URGENT);
      mqttClient.subscribe("CANOPUS/CONFIG");
      state_mqtt_connect = 2;
      return true;
    }
    
    state_mqtt_connect = 3;
    Serial.printf("\n[MQTT_Task]failed with state %d\n", mqttClient.state());
    return false;
  }

  return false;
}

bool MQTTPublish() {
  // const String payload = "{\"temperature\":25.0,\"humidity\":60.0}";

  // ═══════════ MQTT TOPIC TABLE ═══════════════════════════════════════════
  // PUBLISH:
  //   CANOPUS/NODE/{id}/DATA    — Per-node sensor JSON (every cycle)
  //   CANOPUS/NODE/{id}/STATUS  — Per-node status JSON (every cycle)
  //   CANOPUS/NETWORK/STATUS    — Network-wide health summary
  //   CANOPUS/DATA              — Legacy aggregate payload (backward compat)
  //
  // SUBSCRIBE:
  //   CANOPUS/CMD               — Normal downlink: \"node_id,command\"
  //   CANOPUS/URGENT            — Urgent broadcast: \"node_id,command\"
  //   CANOPUS/CONFIG            — Network config: \"SET_SF=10\", \"SET_TOTAL_NODES=3\"
  // ═════════════════════════════════════════════════════════════════════════

  // Publish per-node data from LoRa network
  bool anyPublished = false;
  for (int i = 0; i < MAX_LORA_NODES; i++) {
    if (!loraNodes[i].valid) continue;
    if (millis() - loraNodes[i].lastUpdate > 300000UL) continue; // Skip stale >5min

    NodeData &nd = loraNodes[i];
    char topic[32];
    char payload[256];

    // Per-node sensor data
    snprintf(topic, sizeof(topic), "CANOPUS/NODE/%u/DATA", nd.id);
    if (nd.type == 1) {
      // AGRI: s1=temp*100, s2=hum*100
      snprintf(payload, sizeof(payload),
          "{\"id\":%u,\"type\":\"AGRI\",\"temp\":%.2f,\"hum\":%.2f,\"vbat\":%.2f,\"seq\":%u}",
          nd.id, nd.s1 / 100.0f, nd.s2 / 100.0f, nd.vbat / 100.0f, nd.seq);
    } else {
      // Other: raw ADC values
      snprintf(payload, sizeof(payload),
          "{\"id\":%u,\"type\":%u,\"adc1\":%d,\"adc2\":%d,\"vbat\":%.2f,\"seq\":%u}",
          nd.id, nd.type, nd.s1, nd.s2, nd.vbat / 100.0f, nd.seq);
    }
    mqttClient.publish(topic, payload);

    // Per-node status/diagnostic
    snprintf(topic, sizeof(topic), "CANOPUS/NODE/%u/STATUS", nd.id);
    snprintf(payload, sizeof(payload),
        "{\"id\":%u,\"rssi\":%d,\"snr\":%d,\"drift\":%ld,\"adv\":%u,\"star\":%u,\"sf\":%u,\"nodes\":%u}",
        nd.id, nd.rssi, nd.snr, (long)nd.drift, nd.adv, nd.star, nd.sf, nd.nodes);
    mqttClient.publish(topic, payload);

    anyPublished = true;
  }

  // Legacy aggregate payload for backward compat
  snprintf(messageBuffer, sizeof(messageBuffer), 
         "{\"wind\":%.2f,\"rain\":%.2f,\"solar\":%.2f,\"water\":%.2f,\"T1\":%.2f,\"T2\":%.2f,\"H1\":%.2f,\"H2\":%.2f}",
         wind, rain, solar, water_level, temp1, temp2, hum1, hum2);
  mqttClient.publish(MQTT_TOPIC_DATA, messageBuffer);

  if (anyPublished) {
    Serial.printf("[MQTT_Task] Published node data to MQTT\n");
    return true;
  }
  return true; // Legacy publish still succeeded
}

void MQTTPublishRaw(const char* topic, const char* payload) {
  if (mqttClient.connected()) {
    mqttClient.publish(topic, payload);
  }
}

void Read_Setting() {
  Serial.printf("\nSSID: %s\n", WIFI_SSID);
  wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);
}

void Restart() {
  Serial.printf("\n[System]Restarting ESP32...");
  DELAY(100);
  ESP.restart();
}

void Canopus_Sleep() {
  Serial.printf("\n[System]Entering deep sleep...");
  esp_sleep_enable_timer_wakeup(10 * 1000000);
  esp_deep_sleep_start();
}

void Error(int reason) {
  if (LED_ESP_ERR >= 0) digitalWrite(LED_ESP_ERR, LOW);
  Serial.printf("\n[System]Error: %d\r\n", reason);
}


char inputBuffer[512] = {0};
int inputIndex = 0;
bool stringComplete = false;

void mySerialEvent() {
  while (Serial2.available()) {
    char inChar = (char)Serial2.read();
    if (inputIndex < 511) {
      inputBuffer[inputIndex++] = inChar;
      inputBuffer[inputIndex] = '\0';
    } else {
      // Buffer overflow: keep from last '*' to preserve incoming frame structure
      char* lastStar = strrchr(inputBuffer, '*');
      if (lastStar != nullptr) {
        int len = strlen(lastStar);
        memmove(inputBuffer, lastStar, len);
        inputIndex = len;
      } else {
        inputIndex = 0;
      }
      inputBuffer[inputIndex++] = inChar;
      inputBuffer[inputIndex] = '\0';
    }
    if (inChar == '#') {
      stringComplete = true;
    }
  }
}

void Led_show_status() {
  digitalWrite(LED_ESP_RUN, !digitalRead(LED_ESP_RUN));
}

