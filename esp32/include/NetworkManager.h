#pragma once

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <functional>

class NetworkManager {
public:
    using MessageCallback = std::function<void(const String& topic, const String& payload)>;
    using ConnectCallback = std::function<void()>; // Bağlantı kurulunca tetiklenecek callback

    NetworkManager(const char* ssid, const char* pass,
                   const char* mqttServer, uint16_t mqttPort,
                   const char* mqttUser, const char* mqttPass);

    void begin();
    void update();
    bool subscribe(const char* topic);
    bool publish(const char* topic, const char* payload);
    void onMessage(MessageCallback callback);
    void onConnected(ConnectCallback callback); // main'den abone listesini almak için

private:
    const char* m_ssid;
    const char* m_pass;
    const char* m_mqttServer;
    uint16_t    m_mqttPort;
    const char* m_mqttUser;
    const char* m_mqttPass;

    WiFiClientSecure m_secureClient;
    PubSubClient m_mqttClient;
    MessageCallback m_userCallback;
    ConnectCallback m_connectCallback; // Kaydedilen bağlantı callback'i

    unsigned long m_lastReconnectAttempt;
    unsigned long m_lastWiFiReconnectAttempt; 

    void setupWiFi();
    void reconnectMQTT();
    void internalMqttCallback(char* topic, byte* payload, unsigned int length);
};