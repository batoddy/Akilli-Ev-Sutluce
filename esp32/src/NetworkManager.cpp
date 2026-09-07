#include "NetworkManager.h"

NetworkManager::NetworkManager(const char* ssid, const char* pass,
                               const char* mqttServer, uint16_t mqttPort,
                               const char* mqttUser, const char* mqttPass)
    : m_ssid(ssid), m_pass(pass),
      m_mqttServer(mqttServer), m_mqttPort(mqttPort),
      m_mqttUser(mqttUser), m_mqttPass(mqttPass),
      m_mqttClient(m_secureClient),
      m_connectCallback(nullptr),
      m_lastReconnectAttempt(0) {}

void NetworkManager::begin() 
{
    setupWiFi();

    m_secureClient.setInsecure();
    m_mqttClient.setServer(m_mqttServer, m_mqttPort);
    
    m_mqttClient.setCallback([this](char* topic, byte* payload, unsigned int length) {
        this->internalMqttCallback(topic, payload, length);
    });
}

void NetworkManager::setupWiFi() 
{
    WiFi.mode(WIFI_STA);
    WiFi.begin(m_ssid, m_pass);
    Serial.print("WiFi baglantisi baslatildi: ");
    Serial.println(m_ssid);
}

void NetworkManager::reconnectMQTT() 
{
    unsigned long now = millis();
    if (now - m_lastReconnectAttempt > 5000) {
        m_lastReconnectAttempt = now;
        Serial.print("HiveMQ baglantisi deneniyor...");

        String clientId = "ESP32Node-" + String(random(0xffff), HEX);
        if (m_mqttClient.connect(clientId.c_str(), m_mqttUser, m_mqttPass)) 
        {
            Serial.println(" Baglandi!");

            if (m_connectCallback) {
                m_connectCallback();
            }
        } else {
            Serial.printf(" Basarisiz (rc=%d)\n", m_mqttClient.state());
        }
    }
}

void NetworkManager::update() 
{
    if (WiFi.status() != WL_CONNECTED) 
    {
        unsigned long now = millis();
        if (now - m_lastWiFiReconnectAttempt > 5000) 
        {
            m_lastWiFiReconnectAttempt = now;
            Serial.println("WiFi baglantisi yok. Tekrar deneniyor...");
            WiFi.disconnect();
            WiFi.begin(m_ssid, m_pass);
        }
        return;
    }

    if (!m_mqttClient.connected()) 
    {
        reconnectMQTT();
    } 
    else 
    {
        m_mqttClient.loop();
    }
}

bool NetworkManager::subscribe(const char* topic) 
{
    if (m_mqttClient.connected()) 
    {
        return m_mqttClient.subscribe(topic);
    }
    return false;
}

bool NetworkManager::publish(const char* topic, const char* payload) 
{
    if (m_mqttClient.connected()) 
    {
        return m_mqttClient.publish(topic, payload);
    }
    return false;
}

void NetworkManager::onMessage(MessageCallback callback) 
{
    m_userCallback = callback;
}

void NetworkManager::onConnected(ConnectCallback callback) 
{
    m_connectCallback = callback;
}

void NetworkManager::internalMqttCallback(char* topic, byte* payload, unsigned int length) 
{
    String message;
    message.reserve(length);
    for (unsigned int i = 0; i < length; i++) 
    {
        message += (char)payload[i];
    }

    if (m_userCallback) 
    {
        m_userCallback(String(topic), message);
    }
}