#pragma once
// ============================================================================
//  ROLE_KAMERA (ESP32-CAM) — hareket (PIR) + ışık şimdi; kamera akışı Faz 3.
//  Kamera akışı: cmd:stream_start -> ev/kamera/stream'e binary JPEG kare;
//  180 sn hareketsizlikte otomatik stream_stop. (Faz 3)
// ============================================================================

#include <Arduino.h>
#include "../core/NetworkManager.h"
#include "../core/CommandRouter.h"
#include "../core/Logger.h"
#include "../core/IngestClient.h"

class CamNode {
public:
    CamNode(NetworkManager& net, Logger& log, IngestClient& ingest, CommandRouter& router);
    void begin();
    void loop();
    void onConnected();

private:
    void publishState();
    void setLight(bool on);

    NetworkManager& m_net;
    Logger&         m_log;
    IngestClient&   m_ingest;
    CommandRouter&  m_router;

    bool     m_light = false;
    bool     m_streaming = false;
    String   m_res = "vga";
    uint8_t  m_fps = 0;

    int      m_pirLast = LOW;
    uint32_t m_lastMotionTs = 0;
    uint32_t m_lastMotionMs = 0;
};
