/*
  ArduLoFi.h - Library for ArduLoFi (RAK3172 + ESP32-C3) development.
  Created by NamNamIoT.
  Released into the public domain.
*/

#ifndef ArduLoFi_h
#define ArduLoFi_h

#include <Arduino.h>

#if __has_include(<ArduLora.h>)
#include <ArduLora.h>
#else
#error "The ArduLora library is required to compile ArduLoFi. Please install the ArduLora library from the Arduino Library Manager first!"
#endif

// ArduLofi Hardware Pins Mapping for RUI3
#define ARDULOFI_SENSOR_POWER  PB5

// LEDs
#define ARDULOFI_LED_YELLOW    PA8
#define ARDULOFI_LED_RED       PA9
#define ARDULOFI_LED_BLUE      PB2

// Functional Aliases for LEDs
#define ARDULOFI_LED_STATUS    ARDULOFI_LED_YELLOW
#define ARDULOFI_LED_RX        ARDULOFI_LED_BLUE
#define ARDULOFI_LED_TX        ARDULOFI_LED_RED

// Battery Monitoring (External battery via voltage divider)
#define ARDULOFI_ADC_BAT       PB3

// I2C Interface
#define ARDULOFI_SDA           PA11
#define ARDULOFI_SCL           PA12

// RS485 / Modbus Interface (UART1)
#define ARDULOFI_RS485_TX      PB6
#define ARDULOFI_RS485_RX      PB7

// Serial Alias for ESP32-C3 (UART2 / Serial0 / Serial2 is physically the same port on RAK3172)
#define Serial_ESP32C3         Serial
#define Serial_Canopus         Serial1

class ArduLoFiClass {
  public:
    ArduLoFiClass();

    // 1. Hardware & Power Management
    void begin();
    void sensorPower(bool state);
    void setLed(uint8_t ledPin, bool state);

    // 2. Battery Monitoring (returns millivolts)
    float readBattery_mV();
    
    // 3. ESP32-C3 MQTT Bridge Helper
    void beginESP32(unsigned long baud = 115200);
    bool publishMQTT(const char *topic, const char *payload);
    bool publishMQTT(String topic, String payload);

    // 4. LoRa P2P Configuration Wrapper
    // Note: Calling this might reboot the board if it's currently in LoRaWAN mode.
    // Calling without arguments uses defaults: 868MHz, SF7, BW125, CR4/5, Preamble 8, TX Power 22.
    bool configLoraP2P(double freq = 868000000.0, uint16_t sf = 7, uint16_t bw = 0, uint16_t cr = 0, uint16_t preamble = 8, uint16_t txPower = 22);
    bool sendP2P(uint8_t *payload, uint16_t len);

    // 5. LoRaWAN Configuration Wrapper
    // Note: Calling this might reboot the board if it's currently in P2P mode.
    bool configLoRaWAN(uint8_t *devEui, uint8_t *appEui, uint8_t *appKey, uint8_t lorawanClass = 0, uint8_t region = 4);
    bool joinLoRaWAN(uint8_t join_mode = 1); // 1 = OTAA, 0 = ABP
    bool sendLoRaWAN(uint8_t port, uint8_t *payload, uint16_t len);

    // 6. System Functions
    void deepSleep(uint32_t time_ms);
    void reboot();
    String getChipID();
};

extern ArduLoFiClass ArduLoFi;

#endif
