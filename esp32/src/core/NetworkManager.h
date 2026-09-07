#pragma once
// ============================================================================
//  NetworkManager — WiFi + MQTT(TLS) + LWT + backoff + NTP.
//  Not: PubSubClient publish daima QoS 0. "Yeni abone son değeri görsün" ihtiyacı
//  retained flag ile karşılanır (state / status / telemetry retained yayınlanır).
// ============================================================================

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <functional>
#include "Logger.h"

class NetworkManager {
public:
    using MessageCallback = std::function<void(const String& topic, const String& payload)>;
    using ConnectCallback = std::function<void()>;

    NetworkManager(Logger& logger,
                   const char* ssid, const char* pass,
                   const char* host, uint16_t port,
                   const char* user, const char* mqttPass,
                   const char* clientPrefix);

    void begin();
    void loop();

    bool connected();
    bool publish(const char* topic, const char* payload, bool retained = false);
    bool publishRaw(const char* topic, const uint8_t* payload, unsigned int len, bool retained = false);
    bool subscribe(const char* topic, uint8_t qos = 1);

    void onMessage(MessageCallback cb)   { m_onMessage = cb; }
    void onConnected(ConnectCallback cb) { m_onConnected = cb; }

    void     setBufferSize(uint16_t bytes);
    uint32_t epochNow();                 // 0 = NTP henüz senkron değil
    int      rssi()  { return WiFi.RSSI(); }
    String   ip()    { return WiFi.localIP().toString(); }
    bool     wifiUp(){ return WiFi.status() == WL_CONNECTED; }

private:
    void ensureWifi();
    void ensureMqtt();
    void onRawMessage(char* topic, byte* payload, unsigned int length);

    Logger&          m_log;
    const char*      m_ssid;
    const char*      m_pass;
    const char*      m_host;
    uint16_t         m_port;
    const char*      m_user;
    const char*      m_mqttPass;
    const char*      m_clientPrefix;

    WiFiClientSecure m_tls;
    PubSubClient     m_mqtt;
    MessageCallback  m_onMessage   = nullptr;
    ConnectCallback  m_onConnected = nullptr;

    uint32_t m_lastWifiTry  = 0;
    uint32_t m_lastMqttTry  = 0;
    uint32_t m_mqttBackoff  = 0;
    bool     m_ntpStarted   = false;
    bool     m_wasConnected = false;
};
