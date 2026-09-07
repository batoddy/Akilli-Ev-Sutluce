#pragma once

#include <Arduino.h>
#include "secrets.h"   // gitignore — bkz. secrets.example.h

namespace Config {
    // --- Sırlar (secrets.h'den gelir) ---
    constexpr const char* WIFI_SSID   = SECRET_WIFI_SSID;
    constexpr const char* WIFI_PASS   = SECRET_WIFI_PASS;

    constexpr const char* MQTT_SERVER = SECRET_MQTT_HOST;
    constexpr int         MQTT_PORT   = SECRET_MQTT_PORT;   // 8883 (TLS)
    constexpr const char* MQTT_USER   = SECRET_MQTT_USER;
    constexpr const char* MQTT_PASS   = SECRET_MQTT_PASS;

    // --- Donanım & Topic tanımları (sır değil) ---
    constexpr uint8_t     SALON_LAMBA_PIN   = 2; // Yerleşik LED veya röle pini
    constexpr const char* TOPIC_SALON_LAMBA = "ev/salon/lamba";
}
