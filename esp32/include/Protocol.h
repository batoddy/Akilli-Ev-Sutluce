#pragma once
// ============================================================================
//  MQTT sözleşmesi — MIMARI.md §5 ile birebir. Rol build flag'i ile seçilir.
// ============================================================================

#if defined(ROLE_KAPI)
  #define DEVICE_ID   "kapi"
  #define DEVICE_NAME "Dis Kapi (role)"
#elif defined(ROLE_KAMERA)
  #define DEVICE_ID   "kamera"
  #define DEVICE_NAME "Dis Kamera + Hareket"
#elif defined(ROLE_SALON)
  #define DEVICE_ID   "salon"
  #define DEVICE_NAME "Salon Klima (IR)"
#else
  #error "Rol tanimli degil. platformio.ini env kullan: -DROLE_KAPI | -DROLE_KAMERA | -DROLE_SALON"
#endif

#define FW_VERSION "0.1.0"

// --- Topic'ler (derleme zamanında string birleştirme, sıfır runtime maliyet) ---
#define TOPIC_CMD        "ev/" DEVICE_ID "/cmd"
#define TOPIC_CMD_ACK    "ev/" DEVICE_ID "/cmd/ack"
#define TOPIC_STATE      "ev/" DEVICE_ID "/state"
#define TOPIC_EVENT      "ev/" DEVICE_ID "/event"
#define TOPIC_LOG        "ev/" DEVICE_ID "/log"
#define TOPIC_TELEMETRY  "ev/" DEVICE_ID "/telemetry"
#define TOPIC_STATUS     "ev/" DEVICE_ID "/status"
#define TOPIC_STREAM     "ev/" DEVICE_ID "/stream"   // yalnizca kamera

// --- LWT / status payload ---
#define STATUS_ONLINE   "online"
#define STATUS_OFFLINE  "offline"

// --- Zamanlamalar (ms) ---
#define TELEMETRY_MQTT_INTERVAL   60000UL     // canlı telemetri -> MQTT
#define TELEMETRY_INGEST_INTERVAL 300000UL    // gecmis telemetri -> /api/ingest
#define HEARTBEAT_LOG_INTERVAL    86400000UL  // gunde 1 INFO heartbeat -> logs
#define WIFI_RETRY_INTERVAL       5000UL
#define MQTT_RETRY_BASE           2000UL      // exponential backoff tabani
#define MQTT_RETRY_MAX            60000UL
