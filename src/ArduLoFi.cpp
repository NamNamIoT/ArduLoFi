#include "ArduLoFi.h"

ArduLoFiClass::ArduLoFiClass() {
    // Constructor
}

void ArduLoFiClass::begin() {
    // Initialize power control pin
    pinMode(ARDULOFI_SENSOR_POWER, OUTPUT);
    digitalWrite(ARDULOFI_SENSOR_POWER, HIGH); // Shut down power by default (Active LOW)

    // Initialize LEDs
    pinMode(ARDULOFI_LED_YELLOW, OUTPUT);
    pinMode(ARDULOFI_LED_RED, OUTPUT);
    pinMode(ARDULOFI_LED_BLUE, OUTPUT);

    // Increase ADC resolution for better analog/battery readings
    analogReadResolution(12);
}

void ArduLoFiClass::sensorPower(bool state) {
    // Active LOW: LOW supplies power, HIGH shuts down power
    digitalWrite(ARDULOFI_SENSOR_POWER, state ? LOW : HIGH);
}

void ArduLoFiClass::setLed(uint8_t ledPin, bool state) {
    digitalWrite(ledPin, state ? HIGH : LOW);
}

float ArduLoFiClass::readBattery_mV() {
    // Read external battery voltage via 12-bit ADC on PB3.
    // Assumes a 1:1 voltage divider (scaling factor of 2.0) and 3.3V reference.
    // V_bat = ADC_val * (3300 / 4095) * 2
    return analogRead(ARDULOFI_ADC_BAT) * 3300.0 / 4095.0 * 2.0;
}

void ArduLoFiClass::beginESP32(unsigned long baud) {
    // Initialize Serial (which is shared between USB debug console and ESP32-C3 link)
    Serial_ESP32C3.begin(baud);
}

bool ArduLoFiClass::publishMQTT(const char *topic, const char *payload) {
    if (!topic || !payload) return false;
    // Protocol payload: PUB|<topic>|<message>\n
    Serial_ESP32C3.print("PUB|");
    Serial_ESP32C3.print(topic);
    Serial_ESP32C3.print("|");
    Serial_ESP32C3.println(payload);
    return true;
}

bool ArduLoFiClass::publishMQTT(String topic, String payload) {
    return publishMQTT(topic.c_str(), payload.c_str());
}

bool ArduLoFiClass::configLoraP2P(double freq, uint16_t sf, uint16_t bw, uint16_t cr, uint16_t preamble, uint16_t txPower) {
    // Switch to P2P mode if currently in LoRaWAN
    if (api.lora.nwm.get() != 0) {
        api.lora.nwm.set(0); 
        api.system.reboot(); // Reboot is required after changing network work mode
    }

    bool success = true;
    success &= api.lora.pfreq.set(freq);
    success &= api.lora.psf.set(sf);
    success &= api.lora.pbw.set(bw);
    success &= api.lora.pcr.set(cr);
    success &= api.lora.ppl.set(preamble);
    success &= api.lora.ptp.set(txPower);

    return success;
}

bool ArduLoFiClass::sendP2P(uint8_t *payload, uint16_t len) {
    return api.lora.psend(len, payload);
}

bool ArduLoFiClass::configLoRaWAN(uint8_t *devEui, uint8_t *appEui, uint8_t *appKey, uint8_t lorawanClass, uint8_t region) {
    // Switch to LoRaWAN mode if currently in P2P mode
    if (api.lora.nwm.get() != 1) {
        api.lora.nwm.set(1); 
        api.system.reboot(); // Reboot is required after changing network work mode
    }

    bool success = true;
    success &= api.lora.deveui.set(devEui, 8);
    success &= api.lora.appeui.set(appEui, 8);
    success &= api.lora.appkey.set(appKey, 16);
    success &= api.lora.deviceClass.set(lorawanClass);
    success &= api.lora.band.set(region);
    
    return success;
}

bool ArduLoFiClass::joinLoRaWAN(uint8_t join_mode) {
    api.lora.njm.set(join_mode); // 1 for OTAA, 0 for ABP
    return api.lora.join();
}

bool ArduLoFiClass::sendLoRaWAN(uint8_t port, uint8_t *payload, uint16_t len) {
    return api.lora.send(len, payload, port);
}

void ArduLoFiClass::deepSleep(uint32_t time_ms) {
    api.system.sleep.all(time_ms);
}

void ArduLoFiClass::reboot() {
    api.system.reboot();
}

String ArduLoFiClass::getChipID() {
    return api.system.chipId.get();
}

// Instantiate the global object
ArduLoFiClass ArduLoFi;
