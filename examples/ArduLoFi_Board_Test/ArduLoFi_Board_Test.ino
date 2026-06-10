#include <ArduLoFi.h>
#include <Wire.h>

void setup() {
  // Initialize Serial USB debug console (shared with ESP32-C3)
  Serial.begin(115200);
  delay(2000); // Wait for console serial connection
  
  Serial.println("==================================================");
  Serial.println("         ArduLoFi Board Diagnostic Test           ");
  Serial.println("==================================================");

  // 1. Initialize LEDs, Pin Modes, ADC resolution
  ArduLoFi.begin();
  Serial.println("[OK] Board Core Initialized.");

  // Print Chip Unique Hardware ID
  Serial.printf("RAK3172 Unique Chip ID: %s\r\n", ArduLoFi.getChipID().c_str());

  // 2. Test LEDs
  Serial.println("Testing Status LEDs... Look at the board:");
  
  Serial.println("-> Blinking Status LED (Yellow - PA8)...");
  ArduLoFi.setLed(ARDULOFI_LED_STATUS, true); delay(500); ArduLoFi.setLed(ARDULOFI_LED_STATUS, false); delay(500);
  
  Serial.println("-> Blinking TX LED (Red - PA9)...");
  ArduLoFi.setLed(ARDULOFI_LED_TX, true); delay(500); ArduLoFi.setLed(ARDULOFI_LED_TX, false); delay(500);
  
  Serial.println("-> Blinking RX LED (Blue - PB2)...");
  ArduLoFi.setLed(ARDULOFI_LED_RX, true); delay(500); ArduLoFi.setLed(ARDULOFI_LED_RX, false); delay(500);
  
  Serial.println("[OK] LEDs tested.");

  // 3. Test Sensor Power Rail (PB5)
  Serial.println("Testing Sensor Power gate (PB5)...");
  Serial.println("-> Enabling Sensor Power (PB5 LOW)...");
  ArduLoFi.sensorPower(true);
  delay(100);

  // 4. Scan I2C Bus (PA11 SDA, PA12 SCL)
  Serial.println("Scanning I2C Bus...");
  Wire.begin();
  byte error, address;
  int nDevices = 0;
  for(address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
    if (error == 0) {
      Serial.printf("-> I2C Device found at address 0x%02X\r\n", address);
      nDevices++;
    }
  }
  if (nDevices == 0) {
    Serial.println("-> No I2C devices found. Check your connections and I2C address.");
  }
  Serial.println("[OK] I2C Scan Complete.");

  // 5. Test battery voltage monitoring
  Serial.println("Reading Battery Voltage...");
  float bat_mV = ArduLoFi.readBattery_mV();
  Serial.printf("-> Battery ADC: %0.2f mV (%0.3f V)\r\n", bat_mV, bat_mV / 1000.0);
  
  // Power down sensor power to save energy
  Serial.println("-> Powering down Sensor Power (PB5 HIGH)...");
  ArduLoFi.sensorPower(false);

  Serial.println("==================================================");
  Serial.println("Diagnostic Complete! Cycling every 5 seconds.");
  Serial.println("==================================================");
}

void loop() {
  // Toggle status LED
  static bool state = false;
  state = !state;
  ArduLoFi.setLed(ARDULOFI_LED_STATUS, state);
  
  // Print Battery level periodically
  float bat_mV = ArduLoFi.readBattery_mV();
  Serial.printf("[%lu] Battery Voltage: %0.2f mV (%0.3f V)\r\n", millis(), bat_mV, bat_mV / 1000.0);
  
  delay(5000);
}
