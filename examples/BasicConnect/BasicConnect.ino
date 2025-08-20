#include <ESP32MQTTWS.h>

ESP32MQTTWS mqtt;

void onConnected() {
  Serial.println("MQTT connected callback");
  mqtt.mqttSubscribe("test/gpio");
  mqtt.mqttPublish("test/gpio", "ESP connected");
}

void onMessage(String topic, String payload) {
  Serial.println("Message came: " + topic + " -> " + payload);
  if (topic == "test/gpio") {
    if (payload == "ON") digitalWrite(2, HIGH);
    if (payload == "OFF") digitalWrite(2, LOW);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(2, OUTPUT);
  mqtt.printLibraryInfo();
  mqtt.begin("Redmi 9T", "12345678");
  if (!mqtt.connectBroker("your_url.com", 443, "/mqtt")) {
    Serial.println("Broker connect/handshake failed");
    while (1) delay(1000);
  }
  mqtt.onConnect(onConnected);
  mqtt.onMessage(onMessage);
  mqtt.mqttConnect("username", "password");
  delay(1000);
  mqtt.mqttSubscribe("test/gpio");
  delay(200);
  mqtt.mqttPublish("test/gpio", "Hello from ESP32");
}

void loop() {
  mqtt.loop();
}
