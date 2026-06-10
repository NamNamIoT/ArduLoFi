# 📜 ArduLoFi Technical Hardware Documentation

This document contains the pinout mapping and hardware interface descriptions for the ArduLoFi board.

---

## 📍 1. Pinout Mapping

### RAK3172 Pin Definitions
| **Pin Name** | **Function / Note** |  
| :---: | :--- |    
| **PA8** | Yellow LED (Status) |  
| **PA9** | Red LED (TX Indicator) |  
| **PB2** | Blue LED (RX Indicator) |   
| **PA11** | I2C_SDA (Pulled high to sensor power) |  
| **PA12** | I2C_SCL (Pulled high to sensor power) |  
| **PB5** | Sensor Power Enable (3.3V, Active LOW) | 
| **PB6** | UART1_TX (RS485 Transceiver) |  
| **PB7** | UART1_RX (RS485 Transceiver) |  
| **PB3** | Battery Voltage Sensing (ADC Input via 1:1 Divider) |  
| **PA2** | UART2_TX (Serial / Serial2 TX physically wired to USB CH340 and ESP32-C3 RX) |
| **PA3** | UART2_RX (Serial / Serial2 RX physically wired to USB CH340 and ESP32-C3 TX) |

### ESP32-C3 Pin Definitions
| **Pin Name** | **Function / Note** |
| :---: | :--- |
| **GPIO4** | UART1_TX (Serial communication out to RAK3172 PA3 RX) |
| **GPIO5** | UART1_RX (Serial communication in from RAK3172 PA2 TX) |
| **GPIO20** | Native USB_D- (Programming / Debug UART0) |
| **GPIO21** | Native USB_D+ (Programming / Debug UART0) |

---

## 🛠️ 2. Hardware Architecture & Interfaces

### A. Power Delivery
- **Wide Voltage Input**: 5V to 24V DC input via screw terminals. This is stepped down to 3.3V via an onboard high-efficiency DC-DC buck converter.
- **USB Power**: Can be powered via either of the USB Type-C ports (5V input).
- **Battery Charging**: An onboard linear lithium battery charging IC manages charging of an external Li-Po / Li-Ion battery when USB or external power is active.

### B. RS485 Transceiver
- Connected to RAK3172 UART1 (`PB6`/`PB7`).
- Supported by hardware Modbus RTU libraries.
- Power to the transceiver chip is enabled by driving `PB5` **LOW**.

### C. ESP32-C3 Co-Processor
- Communicates with RAK3172 at `115200` baud.
- Serves as a low-power WiFi client. When RAK3172 sends a publish string, the ESP32-C3 wakes up WiFi (if asleep), connects to the MQTT broker, transmits the message, and returns to sleep/standby to save power.

### D. Dual USB Type-C Flashing Ports
- **RAK3172 USB-C**: Connects to the RAK3172 module via an onboard CH340 USB-UART chip. Used for programming the RAK3172 core via Arduino IDE and accessing the RUI3 AT command console.
- **ESP32-C3 USB-C**: Connects directly to the ESP32-C3 chip's native USB-JTAG-Serial interface. Used for programming the ESP32-C3 core via ESP32 Arduino Board Package and viewing system logs.
