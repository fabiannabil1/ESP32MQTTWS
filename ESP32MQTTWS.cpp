#include "ESP32MQTTWS.h"

ESP32MQTTWS::ESP32MQTTWS() {}

void ESP32MQTTWS::printLibraryInfo() {
  Serial.println("====================");
  Serial.println(" Library : ESP32MQTTWS");
  Serial.println(String(" Version : ") + ESP32MQTTWS_VERSION);
  Serial.println(String(" Author  : ") + ESP32MQTTWS_AUTHOR);
  Serial.println("====================");
}

void ESP32MQTTWS::begin(const char* ssid, const char* password) {
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
}

bool ESP32MQTTWS::connectBroker(const char* broker, int port, const char* ws_path) {
  client.setInsecure();
  brokerHost = broker;
  wsPath = ws_path;
  Serial.print("Connecting to broker...");
  if (!client.connect(broker, port)) {
    Serial.println(" failed!");
    return false;
  }
  Serial.println(" connected!");
  return websocketHandshake();
}

size_t ESP32MQTTWS::encodeRemainingLength(uint8_t* buf, size_t len) {
  size_t i = 0;
  do {
    uint8_t encodedByte = len % 128;
    len /= 128;
    if (len > 0) encodedByte |= 128;
    buf[i++] = encodedByte;
  } while (len > 0);
  return i;
}

bool ESP32MQTTWS::websocketHandshake() {
  // simple static key (ok) — could be improved to random base64 key
  String key = "x3JJHMbDL1EzLkh9GBhXDw=="; // acceptable but can be randomized

  client.print(String("GET ") + wsPath + " HTTP/1.1\r\n" +
               "Host: " + brokerHost + "\r\n" +
               "Upgrade: websocket\r\n" +
               "Connection: Upgrade\r\n" +
               "Sec-WebSocket-Protocol: mqtt\r\n" +
               "Origin: https://" + brokerHost + "\r\n" +
               "Sec-WebSocket-Key: " + key + "\r\n" +
               "Sec-WebSocket-Version: 13\r\n\r\n");

  bool status101 = false;
  Serial.println("=== WebSocket Handshake Response ===");
  unsigned long start = millis();
  while (client.connected() && (millis() - start) < 5000) {
    if (!client.available()) {
      delay(10);
      continue;
    }
    String line = client.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) break;
    Serial.println(line);
    if (line.startsWith("HTTP/1.1 101")) status101 = true;
  }
  Serial.println("====================================");
  return status101;
}

void ESP32MQTTWS::sendWSFrame(const uint8_t* data, size_t len) {
  uint8_t header[14];
  size_t headerLen = 0;

  header[0] = 0x82; // FIN + binary
  if (len <= 125) {
    header[1] = 0x80 | (uint8_t)len; // mask bit + len
    headerLen = 2;
  } else if (len < 65536) {
    header[1] = 0x80 | 126;
    header[2] = (len >> 8) & 0xFF;
    header[3] = len & 0xFF;
    headerLen = 4;
  } else {
    header[1] = 0x80 | 127;
    // 8-byte length (we won't generally use for MQTT small packets)
    for (int i = 0; i < 8; ++i) header[2 + i] = (len >> (56 - i * 8)) & 0xFF;
    headerLen = 10;
  }

  // Mask key
  uint8_t mask[4];
  for (int i = 0; i < 4; i++) mask[i] = random(0, 256);

  memcpy(&header[headerLen], mask, 4);
  headerLen += 4;

  client.write(header, headerLen);

  // send masked payload
  for (size_t i = 0; i < len; i++) {
    uint8_t m = data[i] ^ mask[i % 4];
    client.write(&m, 1);
  }
}

void ESP32MQTTWS::mqttConnect(const char* username, const char* password) {
  mqttUser = username;
  mqttPass = password;

  uint8_t packet[512];
  size_t idx = 0;

  packet[idx++] = 0x10; // CONNECT

  // Protocol Name "MQTT"
  packet[idx++] = 0x00; packet[idx++] = 0x04;
  packet[idx++] = 'M'; packet[idx++] = 'Q'; packet[idx++] = 'T'; packet[idx++] = 'T';
  packet[idx++] = 0x04; // Protocol level
  packet[idx++] = 0xC2; // User name + Password + Clean session
  packet[idx++] = 0x00; packet[idx++] = 0x3C; // Keep Alive 60s

  // Client ID empty
  packet[idx++] = 0x00; packet[idx++] = 0x00;

  // username
  uint16_t ulen = (uint16_t)strlen(username);
  packet[idx++] = (uint8_t)(ulen >> 8); packet[idx++] = (uint8_t)(ulen & 0xFF);
  memcpy(&packet[idx], username, ulen); idx += ulen;

  // password
  uint16_t plen = (uint16_t)strlen(password);
  packet[idx++] = (uint8_t)(plen >> 8); packet[idx++] = (uint8_t)(plen & 0xFF);
  memcpy(&packet[idx], password, plen); idx += plen;

  // encode Remaining Length and shift payload
  uint8_t rlBuf[4];
  size_t rlSize = encodeRemainingLength(rlBuf, idx - 1);
  memmove(&packet[1 + rlSize], &packet[1], idx - 1);
  memcpy(&packet[1], rlBuf, rlSize);
  idx = 1 + rlSize + (idx - 1);

  sendWSFrame(packet, idx);
}

void ESP32MQTTWS::mqttSubscribe(const char* topic) {
  uint8_t packet[512];
  size_t idx = 0;
  packet[idx++] = 0x82; // SUBSCRIBE (with reserved bits for SUBSCRIBE)
  // packet id will be placed in variable header
  packet[idx++] = 0x00; packet[idx++] = 0x01; // Packet id 1

  uint16_t tlen = (uint16_t)strlen(topic);
  packet[idx++] = (tlen >> 8) & 0xFF; packet[idx++] = tlen & 0xFF;
  memcpy(&packet[idx], topic, tlen); idx += tlen;
  packet[idx++] = 0x00; // QoS 0

  // insert Remaining Length properly
  uint8_t rlBuf[4];
  size_t rlSize = encodeRemainingLength(rlBuf, idx - 1);
  memmove(&packet[1 + rlSize], &packet[1], idx - 1);
  memcpy(&packet[1], rlBuf, rlSize);
  idx = 1 + rlSize + (idx - 1);

  sendWSFrame(packet, idx);
  subscribedTopics.push_back(topic);
}

void ESP32MQTTWS::mqttPublish(const char* topic, const char* message) {
  uint8_t packet[1024];
  size_t idx = 0;
  packet[idx++] = 0x30; // PUBLISH QoS0

  uint16_t tlen = (uint16_t)strlen(topic);
  packet[idx++] = (tlen >> 8) & 0xFF; packet[idx++] = tlen & 0xFF;
  memcpy(&packet[idx], topic, tlen); idx += tlen;

  size_t mlen = strlen(message);
  memcpy(&packet[idx], message, mlen); idx += mlen;

  // insert Remaining Length
  uint8_t rlBuf[4];
  size_t rlSize = encodeRemainingLength(rlBuf, idx - 1);
  memmove(&packet[1 + rlSize], &packet[1], idx - 1);
  memcpy(&packet[1], rlBuf, rlSize);
  idx = 1 + rlSize + (idx - 1);

  sendWSFrame(packet, idx);
}

// decode MQTT Variable Byte Integer (remaining length)
// returns number of bytes read (1..4); sets value
int ESP32MQTTWS::decodeVarByteInt(const uint8_t* buf, size_t bufLen, int &value) {
  value = 0;
  int multiplier = 1;
  int i = 0;
  uint8_t encodedByte;
  do {
    if (i >= (int)bufLen) return -1;
    encodedByte = buf[i++];
    value += (encodedByte & 127) * multiplier;
    multiplier *= 128;
    if (multiplier > 128*128*128) return -1; // malformed
  } while ((encodedByte & 128) != 0);
  return i;
}

void ESP32MQTTWS::handleMQTTPacket(uint8_t* payload, size_t len) {
  uint8_t packetType = payload[0] >> 4;

  if (packetType == 3) { // PUBLISH
    uint16_t topicLen = (payload[2] << 8) | payload[3];
    String topic = "";
    for (int i = 0; i < topicLen; i++) topic += (char)payload[4 + i];

    int payloadStart = 4 + topicLen;
    String msg = "";
    for (int i = payloadStart; i < len; i++) msg += (char)payload[i];

    Serial.printf("Message came: %s -> %s\n", topic.c_str(), msg.c_str());

    if (messageCallback) {
      messageCallback(topic, msg);
    }
  }
  else if (packetType == 2) { // CONNACK
    Serial.printf("[MQTT] CONNACK return code: %d\n", payload[3]);
  }
  else if (packetType == 8) { // PINGREQ dari broker
    // Kirim PINGRESP balik
    uint8_t resp[2];
    resp[0] = 0xD0; // PINGRESP
    resp[1] = 0x00;
    sendWSFrame(resp, 2);
    // Optional: catat waktu terakhir ping
    // lastPing = millis();
  }
  else if (packetType == 9) { // SUBACK
    Serial.println("[MQTT] SUBACK received");
  }
  else if (packetType == 13) { // PINGRESP
    Serial.println("[MQTT] PINGRESP received");
  }
  else {
    Serial.printf("[MQTT] Unhandled packet type: 0x%02X (len=%d)\n", packetType, len);
  }
}


void ESP32MQTTWS::readWSMQTT() {
  if (!client.available()) return;

  uint8_t opcode = client.read();
  uint8_t lenByte = client.read();
  bool masked = lenByte & 0x80;
  size_t payloadLen = lenByte & 0x7F;

  if (payloadLen == 126) {
    payloadLen = (client.read() << 8) | client.read();
  }

  uint8_t mask[4];
  if (masked) {
    for (int i = 0; i < 4; i++) mask[i] = client.read();
  }

  uint8_t payload[256];
  for (size_t i = 0; i < payloadLen; i++) {
    uint8_t b = client.read();
    if (masked) b ^= mask[i % 4];
    payload[i] = b;
  }

  // Kirim payload MQTT ke parser
  handleMQTTPacket(payload, payloadLen);
}

void ESP32MQTTWS::loop() {
  readWSMQTT();

  // ping keep-alive every 30s
  if (millis() - lastPing > 30000) {
    uint8_t ping[2] = {0xC0, 0x00}; // MQTT PINGREQ
    sendWSFrame(ping, 2);
    lastPing = millis();
  }

  // auto reconnect if underlying TLS disconnected
  static unsigned long lastReconnectAttempt = 0;

  if (!client.connected() && (millis() - lastReconnectAttempt > 1000)) {
      lastReconnectAttempt = millis();
      Serial.println("Reconnecting to broker...");
      if (client.connect(brokerHost.c_str(), 443) && websocketHandshake()) {
          Serial.println("Reconnected (handshake OK)");
          mqttConnect(mqttUser.c_str(), mqttPass.c_str());
          for (auto &t : subscribedTopics) {
              mqttSubscribe(t.c_str());
          }
      } else {
          Serial.println("Reconnect failed");
      }
  } 
}

void ESP32MQTTWS::onMessage(MessageCallback cb) {
  messageCallback = cb;
}
