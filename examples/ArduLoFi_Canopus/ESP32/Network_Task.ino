void Network_Func(void* arg) {
  VOID SETUP() {
    if (Init_Network()) {
      Serial.printf("\n[Network_Task]Network init successfully");
    } else {
      Serial.printf("\n[Network_Task]Network init fail");
      Error(REASON_INIT_ETH_FAIL);
    }
    // wake up RAK's SIM/VETH supply if needed
    Serial2.printf("VETH_ON\r\n");
    
    Init_MQTT();
  }

  uint32_t lastPublishTime = 0;
  VOID LOOP() {
    static uint32_t lastLedToggle = 0;
    if (millis() - lastLedToggle >= 250) {
      lastLedToggle = millis();
      Led_show_status();
    }
    if (NetworkCheck()) {
      if (MQTTConnect()) {
        MQTTLoop();

        if (millis() - lastPublishTime >= 3000) {
          lastPublishTime = millis();
          DataProcessing();
          if (MQTTPublish()) {
            Serial.printf("\n[Network_Task]Push data successfully");
          } else {
            Error(REASON_MQTT_PUBLISH_FAIL);
          }
        }
        DELAY(100);
      } else {
        Error(REASON_MQTT_CONNECT_FAIL);
        DELAY(5000);
      }
    } else {
      Serial.printf("\n[Network_Task] No network connection, waiting...");
      DELAY(10000);
    }
  }
}