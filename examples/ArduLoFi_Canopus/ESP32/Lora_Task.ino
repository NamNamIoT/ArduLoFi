// Frame format from RAK: *SOURCE,DEVICE_ID,TYPE,VALUE#
// Sensor TYPE_* codes are defined in config.h (included via function.h)

#include "function.h"
#include <ctype.h>

// ═══════════ NODE DATA STORAGE (for MQTT publishing) ═══════════════════════
// NodeData struct defined in function.h
NodeData loraNodes[MAX_LORA_NODES];

// Parse *NODE_DATA,ID,Type,S1,S2,Vbat,Seq,Star,Drift,Nodes,SF,RSSI,SNR,ADV#
void parseNodeData(char* frame) {
  // Strip * and #
  int len = strlen(frame);
  if (len < 12 || frame[0] != '*' || frame[len-1] != '#') return;

  char buf[256];
  if (len >= sizeof(buf)) return;
  memcpy(buf, frame + 1, len - 2); // strip * and #
  buf[len - 2] = '\0';

  // Check prefix
  if (strncmp(buf, "NODE_DATA,", 10) != 0) return;
  char* dataStart = buf + 10;

  // Parse 13 comma-separated fields
  int fields[13] = {0};
  char* token;
  char* saveptr;
  token = strtok_r(dataStart, ",", &saveptr);
  int idx = 0;
  while (token != NULL && idx < 13) {
    fields[idx++] = atoi(token);
    token = strtok_r(NULL, ",", &saveptr);
  }
  if (idx < 13) return; // incomplete frame

  uint8_t nodeId = (uint8_t)fields[0];
  if (nodeId < 1 || nodeId > 254) return;

  // Find or allocate slot
  int slot = -1;
  int emptySlot = -1;
  for (int i = 0; i < MAX_LORA_NODES; i++) {
    if (loraNodes[i].valid && loraNodes[i].id == nodeId) { slot = i; break; }
    if (!loraNodes[i].valid && emptySlot < 0) emptySlot = i;
  }
  if (slot < 0) {
    if (emptySlot >= 0) slot = emptySlot;
    else return; // Table full
  }

  loraNodes[slot].valid = true;
  loraNodes[slot].id    = nodeId;
  loraNodes[slot].type  = (uint8_t)fields[1];
  loraNodes[slot].s1    = (int16_t)fields[2];
  loraNodes[slot].s2    = (int16_t)fields[3];
  loraNodes[slot].vbat  = (uint16_t)fields[4];
  loraNodes[slot].seq   = (uint16_t)fields[5];
  loraNodes[slot].star  = (uint8_t)fields[6];
  loraNodes[slot].drift = (int32_t)fields[7];
  loraNodes[slot].nodes = (uint8_t)fields[8];
  loraNodes[slot].sf    = (uint8_t)fields[9];
  loraNodes[slot].rssi  = (int16_t)fields[10];
  loraNodes[slot].snr   = (int8_t)fields[11];
  loraNodes[slot].adv   = (uint8_t)fields[12];
  loraNodes[slot].lastUpdate = millis();

  Serial.printf("[NODE_DATA] Node %u: type=%u s1=%d s2=%d vbat=%u rssi=%d adv=%u\r\n",
      nodeId, loraNodes[slot].type, loraNodes[slot].s1, loraNodes[slot].s2,
      loraNodes[slot].vbat, loraNodes[slot].rssi, loraNodes[slot].adv);
}

// Parse frame format: *SOURCE,DEVICE_ID,TYPE,VALUE#
SensorFrame parseFrame(char* frame) {
  SensorFrame data;
  data.valid = false;
  data.source[0] = '\0';
  data.deviceId[0] = '\0';
  data.type = 0;
  data.value = 0;

  int len = strlen(frame);
  if (len < 10 || frame[0] != '*' || frame[len-1] != '#') {
    return data;
  }

  char buf[128];
  if (len >= sizeof(buf)) return data;
  memcpy(buf, frame + 1, len - 2);
  buf[len - 2] = '\0';

  char* firstComma = strchr(buf, ',');
  if (firstComma == NULL) return data;
  
  data.valid = true;
  *firstComma = '\0';
  strncpy(data.source, buf, sizeof(data.source) - 1);
  data.source[sizeof(data.source) - 1] = '\0';

  char* deviceStart = firstComma + 1;
  char* secondComma = strchr(deviceStart, ',');
  if (secondComma != NULL) {
    *secondComma = '\0';
    strncpy(data.deviceId, deviceStart, sizeof(data.deviceId) - 1);
    data.deviceId[sizeof(data.deviceId) - 1] = '\0';

    char* typeStart = secondComma + 1;
    char* thirdComma = strchr(typeStart, ',');
    if (thirdComma != NULL) {
      *thirdComma = '\0';
      data.type = (uint8_t)strtol(typeStart, NULL, 10);
      data.value = strtol(thirdComma + 1, NULL, 10);
    } else {
      data.type = 0xFF;
      data.value = strtol(typeStart, NULL, 10);
    }
  }

  return data;
}

// Map device id to human-friendly name
void getDeviceName(const char* source, const char* deviceId, char* outName, size_t maxLen) {
  if (strcmp(source, "RAK") == 0) {
    if (strlen(deviceId) == 1) {
      char c = deviceId[0];
      switch (c) {
        case 'T': snprintf(outName, maxLen, "Tank"); return;
        case 'E': snprintf(outName, maxLen, "Engine"); return;
        case 'S': snprintf(outName, maxLen, "Canopus"); return;
        default: snprintf(outName, maxLen, "RAK-%s", deviceId); return;
      }
    }
    snprintf(outName, maxLen, "RAK-%s", deviceId);
  } else if (strcmp(source, "CAN") == 0) {
    if (strlen(deviceId) > 0 && isdigit((unsigned char)deviceId[0])) {
      snprintf(outName, maxLen, "Canopus Node %d", atoi(deviceId));
    } else {
      snprintf(outName, maxLen, "Canopus %s", deviceId);
    }
  } else if (strcmp(source, "WIND") == 0) {
    if (strlen(deviceId) > 0 && isdigit((unsigned char)deviceId[0])) {
      snprintf(outName, maxLen, "Weather Node %d", atoi(deviceId));
    } else {
      snprintf(outName, maxLen, "Weather %s", deviceId);
    }
  } else if (strcmp(source, "GAS") == 0) {
    if (strlen(deviceId) > 0 && isdigit((unsigned char)deviceId[0])) {
      snprintf(outName, maxLen, "GAS Node %d", atoi(deviceId));
    } else {
      snprintf(outName, maxLen, "GAS %s", deviceId);
    }
  } else {
    snprintf(outName, maxLen, "Device %s", deviceId);
  }
}

// Map type code to human-friendly name
void getTypeName(uint8_t type, char* outName, size_t maxLen) {
  switch (type) {
    case TYPE_TEMPERATURE: snprintf(outName, maxLen, "Temperature"); break;
    case TYPE_HUMIDITY: snprintf(outName, maxLen, "Humidity"); break;
    case TYPE_WIND_SPEED: snprintf(outName, maxLen, "Wind Speed"); break;
    case TYPE_WIND_DIRECTION: snprintf(outName, maxLen, "Wind Direction"); break;
    case TYPE_RAIN: snprintf(outName, maxLen, "Rain"); break;
    case TYPE_ENGINE_VOLT_1: snprintf(outName, maxLen, "Engine Volt 1"); break;
    case TYPE_ENGINE_VOLT_2: snprintf(outName, maxLen, "Engine Volt 2"); break;
    case TYPE_CANOPUS_VOLT: snprintf(outName, maxLen, "Canopus Volt"); break;
    case TYPE_TANK_LEVEL: snprintf(outName, maxLen, "Tank Level"); break;
    case TYPE_DIESEL_LEVEL: snprintf(outName, maxLen, "Diesel Level"); break;
    default: snprintf(outName, maxLen, "Unknown 0x%02X", type); break;
  }
}

// Process parsed sensor data
void processSensorData(SensorFrame data) {
  if (!data.valid) return;
  
  char deviceName[32];
  char typeName[32];
  getDeviceName(data.source, data.deviceId, deviceName, sizeof(deviceName));
  getTypeName(data.type, typeName, sizeof(typeName));
  
  Serial.printf("[Lora_Task] Source: %s, Device: %s, Type: %s (0x%02X), Value: %ld\r\n",
                data.source, deviceName, typeName, data.type, data.value);
  
  // Route data based on source and type
  if (strcmp(data.source, "RAK") == 0) {
    // Internal RAK sensors
    switch (data.type) {
      case TYPE_TANK_LEVEL:
        water_level = data.value / 10.0f;
        Serial.printf("%s Level: %ldmV\r\n", deviceName, data.value);
        break;
      case TYPE_ENGINE_VOLT_1:
        Serial.printf("%s Voltage (Port 1): %ld.%02ldV\r\n", deviceName, data.value / 100, data.value % 100);
        break;
      case TYPE_ENGINE_VOLT_2:
        Serial.printf("%s Voltage (Port 2): %ld.%02ldV\r\n", deviceName, data.value / 100, data.value % 100);
        break;
      case TYPE_CANOPUS_VOLT:
        Serial.printf("%s Voltage: %ld.%02ldV\r\n", deviceName, data.value / 100, data.value % 100);
        break;
    }
  } else if (strcmp(data.source, "CAN") == 0) {
    // Canopus board sensors
    if (strcmp(deviceName, "Canopus Node 1") == 0) {
      switch (data.type) {
        case TYPE_TEMPERATURE:
          temp1 = data.value / 100.0;
          Serial.printf("Temperature1: %.2f\r\n", temp1);
          break;
        case TYPE_HUMIDITY:
          hum1 = data.value / 100.0;
          Serial.printf("Humidity1: %.2f\r\n", hum1);
          break;
      }   
    } else if (strcmp(deviceName, "Canopus Node 2") == 0) {
      switch (data.type) {
        case TYPE_TEMPERATURE:
          temp2 = data.value / 100.0;
          Serial.printf("Temperature2: %.2f\r\n", temp2);
          break;
        case TYPE_HUMIDITY:
          hum2 = data.value / 100.0;
          Serial.printf("Humidity2: %.2f\r\n", hum2);
          break;
      }      
    }  
  } else if (strcmp(data.source, "WIND") == 0) {
    // Weather station sensors
    switch (data.type) {
      case TYPE_WIND_SPEED:
        wind = data.value / 100.0;
        Serial.printf("WIND1: %.2f\r\n", wind); 
        break;
      case TYPE_WIND_DIRECTION:
        Serial.printf("%s Wind Direction: %ld degrees\r\n", deviceName, data.value);
        break;
      case TYPE_RAIN:
        rain = data.value / 100.0f;
        Serial.printf("%s Rain: %ld.%02ldmm\r\n", deviceName, data.value / 100, data.value % 100);
        break;
    }

  } else if (strcmp(data.source, "DATA") == 0) {
    // Telemetry DATA frames forwarded from RAK Gateway
    int nodeId = atoi(data.deviceId);
    if (nodeId == 1) {
      switch (data.type) {
        case TYPE_TEMPERATURE:
          temp1 = data.value / 100.0;
          Serial.printf("Temperature1: %.2f\n", temp1);
          break;
        case TYPE_HUMIDITY:
          hum1 = data.value / 100.0;
          Serial.printf("Humidity1: %.2f\n", hum1);
          break;
      }
    } else if (nodeId == 2) {
      switch (data.type) {
        case TYPE_TEMPERATURE:
          temp2 = data.value / 100.0;
          Serial.printf("Temperature2: %.2f\n", temp2);
          break;
        case TYPE_HUMIDITY:
          hum2 = data.value / 100.0;
          Serial.printf("Humidity2: %.2f\n", hum2);
          break;
      }
    } else {
      // Other nodes
      switch (data.type) {
        case TYPE_WIND_SPEED:
          wind = data.value / 100.0;
          Serial.printf("Wind: %.2f\n", wind);
          break;
        case TYPE_TANK_LEVEL:
          water_level = data.value / 100.0f;
          Serial.printf("Water Level: %.2f\n", water_level);
          break;
      }
    }
  }
}

void Lora_Func(void* arg) {
  VOID SETUP() {
  }

  VOID LOOP() {
    mySerialEvent();

    // USB → RAK passthrough: forward *CMD frames from PC dashboard to RAK gateway
    static char usbCmdBuf[256];
    static int usbCmdLen = 0;
    while (Serial.available()) {
      char c = (char)Serial.read();
      if (c == '\n') {
        while (usbCmdLen > 0 && (usbCmdBuf[usbCmdLen - 1] == ' ' || usbCmdBuf[usbCmdLen - 1] == '\r' || usbCmdBuf[usbCmdLen - 1] == '\n')) {
          usbCmdBuf[--usbCmdLen] = '\0';
        }
        char* startPtr = usbCmdBuf;
        while (*startPtr == ' ') startPtr++;
        
        int finalLen = strlen(startPtr);
        if (finalLen > 0 && startPtr[0] == '*' && strchr(startPtr, '#') != NULL) {
          Serial2.println(startPtr);
          Serial.printf("[Lora_Task] USB→RAK: %s\n", startPtr);
        }
        usbCmdLen = 0;
        usbCmdBuf[0] = '\0';
      } else if (c != '\r') {
        if (usbCmdLen < 255) {
          usbCmdBuf[usbCmdLen++] = c;
          usbCmdBuf[usbCmdLen] = '\0';
        }
      }
    }
    
    if (stringComplete) {
      char workingBuffer[512];
      noInterrupts();
      strncpy(workingBuffer, inputBuffer, sizeof(workingBuffer) - 1);
      workingBuffer[sizeof(workingBuffer) - 1] = '\0';
      inputIndex = 0;
      inputBuffer[0] = '\0';
      stringComplete = false;
      interrupts();

      char* ptr = workingBuffer;
      while (*ptr == ' ' || *ptr == '\r' || *ptr == '\n') ptr++;

      char* hashPos;
      while ((hashPos = strchr(ptr, '#')) != NULL) {
        char nextChar = *(hashPos + 1);
        *(hashPos + 1) = '\0';

        char frame[256];
        strncpy(frame, ptr, sizeof(frame) - 1);
        frame[sizeof(frame) - 1] = '\0';

        *(hashPos + 1) = nextChar;

        ptr = hashPos + 1;
        while (*ptr == ' ' || *ptr == '\r' || *ptr == '\n') ptr++;

        if (strlen(frame) > 0) {
          Serial.printf("[Lora_Task] From RAK: %s\n", frame);

          char* starPos = strchr(frame, '*');
          if (starPos != NULL) {
            MQTTPublishRaw("CANOPUS/LORA", starPos);
          }
          if (strstr(frame, "[GW_STATUS]") != NULL) {
            MQTTPublishRaw("CANOPUS/STATUS", strstr(frame, "[GW_STATUS]"));
          }
          if (strstr(frame, "[GW_RX]") != NULL) {
            MQTTPublishRaw("CANOPUS/RX", strstr(frame, "[GW_RX]"));
          }

          if (strncmp(frame, "*NODE_DATA,", 11) == 0) {
            parseNodeData(frame);
          }

          if (strstr(frame, "[GW_LATE]") != NULL) {
            char* nodePos = strstr(frame, "Node");
            if (nodePos != NULL) {
              char* p = nodePos + 4;
              while (*p && !isdigit((unsigned char)*p)) p++;
              char nid[16] = {0};
              int nidIdx = 0;
              while (*p && isdigit((unsigned char)*p) && nidIdx < 15) {
                nid[nidIdx++] = *p++;
              }
              if (nidIdx > 0) {
                char* latePos = strstr(frame, "Late by");
                int late_ms = 0;
                if (latePos != NULL) {
                  char* q = latePos + 7;
                  while (*q && !isdigit((unsigned char)*q)) q++;
                  char lateS[16] = {0};
                  int lateIdx = 0;
                  while (*q && isdigit((unsigned char)*q) && lateIdx < 15) {
                    lateS[lateIdx++] = *q++;
                  }
                  late_ms = atoi(lateS);
                }
                char out[64];
                snprintf(out, sizeof(out), "*GW_LATE,%s,%d#", nid, late_ms);
                Serial.println(out);
                MQTTPublishRaw("CANOPUS/LORA", out);
              }
            }
          } else if (strstr(frame, "[GW_MISS]") != NULL) {
            char* nodePos = strstr(frame, "Node");
            if (nodePos != NULL) {
              char* p = nodePos + 4;
              while (*p && !isdigit((unsigned char)*p)) p++;
              char nid[16] = {0};
              int nidIdx = 0;
              while (*p && isdigit((unsigned char)*p) && nidIdx < 15) {
                nid[nidIdx++] = *p++;
              }
              if (nidIdx > 0) {
                char out[64];
                snprintf(out, sizeof(out), "*GW_MISS,%s#", nid);
                Serial.println(out);
                MQTTPublishRaw("CANOPUS/LORA", out);
              }
            }
          }

          char* driftIdx = strstr(frame, "drift=");
          if (driftIdx != NULL) {
            char* q = driftIdx + 6;
            while (*q && !isdigit((unsigned char)*q)) q++;
            char driftS[16] = {0};
            int driftIdx2 = 0;
            while (*q && isdigit((unsigned char)*q) && driftIdx2 < 15) {
              driftS[driftIdx2++] = *q++;
            }
            if (driftIdx2 > 0) {
              int drift_ms = atoi(driftS);
              if (drift_ms > DRIFT_LATE_THRESHOLD) {
                char* nodePos = NULL;
                char* tmpPos = strstr(frame, "Node");
                while (tmpPos != NULL && tmpPos < driftIdx) {
                  nodePos = tmpPos;
                  tmpPos = strstr(tmpPos + 1, "Node");
                }
                if (nodePos != NULL) {
                  char* p = nodePos + 4;
                  while (*p && !isdigit((unsigned char)*p)) p++;
                  char nid[16] = {0};
                  int nidIdx = 0;
                  while (*p && isdigit((unsigned char)*p) && nidIdx < 15) {
                    nid[nidIdx++] = *p++;
                  }
                  if (nidIdx > 0) {
                    char out[64];
                    snprintf(out, sizeof(out), "*GW_LATE,%s,%d#", nid, drift_ms);
                    Serial.println(out);
                    MQTTPublishRaw("CANOPUS/LORA", out);
                  }
                }
              }
            }
          }

          SensorFrame sensorData = parseFrame(frame);
          if (sensorData.valid && sensorData.type != 0xFF) {
            processSensorData(sensorData);
          }
        }
      }
      
      if (strlen(ptr) > 0) {
        char* searchPtr = ptr;
        char* idx;
        while ((idx = strstr(searchPtr, "[GW_LATE]")) != NULL) {
          char* nodePos = strstr(idx, "Node");
          if (nodePos != NULL) {
            char* p = nodePos + 4;
            while (*p && !isdigit((unsigned char)*p)) p++;
            char nid[16] = {0};
            int nidIdx = 0;
            while (*p && isdigit((unsigned char)*p) && nidIdx < 15) {
              nid[nidIdx++] = *p++;
            }
            char* latePos = strstr(idx, "Late by");
            int late_ms = 0;
            if (latePos != NULL) {
              char* q = latePos + 7;
              while (*q && !isdigit((unsigned char)*q)) q++;
              char lateS[16] = {0};
              int lateIdx = 0;
              while (*q && isdigit((unsigned char)*q) && lateIdx < 15) {
                lateS[lateIdx++] = *q++;
              }
              late_ms = atoi(lateS);
            }
            if (nidIdx > 0) {
              char out[64];
              snprintf(out, sizeof(out), "*GW_LATE,%s,%d#", nid, late_ms);
              Serial.println(out);
              MQTTPublishRaw("CANOPUS/LORA", out);
            }
          }
          searchPtr = idx + 9;
        }

        searchPtr = ptr;
        while ((idx = strstr(searchPtr, "[GW_MISS]")) != NULL) {
          char* nodePos = strstr(idx, "Node");
          if (nodePos != NULL) {
            char* p = nodePos + 4;
            while (*p && !isdigit((unsigned char)*p)) p++;
            char nid[16] = {0};
            int nidIdx = 0;
            while (*p && isdigit((unsigned char)*p) && nidIdx < 15) {
              nid[nidIdx++] = *p++;
            }
            if (nidIdx > 0) {
              char out[64];
              snprintf(out, sizeof(out), "*GW_MISS,%s#", nid);
              Serial.println(out);
              MQTTPublishRaw("CANOPUS/LORA", out);
            }
          }
          searchPtr = idx + 9;
        }

        searchPtr = ptr;
        while ((idx = strstr(searchPtr, "drift=")) != NULL) {
          char* q = idx + 6;
          while (*q && !isdigit((unsigned char)*q)) q++;
          char driftS[16] = {0};
          int driftIdx2 = 0;
          while (*q && isdigit((unsigned char)*q) && driftIdx2 < 15) {
            driftS[driftIdx2++] = *q++;
          }
          if (driftIdx2 > 0) {
            int drift_ms = atoi(driftS);
            if (drift_ms > DRIFT_LATE_THRESHOLD) {
              char* lineStart = idx;
              while (lineStart > ptr && *(lineStart - 1) != '\n') lineStart--;
              char* nodePos = strstr(lineStart, "Node");
              if (nodePos != NULL && nodePos < idx) {
                char* p = nodePos + 4;
                while (*p && !isdigit((unsigned char)*p)) p++;
                char nid[16] = {0};
                int nidIdx = 0;
                while (*p && isdigit((unsigned char)*p) && nidIdx < 15) {
                  nid[nidIdx++] = *p++;
                }
                if (nidIdx > 0) {
                  char out[64];
                  snprintf(out, sizeof(out), "*GW_LATE,%s,%d#", nid, drift_ms);
                  Serial.println(out);
                  MQTTPublishRaw("CANOPUS/LORA", out);
                }
              }
            }
          }
          searchPtr = q;
        }

        Serial.println(ptr);
      }
    }
    
    static uint32_t lastLedToggle = 0;
    if (millis() - lastLedToggle >= 250) {
      lastLedToggle = millis();
      Led_show_status();
    }
    DELAY(50);
  }
}
