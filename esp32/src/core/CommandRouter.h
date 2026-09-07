#pragma once
// ============================================================================
//  CommandRouter — ev/<dev>/cmd JSON'unu parse eder, ilgili handler'a yollar,
//  ev/<dev>/cmd/ack yayınlar. Ortak komutlar: reboot, set_log_level, get_state.
//  Gelen: {"id":"c-..","cmd":"open","args":{...}}
//  ACK:   {"id":"c-..","cmd":"open","result":"ok"|"error","detail":<str|null>,"ts":<epoch>}
// ============================================================================

#include <Arduino.h>
#include <ArduinoJson.h>
#include <functional>
#include <vector>
#include <utility>
#include "NetworkManager.h"
#include "Logger.h"

class CommandRouter {
public:
    using Handler  = std::function<bool(JsonObjectConst args, String& detail)>;
    using AckSink  = std::function<void(const String& id, const String& cmd, bool ok, const String& detail, uint32_t ts)>;

    CommandRouter(NetworkManager& net, Logger& log);

    void on(const String& cmd, Handler h);      // rol komutları
    void setAckSink(AckSink sink) { m_ackSink = sink; }   // ack'i ayrıca /api/ingest'e yollamak için

    // NetworkManager.onMessage'dan çağrılır
    void handle(const String& topic, const String& payload);

private:
    void ack(const String& id, const String& cmd, bool ok, const String& detail);

    NetworkManager& m_net;
    Logger&         m_log;
    std::vector<std::pair<String, Handler>> m_handlers;
    AckSink         m_ackSink = nullptr;
};
