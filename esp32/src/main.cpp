#include <Arduino.h>
#include "Config.h"
#include "Actuator.h"
#include "NetworkManager.h"

Actuator salonLamba(Config::SALON_LAMBA_PIN);

NetworkManager network(
    Config::WIFI_SSID,
    Config::WIFI_PASS,
    Config::MQTT_SERVER,
    Config::MQTT_PORT,
    Config::MQTT_USER,
    Config::MQTT_PASS
);

void setup() {
    Serial.begin(115200);
    delay(1000);

    salonLamba.begin();

    // 1. MQTT'ye her bağlanıldığında (veya kopup geri geldiğinde) neye abone olunacak?
    network.onConnected([]() {
        Serial.println("[Network] Baglanti saglandi, kanallara abone olunuyor...");
        
        if (network.subscribe(Config::TOPIC_SALON_LAMBA)) {
            Serial.printf("[Network] BASARILI: '%s' kanalina abone olundu ve dinleniyor.\n", Config::TOPIC_SALON_LAMBA);
        } else {
            Serial.println("[Network] HATA: Kanala abone olunamadi!");
        }
    });

    // 2. Gelen mesajları yönlendir (Router)
    network.onMessage([](const String& topic, const String& payload) {
        Serial.printf("[Event] Topic: %s | Payload: %s\n", topic.c_str(), payload.c_str());

        if (topic == Config::TOPIC_SALON_LAMBA) {
            if (payload == "1") {
                salonLamba.turnOn();
            } else if (payload == "0") {
                salonLamba.turnOff();
            }
        }
    });

    network.begin();
}

void loop() {
    network.update();
}