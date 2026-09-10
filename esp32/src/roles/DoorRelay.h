#pragma once
// ============================================================================
//  ROLE_KAPI — kapı-açma rölesi. cmd:open → röle DOOR_PULSE_MS ON, sonra OFF.
//  Zamanlama tamamen firmware'de (ağ kopsa bile güvenli).
// ============================================================================

#include <Arduino.h>
#include "../core/NetworkManager.h"
#include "../core/CommandRouter.h"
#include "../core/Logger.h"
#include "../core/IngestClient.h"
#include "Actuator.h"

class DoorRelay {
public:
    DoorRelay(NetworkManager& net, Logger& log, IngestClient& ingest, CommandRouter& router);
    void begin();
    void loop();
    void onConnected();      // MQTT bağlanınca: subscribe + state

private:
    void publishState();

    NetworkManager& m_net;
    Logger&         m_log;
    IngestClient&   m_ingest;
    CommandRouter&  m_router;
    Actuator        m_relay;

    bool     m_openPending = false;
    uint32_t m_pulseUntil  = 0;
    uint32_t m_lastOpenTs  = 0;
};
