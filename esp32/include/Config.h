#pragma once
// ============================================================================
//  Rol'e göre donanım pinleri + sabitler. Sırlar secrets.h'de, topic'ler Protocol.h'de.
// ============================================================================

#include <Arduino.h>
#include "secrets.h"
#include "Protocol.h"

#if defined(ROLE_SALON)
  #include <IRremoteESP8266.h>   // decode_type_t — namespace DIŞINDA olmalı
#endif

namespace Cfg {
    // --- WiFi / MQTT (secrets.h) ---
    constexpr const char* WIFI_SSID = SECRET_WIFI_SSID;
    constexpr const char* WIFI_PASS = SECRET_WIFI_PASS;
    constexpr const char* MQTT_HOST = SECRET_MQTT_HOST;
    constexpr uint16_t    MQTT_PORT = SECRET_MQTT_PORT;
    constexpr const char* MQTT_USER = SECRET_MQTT_USER;
    constexpr const char* MQTT_PASS = SECRET_MQTT_PASS;

    // --- /api/ingest ---
    constexpr const char* INGEST_URL   = SECRET_INGEST_URL;
    constexpr const char* INGEST_TOKEN = SECRET_INGEST_TOKEN;

#if defined(ROLE_KAPI)
    // Kapı-açma rölesi. Aktif-low röle modülleri yaygın -> ACTIVE_LOW=true dene.
    constexpr uint8_t  RELAY_PIN        = 26;
    constexpr bool     RELAY_ACTIVE_LOW = true;
    constexpr uint32_t DOOR_PULSE_MS    = 2000;

#elif defined(ROLE_KAMERA)
    // ESP32-CAM (AI-Thinker). Kamera+PSRAM pinleri sabit; bunlar boşta kalanlar.
    constexpr uint8_t  PIR_PIN          = 13;   // dijital giriş
    constexpr uint8_t  LIGHT_PIN        = 4;    // kart üstü flash LED / harici röle
    constexpr bool     LIGHT_ACTIVE_LOW = false;
    constexpr uint32_t STREAM_IDLE_TIMEOUT_MS = 180000;  // hareketsizlikte dur
    constexpr uint32_t MOTION_DEBOUNCE_MS     = 8000;    // arka arkaya motion event bastırma

#elif defined(ROLE_SALON)
    // IR verici LED (transistör üzerinden). DHT opsiyonel.
    constexpr uint8_t IR_PIN   = 4;
    constexpr uint8_t DHT_PIN  = 15;   // opsiyonel; 0 = yok
    constexpr bool    HAS_DHT  = false;

    // Klima IR protokolü — kendi klimana göre değiştir (COOLIX çoğu markasız
    // split klima ile çalışır). Liste: IRremoteESP8266 decode_type_t.
    constexpr decode_type_t AC_PROTOCOL     = decode_type_t::COOLIX;
    constexpr uint8_t       AC_DEFAULT_TEMP = 24;
#endif
}
