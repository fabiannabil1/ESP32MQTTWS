#ifndef ESP32MQTTWS_H
#define ESP32MQTTWS_H

#include <WiFi.h>
#include <WiFiClientSecure.h>

#define ESP32MQTTWS_VERSION "1.2.4"
#define ESP32MQTTWS_AUTHOR  "Fabian Nabil"


class ESP32MQTTWS {
  public:
    String mqttUser;
    String mqttPass;
    std::vector<String> subscribedTopics;
    typedef void (*MessageCallback)(String topic, String payload);

    ESP32MQTTWS();
    void begin(const char* ssid, const char* password);
    bool connectBroker(const char* broker, int port, const char* ws_path);
    void mqttConnect(const char* username, const char* password);
    void mqttSubscribe(const char* topic);
    void mqttPublish(const char* topic, const char* message);
    void loop();
    void printLibraryInfo();
    void onMessage(MessageCallback cb);

  private:
    WiFiClientSecure client;
    String wsPath;
    String brokerHost;
    MessageCallback messageCallback = nullptr;

    unsigned long lastPing = 0;   // <<< biar tidak error di loop()
    
    size_t encodeRemainingLength(uint8_t* buf, size_t len);
    bool websocketHandshake();
    void sendWSFrame(const uint8_t* data, size_t len);
    void readWSMQTT();
    void handleMQTTPacket(uint8_t* payload, size_t len);
    int decodeVarByteInt(const uint8_t* buf, size_t bufLen, int &value); // <<< Tambah deklarasi
};

#endif
