#include "DoorRelay.h"
#include "Config.h"
#include "Protocol.h"

DoorRelay::DoorRelay(NetworkManager& net, Logger& log, IngestClient& ingest, CommandRouter& router)
    : m_net(net), m_log(log), m_ingest(ingest), m_router(router),
      m_relay(Cfg::RELAY_PIN, Cfg::RELAY_ACTIVE_LOW) {}

void DoorRelay::begin() {
    m_relay.begin();
    m_relay.turnOff();

    m_router.on("open", [this](JsonObjectConst, String& detail) -> bool {
        if (m_pulseUntil != 0) { detail = "zaten acik"; return false; }
        m_relay.turnOn();
        m_pulseUntil = millis() + Cfg::DOOR_PULSE_MS;
        m_lastOpenTs = m_net.epochNow();
        m_log.info("KAPI: role ON (%lu ms)", Cfg::DOOR_PULSE_MS);

        String ev = String("{\"type\":\"door_open\",\"source\":\"web\",\"ts\":") + m_lastOpenTs + "}";
        m_net.publish(TOPIC_EVENT, ev.c_str(), false);
        m_ingest.post("event", ev, m_lastOpenTs);

        detail = String(Cfg::DOOR_PULSE_MS) + "ms pulse";
        publishState();
        return true;
    });

    m_log.info("KAPI rolu hazir (pin %u, activeLow=%d)", Cfg::RELAY_PIN, Cfg::RELAY_ACTIVE_LOW);
}

void DoorRelay::loop() {
    if (m_pulseUntil != 0 && (int32_t)(millis() - m_pulseUntil) >= 0) {
        m_pulseUntil = 0;
        m_relay.turnOff();
        m_log.info("KAPI: role OFF");
        publishState();
    }
}

void DoorRelay::onConnected() {
    m_net.subscribe(TOPIC_CMD);
    publishState();
}

void DoorRelay::publishState() {
    String s;
    s.reserve(120);
    s  = "{\"online\":true,\"relay\":";
    s += (m_pulseUntil != 0) ? "true" : "false";
    s += ",\"last_open_ts\":"; s += String(m_lastOpenTs);
    s += ",\"fw\":\""; s += FW_VERSION; s += "\"}";
    m_net.publish(TOPIC_STATE, s.c_str(), true);
}
