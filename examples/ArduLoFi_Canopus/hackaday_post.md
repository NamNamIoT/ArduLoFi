# ArduLoFi Canopus: The Next-Gen LoRa Gateway & Dynamic Weather Station

**Project Title**: ArduLoFi Canopus: Industrial-Grade ESP32 LoRa Gateway with Animated Telemetry Dashboard  
**Subtitle**: A plug-and-play, collision-free LoRa telemetry network featuring an elegant, responsive web visualizer and self-powered solar outdoor nodes.

---

## 🛠️ The Challenge & The Solution

Deploying outdoor IoT sensor nodes often comes with two major frustrations:
1. **Radio Traffic Jams**: When multiple sensor nodes transmit at the same time, their signals collide, leading to lost packets and gaps in your telemetry data.
2. **Gateway Crashes**: Many maker-grade base stations suffer from memory leaks and crash after a few days or weeks of continuous operation.

**ArduLoFi Canopus** is built to solve these exact problems. By combining smart time-slot scheduling (TDMA) with high-efficiency, zero-heap-allocation gateway firmware, Canopus provides an incredibly robust, 24/7 continuous telemetry system.

---

## ✨ Key Features

### 📡 Collision-Free LoRa Communication
No more lost packets! Canopus coordinates transmissions using a custom time-slot protocol. The gateway sends a synchronization beacon, and each node takes its turn to transmit in its own designated window. It even auto-corrects for slight timing drifts, keeping your network perfectly in sync.

### ☀️ Autonomous Solar-Powered Nodes
Designed for the great outdoors. The sensor nodes run on low-power STM32-based LoRa hardware, consuming less than 2µA in deep sleep. Combined with a tiny solar panel and battery charging circuit, the weather station runs indefinitely on clean solar energy.

### 🎨 Premium Glassmorphic Web Dashboard
Telemetry shouldn't be boring. Canopus hosts a modern, responsive web dashboard served directly from the gateway's flash memory or streamed remotely over MQTT:
* **Live Animated weather elements**: The SVG weather widget actually matches the real-world metrics! Watch the wind cup speed spin faster, the rain drops speed up, and the sun glow intensify based on real telemetry.
* **Interactive Line Charts**: Track history logs directly in your browser. Click on any sensor card to view real-time temperature and humidity plots.
* **Da Nang Watermark**: A beautiful minimalist line-art outline of the Dragon Bridge (Cầu Rồng) blends technology with local design culture.

### 🔒 Rock-Solid Industrial Firmware
Engineered for ultimate uptime. The ESP32 gateway firmware is built using advanced memory-safe practices. By eliminating dynamic heap allocations in task processing, the gateway is immune to fragmentation crashes and memory leaks.

---

## 📦 What Can It Track?

The system is fully equipped to monitor essential weather and environmental parameters:
* **Air Temperature & Humidity**: Housed in a white louvered radiation shield for precise outdoor readings.
* **Wind Speed & Direction**: Real-time velocity and heading tracking using standard cup and vane sensors.
* **Precipitation**: Tipping-bucket rain gauge to capture exact rain accumulation.
* **Water & Reservoir Levels**: Integrated tank sensors to monitor water storage levels.
* **Power Diagnostics**: Built-in battery voltage monitoring to track gateway and node health.

---

## 🚀 Get Started in 3 Simple Steps

### Step 1: Clone the Repository
```bash
git clone https://github.com/NamNamIoT/ArduLoFi.git
cd ArduLoFi/examples/ArduLoFi_Canopus/
```

### Step 2: Flash the Firmware
1. Open the gateway code in your Arduino IDE (`ESP32/ESP32.ino`).
2. Add your Wi-Fi credentials and MQTT broker details in `ESP32/config.h`.
3. Flash the code to your ESP32 board.

### Step 3: Launch the Dashboard
Simply open `dashboard.html` in your web browser! Enter your ESP32's local IP address to poll live data, or connect via MQTT WebSockets to monitor your weather station from anywhere in the world.
