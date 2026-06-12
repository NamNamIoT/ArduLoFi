# ArduLoFi Board — The Ultimate LoRa, LoRaWAN & WiFi MQTT Development Platform

<p align="center">
  <img src="examples/ArduLoFi_Canopus/canopus_banner_5to1.png" alt="ArduLoFi Canopus Banner" width="100%">
</p>

**ArduLoFi** is an ultra-compact, dual-microcontroller development board designed for long-range communication and cloud IoT integration. Combining the long-range radio capabilities of the **RAK3172** module with the WiFi and Bluetooth power of the **ESP32-C3** co-processor, ArduLoFi acts as a bridge between sensor nodes, LoRaWAN gateways, and MQTT cloud brokers.

Powered by the official **RUI3 (v4.2.4+)** framework on the RAK3172 side and standard ESP32 Arduino framework on the ESP32-C3 side, ArduLoFi is the perfect solution for smart agriculture, industrial automation, low-power remote monitoring, and telemetry nodes. It is highly suitable for building **IoT Gateways, Mesh Core nodes, Meshtastic networks, and LoRa APs (Access Points)**.

---

## ✨ Key Features & Advantages

*   📡 **Dual Wireless Connectivity**:
    *   **LoRa & LoRaWAN (RAK3172)**: Powered by STM32WLE5CC (32 MHz MCU) for standard Class A, B, and C LoRaWAN or point-to-point (P2P) radio.
    *   **WiFi & Bluetooth (ESP32-C3)**: Powered by RISC-V 32-bit core (160 MHz MCU) to publish telemetry from the RAK3172 module straight to MQTT brokers via WiFi.
*   🔌 **Industrial RS485 Port**: Onboard RS485 transceiver managed by the RAK3172 core, supporting hardware Modbus RTU (Master/Slave) to easily poll PLCs and sensors.
*   ⚡ **Power Management & Battery Support**:
    *   Onboard high-efficiency DC-DC buck converter supporting wide input voltages (**5V to 24V DC**).
    *   Dedicated **Lithium Battery Charger** circuit supporting external batteries.
    *   **Battery Voltage Sensing (ADC PB3)** to monitor external battery levels.
    *   Dedicated GPIO-controlled power gate pin (`PB5`) on RAK3172 to power down external sensors during deep sleep.
*   🌡️ **Sensor Interfaces**:
    *   Hardware I2C port (`PA11`/SDA, `PA12`/SCL) with onboard pull-up resistors.
    *   General purpose GPIOs on both microcontrollers.
*   💻 **Dual USB Type-C Flashing**:
    *   Separate USB-C ports for flashing code on RAK3172 and ESP32-C3 independently.
*   📐 **Ultra-Compact Design**: Space-optimized layout with support for **external battery connections** (no onboard battery holder to keep the board footprint small), making it highly compact compared to ArduLora. Both boards support battery charging and monitoring.

---

## 📊 Hardware Specifications

| Parameter | Specifications |
| --- | --- |
| **Core LoRa Module** | RAKwireless RAK3172 (STM32WLE5CC @ 32 MHz) |
| **Co-Processor** | Espressif ESP32-C3 (RISC-V @ 160 MHz) |
| **Inter-MCU Comm** | Hardware Serial UART (UART2/Serial/Serial2 physically shared on RAK, Serial1 on ESP) |
| **WiFi / Bluetooth** | 802.11 b/g/n WiFi & Bluetooth 5 (LE) on ESP32-C3 |
| **Input Voltage** | 5V – 24V DC via power terminal, or 5V USB-C |
| **Battery Charger** | Onboard Lithium charger (with external battery support) |
| **Battery Monitor** | Analog reading on pin `PB3` (RAK3172) |
| **Sensor Power Control** | Pin `PB5` (Active LOW to supply VCC power) |
| **LED Indicators** | Yellow `PA8` (Status), Red `PA9` (TX), Blue `PB2` (RX) |
| **Communication Ports** | 1x RS485 (Modbus RTU), 1x I2C, 1x Serial UART |

---

## 🗺️ Board Information & Layout

#### Version Hardware Revisions

🏷️ **Rev 1.0 [June-2026] Layout:**
- **USB-C (Left)**: Connected to RAK3172 via CH340 USB-Serial converter for firmware upload and debugging console.
- **USB-C (Right)**: Connected to ESP32-C3 native USB-Serial/JTAG controller for firmware upload and WiFi/MQTT logging.
- **UART Interconnect**: RAK3172 `PA2` (TX) / `PA3` (RX) is wired cross-over to ESP32-C3 `GPIO5` (RX) / `GPIO4` (TX), sharing the same UART2/Serial interface used for debugging.

---

## 📖 Quick Start & Documentation Portal

Get up and running quickly by browsing our detailed guides:

1.  🚀 **[Software Setup Guide](./docs/Software_setup.md)**: Steps to install Board Support Packages (BSP) for RAK3172 (RUI3) and ESP32-C3, configure COM ports, and flash both processors.
2.  🧑‍💻 **[Programming & Examples Guide](./docs/Examples_Guide.md)**: Comprehensive examples containing code snippets and explanations for:
    *   **ArduLoFi_Board_Test**: Diagnose status LEDs, toggles PB5 sensor power, scans I2C, and reads battery voltage.
    *   **ESP32C3_MQTT_Bridge**: Bridge co-processor firmware managing WiFi & MQTT client interfaces.
    *   **ArduLoFi_Gateway_MQTT**: IoT gateway polling RS485 Modbus and I2C sensors, formatting JSON, and sending it to MQTT via ESP32-C3.
    *   **ArduLoFi_Mesh_Core**: LoRa P2P routing mesh node forwarding packet payloads and bridging them to MQTT.
3.  ⚙️ **[Hardware Schematics & Pinout](./docs/Source_hardware.md)**: Pinout maps and interface descriptions.
4.  🤖 **[LLM/AI Assistant Reference](./docs/README_for_AI.md)**: Documentation context and API namespaces for developers utilizing AI coding assistants.

---

## 📦 Library Installation & Dependencies

ArduLoFi depends on the **ArduLora** library for core sensor drivers (BH1750, SHT3x) to prevent duplicate code.

### A. via Arduino Library Manager (Recommended)
1. Go to **Sketch > Include Library > Manage Libraries...**
2. Search for **ArduLoFi** and click **Install**.
3. The IDE will automatically detect the dependency and prompt: *"This library requires ArduLora library. Do you want to install all dependencies?"* Click **Install All**.

### B. via Manual ZIP Download
1. Download both **ArduLora** and **ArduLoFi** as `.ZIP` files from their repositories.
2. In Arduino IDE, select **Sketch > Include Library > Add .ZIP Library...** for each file.

> [!NOTE]
> **Safety Check:** A preprocessor guard (`__has_include`) inside `ArduLoFi.h` will halt compilation with a clear error warning if the `ArduLora` dependency is missing from your system.

Include it at the top of your RAK3172 sketches:
```cpp
#include <ArduLoFi.h>
```

---

## 🏷️ License
This project is licensed under the MIT License - see the LICENSE file for details.
