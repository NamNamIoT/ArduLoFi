#include "function.h"
#include "web_server.h"
#include <TridentTD_EasyFreeRTOS32.h>

EasyFreeRTOS32 Network_Task, Sensor_Task, Lora_Task, Web_Task;
void Network_Func(void*), Sensor_Func(void*), Lora_Func(void*), Web_Func(void*);

void setup() {
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, RAK_RX, RAK_TX);

  Serial.printf("\nArduLoFi Canopus Lite V1.0 Testing");
  Read_Setting();
  Serial.printf("\nRead_Setting done");
  Init_ADC();
  Init_io();
  Init_sensor();
  Serial.printf("\nInit_io done");

  Lora_Task.start(Lora_Func, NULL, 2048*6, 1);
  Serial.printf("\nLora_Func start");

  Sensor_Task.start(Sensor_Func, NULL, 2048*4, 0);
  Serial.printf("\nSensor_Func start");

  Network_Task.start(Network_Func, NULL, 2048*8, 1);
  Serial.printf("\nNetwork_Func start");

  Web_Task.start(Web_Func, NULL, 2048*6, 0);
  Serial.printf("\nWeb_Task start");
}

void loop() {
  // HandleClient(); 
  // Canopus_Sleep();
}
