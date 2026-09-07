#pragma once
// ============================================================================
//  ROLE_SALON — klimaya IR aç/kapa (IRremoteESP8266 / IRac).
//  Klima markası Config.h -> Cfg::AC_PROTOCOL ile seçilir (varsayılan COOLIX,
//  markasız/çoğu split klima ile uyumlu). Marka desteklenmiyorsa Faz sonrası
//  ham (raw) yakala-tekrarla yöntemine geçilir.
// ============================================================================

#include <Arduino.h>
#include <IRac.h>
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
    void applyAc(bool on, String& detail);

    NetworkManager& m_net;
    Logger&         m_log;
    IngestClient&   m_ingest;
    CommandRouter&  m_router;

    IRac*   m_ac = nullptr;
    bool    m_acOn  = false;
    uint8_t m_temp  = 24;
};
