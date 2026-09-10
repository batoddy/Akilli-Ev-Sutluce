#include "NetworkManager.h"
#include "Protocol.h"
#include <time.h>

NetworkManager::NetworkManager(Logger& logger,
                               const char* ssid, const char* pass,
                               const char* host, uint16_t port,
                               const char* user, const char* mqttPass,
                               const char* clientPrefix)
    : m_log(logger),
      m_ssid(ssid), m_pass(pass),
      m_host(host), m_port(port),
      m_user(user), m_mqttPass(mqttPass),
      m_clientPrefix(clientPrefix),
      m_mqtt(m_tls) {}

void NetworkManager::begin() {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.persistent(false);
    WiFi.begin(m_ssid, m_pass);
    m_lastWifiTry = millis();
    m_log.info("WiFi baglaniliyor: %s", m_ssid);

    m_tls.setInsecure();                       // sertifika doğrulaması yok (hobi)
    m_mqtt.setServer(m_host, m_port);
    m_mqtt.setKeepAlive(20);                   // broker ~30 sn'de offline algılar (LWT)
    m_mqtt.setSocketTimeout(10);
    m_mqtt.setBufferSize(1024);
    m_mqtt.setCallback([this](char* t, byte* p, unsigned int l) { onRawMessage(t, p, l); });
}

void NetworkManager::setBufferSize(uint16_t bytes) {
    m_mqtt.setBufferSize(bytes);
}

void NetworkManager::loop() {
    ensureWifi();
    if (wifiUp()) {
        if (!m_mqtt.connected()) ensureMqtt();
        else                     m_mqtt.loop();
    }

    bool up = m_mqtt.connected();
    if (up != m_wasConnected) {
        m_wasConnected = up;
        if (!up) m_log.warn("MQTT baglantisi koptu");
    }
}

void NetworkManager::ensureWifi() {
    if (wifiUp()) return;
    uint32_t now = millis();
    if (now - m_lastWifiTry < WIFI_RETRY_INTERVAL) return;
    m_lastWifiTry = now;
    m_log.warn("WiFi yok, yeniden deneniyor...");
    WiFi.disconnect();
    WiFi.begin(m_ssid, m_pass);
}

void NetworkManager::ensureMqtt() {
    uint32_t now = millis();
    if (m_mqttBackoff == 0) m_mqttBackoff = MQTT_RETRY_BASE;
    if (now - m_lastMqttTry < m_mqttBackoff) return;
    m_lastMqttTry = now;

    if (!m_ntpStarted) {
        configTime(0, 0, "pool.ntp.org", "time.nist.gov");
        m_ntpStarted = true;
    }

    String clientId = String(m_clientPrefix) + "-" + String((uint32_t)ESP.getEfuseMac(), HEX);
    m_log.info("MQTT baglaniliyor (%s)...", m_host);

    bool ok = m_mqtt.connect(
        clientId.c_str(), m_user, m_mqttPass,
        TOPIC_STATUS, 1, true, STATUS_OFFLINE);   // LWT: retained offline

    if (ok) {
        m_mqttBackoff = MQTT_RETRY_BASE;
        m_log.info("MQTT bagli. status=online yayinlaniyor");
        m_mqtt.publish(TOPIC_STATUS, STATUS_ONLINE, true);
        if (m_onConnected) m_onConnected();
    } else {
        m_mqttBackoff *= 2;
        if (m_mqttBackoff > MQTT_RETRY_MAX) m_mqttBackoff = MQTT_RETRY_MAX;
        m_log.warn("MQTT basarisiz rc=%d, %lus sonra tekrar", m_mqtt.state(), m_mqttBackoff / 1000);
    }
}

bool NetworkManager::connected() { return m_mqtt.connected(); }

bool NetworkManager::publish(const char* topic, const char* payload, bool retained) {
    if (!m_mqtt.connected()) return false;
    return m_mqtt.publish(topic, payload, retained);
}

bool NetworkManager::publishRaw(const char* topic, const uint8_t* payload, unsigned int len, bool retained) {
    if (!m_mqtt.connected()) return false;
    return m_mqtt.publish(topic, payload, len, retained);
}

bool NetworkManager::subscribe(const char* topic, uint8_t qos) {
    if (!m_mqtt.connected()) return false;
    return m_mqtt.subscribe(topic, qos);
}

uint32_t NetworkManager::epochNow() {
    time_t t = time(nullptr);
    return (t > 1600000000) ? (uint32_t)t : 0;
}

void NetworkManager::onRawMessage(char* topic, byte* payload, unsigned int length) {
    String msg;
    msg.reserve(length);
    for (unsigned int i = 0; i < length; ++i) msg += (char)payload[i];
    if (m_onMessage) m_onMessage(String(topic), msg);
}
