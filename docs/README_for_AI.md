# ArduLoFi AI Documentation (for LLMs & AI Assistants)

## Project Overview
ArduLoFi is an IoT development board powered by the RAK3172 module (STM32WLE5CC running RUI3 framework v4.2.4+) and an ESP32-C3 co-processor. It includes onboard RS485 (Modbus RTU), I2C, battery charger circuit, battery voltage sensor, and dual USB-C interfaces.
ESP32-C3 functions exclusively as a WiFi-to-MQTT bridge, receiving data from RAK3172 over UART and pushing it to the cloud.

## Critical Information for AI Agents

### 1. RUI3 Framework & API (v4.2.4+)
*   **Crucial Version Constraint:** All RAK3172 code must comply with RUI3 v4.x API namespaces (e.g. `api.lora.*`, `api.system.sleep.all()`).
*   **No Analog Inputs:** ArduLoFi does not support 0-10V analog readings. Methods like `readAI1_mV` or pins `PA10`/`PA15` are not defined.

### 2. Hardware Mapping & Pinout (RAK3172)
*   **Modbus/RS485 Power:** External sensor power is gated by pin `PB5`. Enable power in `setup()` using: `pinMode(PB5, OUTPUT); digitalWrite(PB5, LOW);` (Active LOW).
*   **Status LEDs:** Status/Yellow is connected to `PA8`, Red/TX is `PA9`, Blue/RX is `PB2`.
*   **I2C:** `PA11` (SDA) and `PA12` (SCL).
*   **RS485 Serial:** Hardware `Serial1` (alias `Serial_Canopus`) mapped to pins `PB6` (TX) and `PB7` (RX).
*   **Battery Voltage Sense:** Pin `PB3` (ADC). Read using `ArduLoFi.readBattery_mV()`.
*   **ESP32-C3 UART Communication:** Hardware `Serial` (alias `Serial_ESP32C3` or `Serial2`) mapped to pins `PA2` (TX) and `PA3` (RX). Physically shared with the USB CH340 console interface.

### 3. ESP32-C3 Details
*   **Serial Interface to RAK3172:** Uses `Serial1` (GPIO4 as RX, GPIO5 as TX) at `115200` baud.
*   **USB Flashing/Debug:** Managed via native USB `Serial` (GPIO20/GPIO21).
*   **MQTT Protocol:** Communicates via a simple line-terminated format: `PUB|<topic>|<payload>\n` sent from RAK3172 to ESP32-C3.

### 4. Code Directives
*   Do not inject standard ESP32 libraries into RAK3172 code, and vice versa.
*   Keep RAK3172 power-optimized (utilize `api.system.sleep.all()` and shut down the ESP32-C3 or disable sensor power pin `PB5` during sleep if required).

## End of AI Context
