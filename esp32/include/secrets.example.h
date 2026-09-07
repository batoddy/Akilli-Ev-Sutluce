#pragma once
// ============================================================================
//  ŞABLON — bu dosyayı `secrets.h` olarak kopyala ve gerçek değerleri gir.
//  `secrets.h` git'e girmez (.gitignore). Bu şablon commit edilir.
// ============================================================================

// --- WiFi ---
#define SECRET_WIFI_SSID   "WIFI_ADI"
#define SECRET_WIFI_PASS   "WIFI_SIFRESI"

// --- HiveMQ (bu ESP'ye ait kullanıcı: esp-kapi / esp-kamera / esp-salon) ---
#define SECRET_MQTT_HOST   "xxxxxxxx.s1.eu.hivemq.cloud"
#define SECRET_MQTT_PORT   8883
#define SECRET_MQTT_USER   "esp-xxx"
#define SECRET_MQTT_PASS   "GUCLU_SIFRE"

// --- Vercel /api/ingest (kalıcı veri yazımı) ---
#define SECRET_INGEST_URL     "https://<proje>.vercel.app/api/ingest"
#define SECRET_INGEST_TOKEN   "INGEST_SECRET_ile_ayni_deger"
