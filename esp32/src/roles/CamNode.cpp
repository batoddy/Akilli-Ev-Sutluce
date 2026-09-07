#include "CamNode.h"
#include "Config.h"
#include "Protocol.h"
#include <esp_camera.h>

// --- AI-Thinker ESP32-CAM pin haritası (sabit) ---
#define CAM_PWDN   32
#define CAM_RESET  -1
#define CAM_XCLK    0
#define CAM_SIOD   26
#define CAM_SIOC   27
#define CAM_Y9     35
#define CAM_Y8     34
#define CAM_Y7     39
#define CAM_Y6     36
#define CAM_Y5     21
#define CAM_Y4     19
#define CAM_Y3     18
#define CAM_Y2      5
#define CAM_VSYNC  25
#define CAM_HREF   23
#define CAM_PCLK   22

static framesize_t resToFramesize(const String& r, String& applied) {
    if (r == "qvga") { applied = "qvga"; return FRAMESIZE_QVGA; } // 320x240
    if (r == "hvga") { applied = "hvga"; return FRAMESIZE_HVGA; } // 480x320
    if (r == "vga")  { applied = "vga";  return FRAMESIZE_VGA;  } // 640x480
    if (r == "svga") { applied = "svga"; return FRAMESIZE_SVGA; } // 800x600
    if (r == "xga")  { applied = "svga"; return FRAMESIZE_SVGA; } // MQTT sınırı -> kırp
    applied = "vga"; return FRAMESIZE_VGA;
}

CamNode::CamNode(NetworkManager& net, Logger& log, IngestClient& ingest, CommandRouter& router)
    : m_net(net), m_log(log), m_ingest(ingest), m_router(router) {}

void CamNode::begin() {
    pinMode(Cfg::PIR_PIN, INPUT);
    pinMode(Cfg::LIGHT_PIN, OUTPUT);
    setLight(false);

    // Kamera kareleri için MQTT buffer'ı büyüt (SVGA ~45KB'a kadar).
    m_net.setBufferSize(50000);

    m_router.on("light_on",  [this](JsonObjectConst, String& d){ setLight(true);  d = "acik";  publishState(); return true; });
    m_router.on("light_off", [this](JsonObjectConst, String& d){ setLight(false); d = "kapali"; publishState(); return true; });

    m_router.on("stream_start", [this](JsonObjectConst a, String& d) -> bool {
        String res = a["res"] | m_res.c_str();
        uint8_t fps = a["fps"] | m_fps;
        startStream(res, fps, d);
        return m_streaming;
    });
    m_router.on("stream_set", [this](JsonObjectConst a, String& d) -> bool {
        if (!m_streaming) { d = "akis kapali"; return false; }
        String res = a["res"] | m_res.c_str();
        uint8_t fps = a["fps"] | m_fps;
        startStream(res, fps, d);          // deadline'ı da yeniler (keepalive)
        return true;
    });
    m_router.on("stream_stop", [this](JsonObjectConst, String& d){ stopStream("cmd"); d = "durdu"; return true; });
    m_router.on("snapshot",    [this](JsonObjectConst, String& d) -> bool {
        if (!ensureCamera()) { d = "kamera init hatasi"; return false; }
        captureAndPublish();
        d = "tek kare";
        return true;
    });

    m_log.info("KAMERA rolu hazir (PIR %u, isik %u).", Cfg::PIR_PIN, Cfg::LIGHT_PIN);
}

bool CamNode::ensureCamera() {
    if (m_camReady) return true;

    camera_config_t c = {};
    c.ledc_channel = LEDC_CHANNEL_0;
    c.ledc_timer   = LEDC_TIMER_0;
    c.pin_d0 = CAM_Y2;  c.pin_d1 = CAM_Y3;  c.pin_d2 = CAM_Y4;  c.pin_d3 = CAM_Y5;
    c.pin_d4 = CAM_Y6;  c.pin_d5 = CAM_Y7;  c.pin_d6 = CAM_Y8;  c.pin_d7 = CAM_Y9;
    c.pin_xclk = CAM_XCLK; c.pin_pclk = CAM_PCLK;
    c.pin_vsync = CAM_VSYNC; c.pin_href = CAM_HREF;
    c.pin_sccb_sda = CAM_SIOD; c.pin_sccb_scl = CAM_SIOC;
    c.pin_pwdn = CAM_PWDN; c.pin_reset = CAM_RESET;
    c.xclk_freq_hz = 20000000;
    c.pixel_format = PIXFORMAT_JPEG;
    c.frame_size   = FRAMESIZE_VGA;
    c.jpeg_quality = 12;
    c.fb_count     = psramFound() ? 2 : 1;
    c.fb_location  = psramFound() ? CAMERA_FB_IN_PSRAM : CAMERA_FB_IN_DRAM;
    c.grab_mode    = CAMERA_GRAB_LATEST;

    esp_err_t err = esp_camera_init(&c);
    if (err != ESP_OK) {
        m_log.error("esp_camera_init hata 0x%x", err);
        return false;
    }
    m_camReady = true;
    m_log.info("kamera init OK (psram=%d)", psramFound());
    applyResolution(m_res);
    return true;
}

void CamNode::applyResolution(const String& res) {
    String applied;
    framesize_t fs = resToFramesize(res, applied);
    sensor_t* s = esp_camera_sensor_get();
    if (s) s->set_framesize(s, fs);
    m_res = applied;
}

void CamNode::startStream(const String& res, uint8_t fps, String& detail) {
    if (!ensureCamera()) { detail = "kamera init hatasi"; return; }

    if (fps < 1) fps = 1;
    if (fps > 10) fps = 10;
    m_fps = fps;
    m_frameGap = 1000UL / m_fps;
    applyResolution(res);

    bool wasStreaming = m_streaming;
    m_streaming = true;
    m_streamDeadline = millis() + Cfg::STREAM_IDLE_TIMEOUT_MS;
    detail = m_res + "@" + String(m_fps) + "fps";

    if (!wasStreaming) {
        m_log.info("akis basladi %s", detail.c_str());
        uint32_t ts = m_net.epochNow();
        String ev = String("{\"type\":\"stream\",\"action\":\"start\",\"res\":\"") + m_res +
                    "\",\"fps\":" + m_fps + ",\"ts\":" + ts + "}";
        m_net.publish(TOPIC_EVENT, ev.c_str(), false);
        m_ingest.post("event", ev, ts);
    }
    publishState();
}

void CamNode::stopStream(const char* reason) {
    if (!m_streaming) return;
    m_streaming = false;
    m_fps = 0;
    m_log.info("akis durdu (%s)", reason);

    uint32_t ts = m_net.epochNow();
    String ev = String("{\"type\":\"stream\",\"action\":\"stop\",\"reason\":\"") + reason + "\",\"ts\":" + ts + "}";
    m_net.publish(TOPIC_EVENT, ev.c_str(), false);
    m_ingest.post("event", ev, ts);
    publishState();
}

void CamNode::captureAndPublish() {
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) { m_log.warn("fb_get NULL"); return; }
    if (fb->format == PIXFORMAT_JPEG && fb->len > 0) {
        m_net.publishRaw(TOPIC_STREAM, fb->buf, fb->len, false);
    }
    esp_camera_fb_return(fb);
}

void CamNode::loop() {
    checkMotion();

    if (m_streaming) {
        if ((int32_t)(millis() - m_streamDeadline) >= 0) {
            stopStream("timeout");
            return;
        }
        uint32_t now = millis();
        if (now - m_lastFrame >= m_frameGap) {
            m_lastFrame = now;
            if (m_net.connected()) captureAndPublish();
        }
    }
}

void CamNode::checkMotion() {
    int pir = digitalRead(Cfg::PIR_PIN);
    if (pir == HIGH && m_pirLast == LOW) {
        uint32_t now = millis();
        if (now - m_lastMotionMs >= Cfg::MOTION_DEBOUNCE_MS) {
            m_lastMotionMs = now;
            m_lastMotionTs = m_net.epochNow();
            m_log.info("hareket algilandi");
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
    s.reserve(180);
    s  = "{\"online\":true,\"light\":";  s += (m_light ? "true" : "false");
    s += ",\"streaming\":";              s += (m_streaming ? "true" : "false");
    s += ",\"res\":\"";                  s += m_res; s += "\"";
    s += ",\"fps\":";                    s += String(m_fps);
    s += ",\"last_motion_ts\":";         s += String(m_lastMotionTs);
    s += ",\"fw\":\"";                   s += FW_VERSION; s += "\"}";
    m_net.publish(TOPIC_STATE, s.c_str(), true);
}
