// ============================================================================
//  Sütlüce Akıllı Ev — ortak firmware iskeleti (3 rol build flag ile seçilir)
//  MIMARI.md §9. Rol kodları: src/roles/
// ============================================================================
#include <Arduino.h>
#include "Config.h"
#include "Protocol.h"
#include "core/Logger.h"
#include "core/NetworkManager.h"
#include "core/CommandRouter.h"
#include "core/Telemetry.h"
#include "core/IngestClient.h"
#include "core/StatusLed.h"

#if defined(ROLE_KAPI)
  #include "roles/DoorRelay.h"
#elif defined(ROLE_KAMERA)
  #include "roles/CamNode.h"
#elif defined(ROLE_SALON)
  #include "roles/AcNode.h"
#endif

Logger         logger;
StatusLed      led;
NetworkManager net(logger, Cfg::WIFI_SSID, Cfg::WIFI_PASS,
                   Cfg::MQTT_HOST, Cfg::MQTT_PORT, Cfg::MQTT_USER, Cfg::MQTT_PASS,
                   DEVICE_ID);
IngestClient   ingest(logger, Cfg::INGEST_URL, Cfg::INGEST_TOKEN, DEVICE_ID);
CommandRouter  router(net, logger);
Telemetry      telemetry(net, logger, ingest);

#if defined(ROLE_KAPI)
  DoorRelay role(net, logger, ingest, router);
#elif defined(ROLE_KAMERA)
  CamNode   role(net, logger, ingest, router);
#elif defined(ROLE_SALON)
  AcNode    role(net, logger, ingest, router);
#endif

void setup() {
    Serial.begin(115200);
    delay(300);

    logger.begin(TOPIC_LOG, LogLevel::DEBUG, LogLevel::INFO);
    logger.info("=== %s [%s] fw %s ===", DEVICE_NAME, DEVICE_ID, FW_VERSION);

    led.begin(Cfg::STATUS_LED_PIN, Cfg::STATUS_LED_ACTIVE_LOW);

    net.onMessage([](const String& t, const String& p) { router.handle(t, p); });

    net.onConnected([]() {
        role.onConnected();
    });

    // ACK'i MQTT'nin yanı sıra /api/ingest'e de yaz (kalıcı kayıt) + LED flaşı
    router.setAckSink([](const String& id, const String& cmd, bool ok,
                         const String& detail, uint32_t ts) {
        led.blip(2500);   // komut geldi -> LED hızlı yanıp söner
        String d = "{\"id\":\"";      d += id;
        d += "\",\"cmd\":\"";         d += cmd;
        d += "\",\"result\":\"";      d += (ok ? "ok" : "error");
        d += "\",\"detail\":";        d += (detail.length() ? ("\"" + detail + "\"") : String("null"));
        d += "}";
        ingest.post("command_ack", d, ts);
    });

    net.begin();
    logger.attachPublisher([](const char* topic, const char* payload) {
        return net.publish(topic, payload, false);
    });

    role.begin();
    logger.info("setup tamam, loop basliyor");
}

void loop() {
    net.loop();
    role.loop();
    telemetry.loop();
    logger.tick();

    // LED: WiFi yok=sönük, MQTT bağlanıyor=yavaş blink, bağlı=sürekli yanık
    led.base(!net.wifiUp() ? 0 : !net.connected() ? 1 : 2);
    led.loop();
}
