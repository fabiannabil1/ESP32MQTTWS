# ESP32MQTTWS

**ESP32MQTTWS** is a lightweight MQTT over WebSockets library for ESP32, designed to connect to MQTT brokers that are tunneled through **Cloudflare (free tier)** or similar WebSocket-only endpoints.

Unlike traditional MQTT libraries that connect via raw TCP (1883) or TLS (8883), this library implements **native WebSocket framing** and **MQTT protocol packets** directly, allowing ESP32 to communicate with brokers exposed only through `wss://` endpoints.


## ✨ Features

* Connect ESP32 to MQTT brokers via **WebSocket Secure (wss\://)**.
* Minimal dependency (built directly on `WiFiClientSecure`).
* Handles **MQTT CONNECT, SUBSCRIBE, PUBLISH, PINGREQ/PINGRESP** packets.
* Auto-reconnect with re-subscription.
* Custom **onMessage() callback** for receiving MQTT messages.


## 🚀 Usage

### 1. Include the library

```cpp
#include "ESP32MQTTWS.h"

ESP32MQTTWS mqtt;
```

---

### 2. Setup WiFi

```cpp
mqtt.begin("YourWiFiSSID", "YourWiFiPassword");
```

---

### 3. Connect to Broker (via WebSockets)

```cpp
if (mqtt.connectBroker("your-broker.com", 443, "/mqtt")) {
    Serial.println("Broker connected via WebSocket!");
    mqtt.mqttConnect("username", "password");
} else {
    Serial.println("Failed to connect to broker.");
}
```

* **Broker** → your MQTT endpoint (Cloudflare-proxied or native WS).
* **Port** → usually `443` for secure WebSockets.
* **WS Path** → e.g., `/mqtt` or `/ws`.


### 4. Subscribe to a topic

```cpp
mqtt.mqttSubscribe("test/topic");
```


### 5. Publish a message

```cpp
mqtt.mqttPublish("test/topic", "Hello from ESP32 over WebSockets!");
```

### 6. Handle incoming messages

```cpp
mqtt.onMessage([](String topic, String msg) {
    Serial.printf("Received: %s -> %s\n", topic.c_str(), msg.c_str());
});
```


### 7. Loop

```cpp
void loop() {
    mqtt.loop(); // keep connection alive and process packets
}
```


## 📌 Example Full Code

```cpp
#include "ESP32MQTTWS.h"

ESP32MQTTWS mqtt;

void setup() {
  Serial.begin(115200);

  // Connect WiFi
  mqtt.begin("YourWiFiSSID", "YourWiFiPassword");

  // Connect to Broker
  if (mqtt.connectBroker("your-broker.com", 443, "/mqtt")) {
    mqtt.mqttConnect("username", "password");
    mqtt.mqttSubscribe("test/topic");
  }

  // Handle messages
  mqtt.onMessage([](String topic, String msg) {
    Serial.printf("[MQTT] %s -> %s\n", topic.c_str(), msg.c_str());
  });
}

void loop() {
  mqtt.loop();
}
```

## ⚠️ Notes

* Cloudflare free tier only supports WebSocket tunneling, not raw MQTT → this library solves that.
* Recommended to use **secure port 443 (WSS)** instead of `80 (WS)`.
* Still experimental → some MQTT features like QoS1/2 are not fully implemented.
