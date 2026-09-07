#include "CamNode.h"
#include "Config.h"
#include "Protocol.h"

CamNode::CamNode(NetworkManager& net, Logger& log, IngestClient& ingest, CommandRouter& router)
    : m_net(net), m_log(log), m_ingest(ingest), m_router(router) {}

void CamNode::begin() {
    pinMode(Cfg::PIR_PIN, INPUT);
    pinMode(Cfg::LIGHT_PIN, OUTPUT);
    setLight(false);

    m_router.on("light_on",  [this](JsonObjectConst, String& d){ setLight(true);  d = "acik";  return true; });
    m_router.on("light_off", [this](JsonObjectConst, String& d){ setLight(false); d = "kapali"; return true; });

    auto notImpl = [this](JsonObjectConst, String& d){ d = "kamera akisi Faz 3"; return false; };
    m_router.on("stream_start", notImpl);
    m_router.on("stream_stop",  notImpl);
    m_router.on("stream_set",   notImpl);
    m_router.on("snapshot",     notImpl);

    m_log.info("KAMERA rolu hazir (PIR %u, isik %u). NOT: kamera akisi Faz 3.",
               Cfg::PIR_PIN, Cfg::LIGHT_PIN);
}

void CamNode::loop() {
    int pir = digitalRead(Cfg::PIR_PIN);
    if (pir == HIGH && m_pirLast == LOW) {
        uint32_t now = millis();
        if (now - m_lastMotionMs >= Cfg::MOTION_DEBOUNCE_MS) {
            m_lastMotionMs = now;
            m_lastMotionTs = m_net.epochNow();
            m_log.info("KAMERA: hareket algilandi");
            String ev = String("{\"type\":\"motion\",\"ts\":") + m_lastMotionTs + "}";
            m_net.publish(TOPIC_EVENT, ev.c_str(), false);
            m_ingest.post("event", ev, m_lastMotionTs);
            publishState();
        }
    }
    m_pirLast = pir;
}

void CamNode::onConnected() {
    m_net.subscribe(TOPIC_CMD);
    publishState();
}

void CamNode::setLight(bool on) {
    m_light = on;
    digitalWrite(Cfg::LIGHT_PIN, (on ^ Cfg::LIGHT_ACTIVE_LOW) ? HIGH : LOW);
}

void CamNode::publishState() {
    String s;
    s.reserve(160);
    s  = "{\"online\":true,\"light\":"; s += (m_light ? "true" : "false");
    s += ",\"streaming\":";             s += (m_streaming ? "true" : "false");
    s += ",\"res\":\"";                 s += m_res; s += "\"";
    s += ",\"fps\":";                   s += String(m_fps);
    s += ",\"last_motion_ts\":";        s += String(m_lastMotionTs);
    s += ",\"fw\":\"";                  s += FW_VERSION; s += "\"}";
    m_net.publish(TOPIC_STATE, s.c_str(), true);
}
