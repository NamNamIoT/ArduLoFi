#include <ArduLoFi.h>
#include <Wire.h>
#include <ArduLora_SHT3x.h>   // Includes sensor driver from dependency library
#include "Canopus_Modbus.h"   // Local Modbus master library files

// Initialize sensors and communication
ArduLora_SHT3x sht3x(0x44, &Wire);
ModbusMaster modbusNode;

unsigned long lastPollTime = 0;
const unsigned long pollInterval = 10000; // Poll and publish every 10 seconds

void setup() {
  // 1. Initialize core ArduLoFi hardware configuration
  ArduLoFi.begin();
  
  // Power up external sensors and RS485 transceiver (PB5 LOW)
  ArduLoFi.sensorPower(true);
  delay(100);

  // 2. Initialize ESP32-C3 communication on the shared UART2
  ArduLoFi.beginESP32(115200);
  
  // Give notice to serial console (debug monitor will see this too)
  Serial.println("--- ArduLoFi Gateway Node Starting ---");

  // 3. Initialize I2C Bus & SHT3x Sensor
  Wire.begin();
  if (sht3x.begin()) {
    Serial.println("[OK] SHT3x Sensor Initialized.");
  } else {
    Serial.println("[WARN] SHT3x Sensor not detected. Check I2C wiring.");
  }

  // 4. Initialize Modbus RTU Master on Serial1 (PB6/PB7 RS485 pins)
  Serial_Canopus.begin(9600, SERIAL_8N1);
  modbusNode.begin(1, Serial_Canopus); // Communicate with Modbus Slave ID 1
  Serial.println("[OK] Modbus RTU Master Initialized on Serial1.");
}

void loop() {
  // Check if poll interval elapsed
  if (millis() - lastPollTime >= pollInterval) {
    lastPollTime = millis();
    
    // Toggle status LED to show activity
    ArduLoFi.setLed(ARDULOFI_LED_STATUS, true);

    // --- A. Read Battery Voltage ---
    float bat_V = ArduLoFi.readBattery_mV() / 1000.0;

    // --- B. Read I2C SHT3x Temperature & Humidity ---
    float temp = -99.0;
    float humi = -99.0;
    if (sht3x.measure()) {
      temp = sht3x.temperature();
      humi = sht3x.humidity();
    }

    // --- C. Read RS485 Modbus holding register 40001 ---
    uint16_t modbusVal = 0;
    uint8_t result = modbusNode.readHoldingRegisters(0, 1);
    if (result == modbusNode.ku8MBSuccess) {
      modbusVal = modbusNode.getResponseBuffer(0);
    }

    // --- D. Formulate MQTT Payload (JSON String) ---
    // Example: {"temp":24.5,"humi":60.2,"modbus":1024,"bat":4.15,"up":60000}
    String payload = "{\"temp\":" + (temp != -99.0 ? String(temp, 1) : "null") + ",";
    payload += "\"humi\":" + (humi != -99.0 ? String(humi, 1) : "null") + ",";
    payload += "\"modbus\":" + String(modbusVal) + ",";
    payload += "\"bat\":" + String(bat_V, 2) + ",";
    payload += "\"up\":" + String(millis()) + "}";

    // --- E. Send Publish request to ESP32-C3 co-processor ---
    String chipID = ArduLoFi.getChipID();
    String topic = "ardulofi/" + chipID + "/telemetry";
    
    // Serial debug print
    Serial.printf("[GATEWAY] Publishing JSON to topic '%s'\r\n", topic.c_str());
    
    // Sends "PUB|<topic>|<payload>\n" over Serial to ESP32-C3
    ArduLoFi.publishMQTT(topic, payload);
    
    // Flash TX LED (Red) to indicate transmission
    ArduLoFi.setLed(ARDULOFI_LED_TX, true);
    delay(100);
    ArduLoFi.setLed(ARDULOFI_LED_TX, false);
    ArduLoFi.setLed(ARDULOFI_LED_STATUS, false);
  }

  // Monitor incoming status bytes from ESP32-C3 bridge
  if (Serial_ESP32C3.available()) {
    String response = Serial_ESP32C3.readStringUntil('\n');
    response.trim();
    if (response.length() > 0) {
      // Clean display if it's a bridge report
      if (response.startsWith("MQTT_")) {
        Serial.printf("[BRIDGE STATUS] %s\r\n", response.c_str());
        
        // Flash RX LED (Blue) on success
        if (response == "MQTT_OK") {
          ArduLoFi.setLed(ARDULOFI_LED_RX, true);
          delay(100);
          ArduLoFi.setLed(ARDULOFI_LED_RX, false);
        }
      }
    }
  }
}
