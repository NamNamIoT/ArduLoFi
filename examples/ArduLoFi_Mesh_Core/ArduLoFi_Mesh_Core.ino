#include <ArduLoFi.h>

// Simple Mesh Packet Structure
// Byte 0: Hop Limit (TTL) - packet dies when TTL reaches 0
// Byte 1: Origin Node ID (LSB of Chip ID)
// Byte 2: Target Node ID (LSB of Chip ID, 0xFF for Broadcast)
// Byte 3: Message ID (for duplicate packet detection)
// Bytes 4-63: Payload (ASCII data)
struct MeshPacket {
  uint8_t hop_limit;
  uint8_t origin_id;
  uint8_t target_id;
  uint8_t msg_id;
  char payload[60];
};

uint8_t myNodeID = 0;
uint8_t lastRxMsgID = 0xFF;
bool txDone = false;

// Packet Buffer for Forwarding
MeshPacket packetToForward;
bool hasPacketToForward = false;
unsigned long forwardTime = 0;

void recv_cb(rui_lora_p2p_recv_t data) {
  ArduLoFi.setLed(ARDULOFI_LED_RX, true);
  
  if (data.BufferSize < 4) {
    Serial.println("[MESH] Packet too small. Ignored.");
    ArduLoFi.setLed(ARDULOFI_LED_RX, false);
    return;
  }

  // Parse packet from buffer
  MeshPacket rxPacket;
  memcpy(&rxPacket, data.Buffer, sizeof(MeshPacket));
  
  Serial.printf("[MESH RX] Hop Limit: %d | Origin: 0x%02X | Target: 0x%02X | MsgID: %d\r\n", 
                rxPacket.hop_limit, rxPacket.origin_id, rxPacket.target_id, rxPacket.msg_id);
  Serial.printf("[MESH RX] Payload: %s\r\n", rxPacket.payload);

  // Duplicate Check
  if (rxPacket.msg_id == lastRxMsgID && rxPacket.origin_id != myNodeID) {
    Serial.println("[MESH] Duplicate packet. Dropping.");
    ArduLoFi.setLed(ARDULOFI_LED_RX, false);
    return;
  }
  lastRxMsgID = rxPacket.msg_id;

  // Check if packet is meant for us or broadcast
  if (rxPacket.target_id == myNodeID || rxPacket.target_id == 0xFF) {
    Serial.printf("[MESH] Processing Message: '%s'\r\n", rxPacket.payload);
    
    // MQTT Bridge integration: if packet is for us, publish it to cloud MQTT via ESP32-C3!
    String topic = "ardulofi/mesh/node_" + String(rxPacket.origin_id);
    String msg = String(rxPacket.payload);
    ArduLoFi.publishMQTT(topic, msg);
  }

  // Forwarding check: if target is not us and hop limit > 0, we forward it!
  if (rxPacket.target_id != myNodeID && rxPacket.hop_limit > 1) {
    // Stage packet for forwarding with decremented hop limit
    packetToForward = rxPacket;
    packetToForward.hop_limit--;
    hasPacketToForward = true;
    
    // Add a randomized delay (200ms - 800ms) to avoid simultaneous collision
    forwardTime = millis() + random(200, 800);
    Serial.printf("[MESH] Staging forward in %lu ms...\r\n", forwardTime - millis());
  }

  ArduLoFi.setLed(ARDULOFI_LED_RX, false);
}

void send_cb(void) {
  txDone = true;
  Serial.println("[MESH TX] Transmission complete. Listening...");
  
  // Re-enter receive mode
  api.lora.precv(65534);
}

void setup() {
  ArduLoFi.begin();
  
  // Power up ESP32-C3 link
  ArduLoFi.beginESP32(115200);

  Serial.println("--- ArduLoFi LoRa Mesh Core Gateway Starting ---");

  // Calculate a unique 1-byte Node ID from unique Chip ID LSB
  String chipID = ArduLoFi.getChipID();
  uint32_t rawID = strtoul(chipID.c_str(), NULL, 16);
  myNodeID = (uint8_t)(rawID & 0xFF);
  Serial.printf("Mesh Node ID configured: 0x%02X\r\n", myNodeID);

  // Configure LoRa P2P parameters: Default 868MHz, SF7, BW125
  if (ArduLoFi.configLoraP2P(868000000.0, 7, 0, 0, 8, 22)) {
    Serial.println("[OK] LoRa P2P Core configured.");
  } else {
    Serial.println("[ERROR] Failed to configure LoRa P2P.");
  }

  // Register RUI3 callbacks
  api.lora.registerPRecvCallback(recv_cb);
  api.lora.registerPSendCallback(send_cb);

  // Set LoRa to receive mode
  api.lora.precv(65534);
  randomSeed(micros());
}

void loop() {
  // Handle staged forwarding packets asynchronously
  if (hasPacketToForward && millis() >= forwardTime) {
    hasPacketToForward = false;
    
    Serial.println("[MESH] Forwarding packet now...");
    
    // Stop RX before sending
    api.lora.precv(0);
    
    ArduLoFi.setLed(ARDULOFI_LED_TX, true);
    bool result = ArduLoFi.sendP2P((uint8_t*)&packetToForward, sizeof(MeshPacket));
    ArduLoFi.setLed(ARDULOFI_LED_TX, false);
    
    if (result) {
      Serial.println("[MESH] Packet forwarded successfully.");
    } else {
      Serial.println("[MESH] Forwarding failed. Resuming RX...");
      api.lora.precv(65534);
    }
  }

  // Example: Periodically broadcast a mesh message (e.g. every 30 seconds)
  static unsigned long lastBroadcast = 0;
  if (millis() - lastBroadcast >= 30000) {
    lastBroadcast = millis();
    
    MeshPacket ping;
    ping.hop_limit = 4; // Max 4 hops
    ping.origin_id = myNodeID;
    ping.target_id = 0xFF; // Broadcast
    ping.msg_id = (uint8_t)random(0, 255);
    
    float battery_V = ArduLoFi.readBattery_mV() / 1000.0;
    sprintf(ping.payload, "Hi, I am core node 0x%02X. Bat: %0.2fV", myNodeID, battery_V);
    
    Serial.println("[MESH] Broadcasting periodic status ping...");
    api.lora.precv(0); // Stop RX
    
    ArduLoFi.setLed(ARDULOFI_LED_TX, true);
    ArduLoFi.sendP2P((uint8_t*)&ping, sizeof(MeshPacket));
    ArduLoFi.setLed(ARDULOFI_LED_TX, false);
  }
}
