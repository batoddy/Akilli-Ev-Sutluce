#pragma once
// ============================================================================
//  ROLE_SALON — klimaya IR aç/kapa. (IR protokolü Faz 3'te IRremoteESP8266 ile.)
// ============================================================================

#include <Arduino.h>
#include "../core/NetworkManager.h"
#include "../core/CommandRouter.h"
#include "../core/Logger.h"
#include "../core/IngestClient.h"

class AcNode {
public:
    AcNode(NetworkManager& net, Logger& log, IngestClient& ingest, CommandRouter& router);
    void begin();
    void loop();
    void onConnected();

private:
    void publishState();
    void setAc(bool on, String& detail);

    NetworkManager& m_net;
    Logger&         m_log;
    IngestClient&   m_ingest;
    CommandRouter&  m_router;
    bool            m_acOn = false;
};
