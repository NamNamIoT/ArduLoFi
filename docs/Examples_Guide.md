# Examples and Programming Guide for ArduLoFi

This document provides detailed breakdowns of the 4 specialized, board-specific examples packaged with the **ArduLoFi** library.

---

## 📌 Table of Contents

- [1. ArduLoFi Board Test (Diagnostics)](#1-ardulofi-board-test-diagnostics)
- [2. ESP32-C3 WiFi MQTT Bridge Firmware](#2-esp32-c3-wifi-mqtt-bridge-firmware)
- [3. ArduLoFi Gateway Node (RAK3172)](#3-ardulofi-gateway-node-rak3172)
- [4. ArduLoFi Mesh Core Routing (LoRa P2P)](#4-ardulofi-mesh-core-routing-lora-p2p)

---

## 1. ArduLoFi Board Test (Diagnostics)

This example is a comprehensive hardware diagnostics script designed to test all primary on-board peripherals of the ArduLoFi hardware on the RAK3172 side.

### What it does:
1.  Initializes status LEDs and performs a sequential blink test.
2.  Toggles the `PB5` sensor power gate pin to verify external rail power control.
3.  Scans the I2C bus (`PA11`/`PA12`) for any connected sensors and prints their hex addresses.
4.  Polls and prints the external battery voltage on `PB3` (using the 1:1 division ratio scaling factor).
5.  Prints the unique MCU Chip Hardware ID.

*Find the complete source code under [examples/ArduLoFi_Board_Test/ArduLoFi_Board_Test.ino](file:///d:/Github/ArduLofi/examples/ArduLoFi_Board_Test/ArduLoFi_Board_Test.ino).*

---

## 2. ESP32-C3 WiFi MQTT Bridge Firmware

This firmware runs on the **ESP32-C3 co-processor**. It connects to a local WiFi network, establishes a connection to an MQTT broker, and acts as a bridge for the RAK3172 module.

### How it works:
- It listens on `Serial1` (pins `GPIO4` RX and `GPIO5` TX) at `115200` baud.
- When it receives a packet formatted as `PUB|<topic>|<payload>\n` from the RAK3172 module, it parses it and publishes the payload to the specified MQTT topic.
- It returns an acknowledgment string (`MQTT_OK`, `MQTT_ERR_PUB`, etc.) back to the RAK3172 to confirm status.
- System logs and connection statuses are output via the native USB port (`Serial0`).

*Find the complete source code under [examples/ESP32C3_MQTT_Bridge/ESP32C3_MQTT_Bridge.ino](file:///d:/Github/ArduLofi/examples/ESP32C3_MQTT_Bridge/ESP32C3_MQTT_Bridge.ino).*

---

## 3. ArduLoFi Gateway Node (RAK3172)

This example demonstrates how to deploy the ArduLoFi board as a **Sensor Gateway**. The RAK3172 core aggregates local measurements and pushes them to the cloud via the ESP32-C3 WiFi MQTT Bridge.

### Data Flow & Logic:
- **Peripherals Polled**:
  - Temp/Humidity from an **I2C SHT3x sensor** (uses the dependency header `<ArduLora_SHT3x.h>`).
  - Registers from an external **RS485 Modbus RTU slave** via Serial1 (PB6/PB7 pins, gated by PB5 power).
  - External **battery voltage** levels on PB3.
- **Payload Construction**:
  - Synthesizes all gathered sensor data into a single minified JSON payload (e.g. `{"temp":24.5,"humi":60.2,"modbus":1024,"bat":4.15,"up":60000}`).
- **Bridge Publishing**:
  - Sends the JSON string to the ESP32-C3 via the shared `Serial` channel using `ArduLoFi.publishMQTT()`.
  - Listens for `MQTT_OK` confirmation from the ESP32-C3 and flashes the blue RX LED on success.

*Find the complete source code under [examples/ArduLoFi_Gateway_MQTT/ArduLoFi_Gateway_MQTT.ino](file:///d:/Github/ArduLofi/examples/ArduLoFi_Gateway_MQTT/ArduLoFi_Gateway_MQTT.ino).*

---

## 4. ArduLoFi Mesh Core Routing (LoRa P2P)

This example implements a basic **LoRa P2P routing mesh node** running on the RAK3172 core. It allows nodes to dynamically route and forward messages across hops, and bridges received messages to the internet.

### Mesh Logic & Protocol:
- **Packet Structure**:
  - `Hop Limit (TTL)`: Decremented at each hop. The packet is dropped when hop count reaches 1.
  - `Origin Node ID` / `Target Node ID`: 1-byte LSB values from the unique chip ID.
  - `Message ID`: Used to prevent packet duplication loops.
  - `Payload`: Raw ASCII text.
- **Routing & Forwarding**:
  - If a node receives a packet meant for another node (and Hop Limit > 1), it stages the packet, applies a randomized collision-avoidance delay (200ms to 800ms), and retransmits (forwards) it.
  - If a packet is targeted for this node (or is broadcast `0xFF`), it processes the message and automatically forwards it to the ESP32-C3 to publish to MQTT, bridging the local LoRa mesh with the cloud.
- **Status Broadcasting**:
  - Every 30 seconds, the node broadcasts its battery voltage and status string across the mesh.

*Find the complete source code under [examples/ArduLoFi_Mesh_Core/ArduLoFi_Mesh_Core.ino](file:///d:/Github/ArduLofi/examples/ArduLoFi_Mesh_Core/ArduLoFi_Mesh_Core.ino).*
