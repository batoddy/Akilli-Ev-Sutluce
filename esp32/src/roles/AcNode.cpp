#include "AcNode.h"
#include "Config.h"
#include "Protocol.h"

AcNode::AcNode(NetworkManager& net, Logger& log, IngestClient& ingest, CommandRouter& router)
    : m_net(net), m_log(log), m_ingest(ingest), m_router(router) {}

void AcNode::begin() {
    m_ac = new IRac(Cfg::IR_PIN);
    m_temp = Cfg::AC_DEFAULT_TEMP;

    m_router.on("ac_on",  [this](JsonObjectConst a, String& d) -> bool {
        if (a["temp"].is<int>()) {
            int t = a["temp"] | (int)m_temp;
            if (t >= 16 && t <= 30) m_temp = (uint8_t)t;
        }
        applyAc(true, d);
        return true;
    });
    m_router.on("ac_off", [this](JsonObjectConst, String& d) -> bool { applyAc(false, d); return true; });

    m_log.info("SALON rolu hazir (IR pin %u, protokol=%s)",
               Cfg::IR_PIN, typeToString(Cfg::AC_PROTOCOL).c_str());
}

void AcNode::loop() {
    // Faz sonrası: opsiyonel DHT -> state.temp / state.hum
}

void AcNode::onConnected() {
    m_net.subscribe(TOPIC_CMD);
    publishState();
}

void AcNode::applyAc(bool on, String& detail) {
    m_acOn = on;

    stdAc::state_t s;
    s.protocol  = Cfg::AC_PROTOCOL;
    s.model     = 1;
    s.power     = on;
    s.mode      = stdAc::opmode_t::kCool;
    s.degrees   = m_temp;
    s.celsius   = true;
    s.fanspeed  = stdAc::fanspeed_t::kAuto;
    s.swingv    = stdAc::swingv_t::kOff;
    s.swingh    = stdAc::swingh_t::kOff;
    s.light     = true;
    s.beep      = false;
    s.econo     = false;
    s.filter    = false;
    s.turbo     = false;
    s.quiet     = false;
    s.clean     = false;
    s.sleep     = -1;
    s.clock     = -1;

    bool sent = m_ac->sendAc(s, nullptr);
    detail = on ? (String("ac_on ") + m_temp + "C") : "ac_off";
    if (!sent) { detail = "IR protokol desteklenmiyor"; m_log.warn("IRac.sendAc basarisiz"); }
    else       { m_log.info("IR gonderildi: %s", detail.c_str()); }

    uint32_t ts = m_net.epochNow();
    String ev = String("{\"type\":\"ac\",\"on\":") + (on ? "true" : "false") +
                ",\"temp\":" + m_temp + ",\"ts\":" + ts + "}";
    m_net.publish(TOPIC_EVENT, ev.c_str(), false);
    m_ingest.post("event", ev, ts);
    publishState();
}

void AcNode::publishState() {
    String s = String("{\"online\":true,\"ac\":") + (m_acOn ? "true" : "false") +
               ",\"temp\":" + m_temp + ",\"fw\":\"" + FW_VERSION + "\"}";
    m_net.publish(TOPIC_STATE, s.c_str(), true);
}
