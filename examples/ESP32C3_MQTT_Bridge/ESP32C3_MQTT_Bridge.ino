#include <WiFi.h>
#include <PubSubClient.h>

// WiFi Configuration - Replace with your network details
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// MQTT Broker Configuration - Replace with your MQTT server details
const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;
const char* mqtt_user = "";     // Leave empty if not required
const char* mqtt_pass = "";     // Leave empty if not required

WiFiClient espClient;
PubSubClient client(espClient);

// UART1 Pins for communicating with RAK3172
#define RX_PIN 4
#define TX_PIN 5

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 20) {
    delay(500);
    Serial.print(".");
    retries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.println("WiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWiFi connection failed! Will retry in the main loop.");
  }
}

void reconnect() {
  // Loop until we're reconnected to MQTT
  if (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP32C3Client-";
    clientId += String(random(0xffff), HEX);
    
    // Attempt to connect
    bool connected = false;
    if (strlen(mqtt_user) > 0) {
      connected = client.connect(clientId.c_str(), mqtt_user, mqtt_pass);
    } else {
      connected = client.connect(clientId.c_str());
    }

    if (connected) {
      Serial.println("connected to MQTT broker!");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again later.");
    }
  }
}

void setup() {
  // Serial0 (USB-C) for debugging and flashing
  Serial.begin(115200);
  Serial.println("--- ESP32-C3 MQTT Serial Bridge Starting ---");

  // Serial1 (connected to RAK3172 PA2/PA3 / Serial)
  Serial1.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN);
  Serial.println("Serial1 (RAK3172 connection) initialized at 115200 baud.");

  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  randomSeed(micros());
}

void loop() {
  // Maintain WiFi Connection
  if (WiFi.status() != WL_CONNECTED) {
    static unsigned long lastWiFiCheck = 0;
    if (millis() - lastWiFiCheck > 10000) {
      lastWiFiCheck = millis();
      Serial.println("WiFi disconnected. Reconnecting...");
      WiFi.begin(ssid, password);
    }
  }

  // Maintain MQTT Connection
  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) {
      static unsigned long lastMQTTCheck = 0;
      if (millis() - lastMQTTCheck > 5000) {
        lastMQTTCheck = millis();
        reconnect();
      }
    }
  }

  client.loop();

  // Process incoming data from RAK3172 on Serial1
  if (Serial1.available()) {
    String command = Serial1.readStringUntil('\n');
    command.trim(); // Remove any carriage return \r or trailing whitespace
    
    if (command.length() > 0) {
      Serial.print("Received from RAK3172: ");
      Serial.println(command);

      // Protocol check: starts with "PUB|"
      if (command.startsWith("PUB|")) {
        int firstDelim = command.indexOf('|');
        int secondDelim = command.indexOf('|', firstDelim + 1);

        if (secondDelim != -1) {
          String topic = command.substring(firstDelim + 1, secondDelim);
          String payload = command.substring(secondDelim + 1);

          Serial.printf("Parsing command: Topic='%s', Payload='%s'\r\n", topic.c_str(), payload.c_str());

          if (WiFi.status() == WL_CONNECTED && client.connected()) {
            if (client.publish(topic.c_str(), payload.c_str())) {
              Serial.println("Publish Successful!");
              Serial1.println("MQTT_OK"); // Acknowledge to RAK3172
            } else {
              Serial.println("Publish Failed!");
              Serial1.println("MQTT_ERR_PUB");
            }
          } else {
            Serial.println("Cannot publish: WiFi or MQTT not connected.");
            Serial1.println("MQTT_ERR_CONN");
          }
        } else {
          Serial.println("Invalid format. Missing second '|' delimiter.");
          Serial1.println("MQTT_ERR_FORMAT");
        }
      } else {
        Serial.println("Unknown command prefix.");
        Serial1.println("MQTT_ERR_UNKNOWN");
      }
    }
  }
}
