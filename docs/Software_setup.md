# 🧑🏼‍💻 Software Setup Guide for ArduLoFi

The ArduLoFi board features two independent microcontrollers: a **RAK3172** core module and an **ESP32-C3** co-processor. Both can be programmed natively inside the **Arduino IDE**.

---

## 🛠️ Step 1: Install Arduino IDE
1. Download and install the latest [Arduino IDE](https://www.arduino.cc/en/Main/Software) for your operating system.
2. If on Windows, avoid using the Microsoft Store version to prevent potential issues with custom Board Support Packages.

---

## 📡 Step 2: Configure Board Support Packages (BSP)

Both microcontrollers require separate board packages to be installed in the Board Manager.

### A. RAK3172 Setup (RUI3 Core)
1. In Arduino IDE, open **File > Preferences**.
2. Locate the **Additional Board Manager URLs** field and add the following URL (if there are existing URLs, separate them with a comma or new line):
   ```json
   https://raw.githubusercontent.com/RAKWireless/RAKwireless-Arduino-BSP-Index/main/package_rakwireless.com_rui_index.json
   ```
3. Click **OK**.
4. Go to **Tools > Board > Boards Manager...** (or click the Board Manager icon on the left sidebar).
5. Search for `RAK` and install **RAKwireless RUI STM32 Boards**.
6. Once installed, select the board:
   - **Tools > Board > RAKWireless RUI STM32 Modules > WisDuo RAK3172 Evaluation Board**.

### B. ESP32-C3 Setup
1. Open **File > Preferences** again.
2. Add the official ESP32 Board Manager URL:
   ```json
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_dev_index.json
   ```
3. Click **OK**.
4. In **Boards Manager...**, search for `esp32` and install the **esp32** package by Espressif Systems.
5. Once installed, select the board:
   - **Tools > Board > ESP32 Arduino > ESP32C3 Dev Module** (or similar ESP32-C3 module variant).

---

## 🔌 Step 3: Library Installation

ArduLoFi requires the **ArduLora** library for core sensor wrappers to avoid duplication.

### A. Automatic Installation (Arduino Library Manager)
1. Open **Sketch > Include Library > Manage Libraries...** (or press Ctrl+Shift+I).
2. Search for **ArduLoFi**.
3. Click **Install**. The IDE will detect the `ArduLora` dependency and ask to install it automatically. Choose **Install All**.

### B. Manual Installation (ZIP)
1. Download both **ArduLora** and **ArduLoFi** as `.ZIP` files.
2. In Arduino IDE, go to **Sketch > Include Library > Add .ZIP Library...** and add each one.
3. Open **Tools > Manage Libraries...**, search for `PubSubClient` by Nick O'Leary, and click **Install** (required for ESP32-C3 MQTT).

*Note:* A compile-time guard check (`__has_include`) in `ArduLoFi.h` will prompt you with a clear `#error` if `ArduLora` is missing when compiling your sketch.

---

## 💻 Step 4: Connecting and Uploading Code

ArduLoFi has **two separate USB Type-C ports**. Verify which port you have connected to your PC using the Windows **Device Manager** (check under *Ports (COM & LPT)*).

### A. Programming the RAK3172 Core
1. Connect a USB-C cable to the **RAK3172 USB-C port** (the left port).
2. Ensure you have the **WisDuo RAK3172 Evaluation Board** selected under **Tools > Board**.
3. Select the correct COM port under **Tools > Port** (usually listed as a CH340 device).
4. Click the **Upload** button.
5. *Note:* The module should automatically enter Bootloader mode. If it fails to upload, double-click the reset button on the board to force BOOT mode.
6. *Note on Serial Port sharing:* The RAK3172's default `Serial` (also referenced as `Serial2` / UART2) is physically shared between the CH340 USB-UART chip and the ESP32-C3 communication pins. Any prints to `Serial` will be sent to the ESP32-C3 bridge as well.

### B. Programming the ESP32-C3 Co-Processor
1. Connect a USB-C cable to the **ESP32-C3 USB-C port** (the right port).
2. Ensure you have the **ESP32C3 Dev Module** selected under **Tools > Board**.
3. Configure the board upload settings:
   - **Flash Mode**: QIO
   - **Flash Frequency**: 80MHz
   - **Partition Scheme**: Default 4MB with spiffs
   - **Upload Speed**: 921600
4. Select the correct COM port under **Tools > Port** (usually listed as *USB JTAG/serial debug unit*).
5. Click the **Upload** button.
