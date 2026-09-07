#include "CommandRouter.h"
#include "Protocol.h"

CommandRouter::CommandRouter(NetworkManager& net, Logger& log)
    : m_net(net), m_log(log) {}

void CommandRouter::on(const String& cmd, Handler h) {
    m_handlers.emplace_back(cmd, h);
}

void CommandRouter::handle(const String& topic, const String& payload) {
    if (topic != TOPIC_CMD) return;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
        m_log.warn("cmd JSON hatasi: %s", err.c_str());
        return;
    }

    String id  = doc["id"]  | "";
    String cmd = doc["cmd"] | "";
    JsonObjectConst args = doc["args"].as<JsonObjectConst>();

    if (cmd.isEmpty()) { m_log.warn("cmd bos"); return; }
    m_log.info("cmd alindi: %s (id=%s)", cmd.c_str(), id.c_str());

    String detail;
    bool ok = false;

    // --- Ortak komutlar ---
    if (cmd == "reboot") {
        ack(id, cmd, true, "yeniden baslatiliyor");
        delay(200);
        ESP.restart();
        return;
    }
    if (cmd == "set_log_level") {
        LogLevel lvl;
        String s = args["level"] | "";
        if (Logger::parseLevel(s, lvl)) { m_log.setMqttLevel(lvl); ok = true; detail = s; }
        else { detail = "gecersiz seviye"; }
        ack(id, cmd, ok, detail);
        return;
    }

    // --- Rol komutları ---
    for (auto& h : m_handlers) {
        if (h.first == cmd) {
            ok = h.second(args, detail);
            ack(id, cmd, ok, detail);
            return;
        }
    }

    m_log.warn("bilinmeyen komut: %s", cmd.c_str());
    ack(id, cmd, false, "bilinmeyen komut");
}

void CommandRouter::ack(const String& id, const String& cmd, bool ok, const String& detail) {
    uint32_t ts = m_net.epochNow();

    JsonDocument doc;
    doc["id"]     = id;
    doc["cmd"]    = cmd;
    doc["result"] = ok ? "ok" : "error";
    if (detail.isEmpty()) doc["detail"] = nullptr;
    else                  doc["detail"] = detail;
    doc["ts"] = ts;

    String out;
    serializeJson(doc, out);
    m_net.publish(TOPIC_CMD_ACK, out.c_str(), false);

    if (m_ackSink) m_ackSink(id, cmd, ok, detail, ts);
}
