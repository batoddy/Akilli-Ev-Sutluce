#include "AcNode.h"
#include "Config.h"
#include "Protocol.h"

AcNode::AcNode(NetworkManager& net, Logger& log, IngestClient& ingest, CommandRouter& router)
    : m_net(net), m_log(log), m_ingest(ingest), m_router(router) {}

void AcNode::begin() {
    pinMode(Cfg::IR_PIN, OUTPUT);
    digitalWrite(Cfg::IR_PIN, LOW);

    m_router.on("ac_on",  [this](JsonObjectConst, String& d) -> bool { setAc(true,  d); return true; });
    m_router.on("ac_off", [this](JsonObjectConst, String& d) -> bool { setAc(false, d); return true; });

    m_log.info("SALON rolu hazir (IR pin %u). NOT: IR gonderimi Faz 3.", Cfg::IR_PIN);
}

void AcNode::loop() {
    // Faz 3: opsiyonel DHT okuma -> state.temp / state.hum
}

void AcNode::onConnected() {
    m_net.subscribe(TOPIC_CMD);
    publishState();
}

void AcNode::setAc(bool on, String& detail) {
    m_acOn = on;
    // TODO Faz 3: IRsend ile gerçek klima protokolü
    m_log.info("SALON: klima %s (IR stub)", on ? "ON" : "OFF");
    detail = on ? "ac_on (stub)" : "ac_off (stub)";

    uint32_t ts = m_net.epochNow();
    String ev = String("{\"type\":\"ac\",\"on\":") + (on ? "true" : "false") + ",\"ts\":" + ts + "}";
    m_net.publish(TOPIC_EVENT, ev.c_str(), false);
    m_ingest.post("event", ev, ts);
    publishState();
}

void AcNode::publishState() {
    String s = String("{\"online\":true,\"ac\":") + (m_acOn ? "true" : "false") +
               ",\"fw\":\"" + FW_VERSION + "\"}";
    m_net.publish(TOPIC_STATE, s.c_str(), true);
}
