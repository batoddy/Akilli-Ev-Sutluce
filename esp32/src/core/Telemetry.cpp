#include "Telemetry.h"
#include "Protocol.h"

Telemetry::Telemetry(NetworkManager& net, Logger& log, IngestClient& ingest)
    : m_net(net), m_log(log), m_ingest(ingest) {}

String Telemetry::snapshot() {
    String s;
    s.reserve(96);
    s  = "{\"rssi\":";   s += String(m_net.rssi());
    s += ",\"heap\":";   s += String((uint32_t)ESP.getFreeHeap());
    s += ",\"uptime\":"; s += String(millis() / 1000UL);
    s += ",\"ip\":\"";   s += m_net.ip();
    s += "\"}";
    return s;
}

void Telemetry::loop() {
    if (!m_net.connected()) return;
    uint32_t now = millis();

    if (m_lastLive == 0 || now - m_lastLive >= TELEMETRY_MQTT_INTERVAL) {
        m_lastLive = now;
        m_net.publish(TOPIC_TELEMETRY, snapshot().c_str(), true);   // retained
    }

    if (m_lastHist == 0 || now - m_lastHist >= TELEMETRY_INGEST_INTERVAL) {
        m_lastHist = now;
        m_ingest.post("telemetry", snapshot(), m_net.epochNow());
    }

    if (!m_firstBeatDone || now - m_lastBeat >= HEARTBEAT_LOG_INTERVAL) {
        m_firstBeatDone = true;
        m_lastBeat = now;
        m_log.info("heartbeat: fw=%s up=%lus heap=%u rssi=%d",
                   FW_VERSION, millis() / 1000UL, (uint32_t)ESP.getFreeHeap(), m_net.rssi());
    }
}
