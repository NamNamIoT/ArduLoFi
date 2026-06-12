void Sensor_Func(void* arg) {
  VOID SETUP() {
    DELAY(3000);
    // request RAK to enable its VETH output (power to external devices)
    Serial2.printf("VETH_ON\r\n");
  }

  VOID LOOP() {
    Led_show_status();
    SensorDataCollection();
    DELAY(5000);
  }
}