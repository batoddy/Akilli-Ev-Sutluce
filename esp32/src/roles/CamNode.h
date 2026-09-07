#pragma once
// ============================================================================
//  ROLE_KAMERA (ESP32-CAM AI-Thinker)
//  - PIR hareket -> ev/kamera/event {type:motion}
//  - ışık (GPIO4) aç/kapa
//  - on-demand kamera akışı: cmd:stream_start -> her kare binary JPEG -> ev/kamera/stream
//    cmd:stream_set (canlı res/fps), cmd:stream_stop, cmd:snapshot
//  - Akış, stream_start/stream_set'ten 180 sn sonra otomatik durur.
//    Arayüz akışı sürdürmek için ~60 sn'de bir stream_start tekrar yollar (keepalive).
//  Not: PubSubClient buffer 16-bit -> pratikte SVGA'ya kadar. XGA istenirse SVGA'ya kırpılır.
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
    bool ensureCamera();                 // lazy init, true = hazır
    void applyResolution(const String& res);
    void startStream(const String& res, uint8_t fps, String& detail);
    void stopStream(const char* reason);
    void captureAndPublish();
    void publishState();
    void setLight(bool on);
    void checkMotion();

    NetworkManager& m_net;
    Logger&         m_log;
    IngestClient&   m_ingest;
    CommandRouter&  m_router;

    bool     m_camReady   = false;
    bool     m_light      = false;
    bool     m_streaming  = false;
    String   m_res        = "vga";
    uint8_t  m_fps        = 5;
    uint32_t m_frameGap   = 200;          // ms (1000/fps)
    uint32_t m_lastFrame  = 0;
    uint32_t m_streamDeadline = 0;

    int      m_pirLast       = LOW;
    uint32_t m_lastMotionTs  = 0;
    uint32_t m_lastMotionMs  = 0;
};
