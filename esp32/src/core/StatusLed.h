#pragma once
// ============================================================================
//  StatusLed — tek LED ile durum göstergesi.
//   base 0 = sönük        (WiFi yok)
//   base 1 = yavaş yanıp söner (WiFi var, MQTT bağlanıyor)
//   base 2 = sürekli yanık (MQTT bağlı, hazır)
//   blip() = kısa süre hızlı yanıp söner (komut geldi / aktivite)
// ============================================================================

#include <Arduino.h>

class StatusLed {
public:
    void begin(uint8_t pin, bool activeLow);
    void base(uint8_t mode);          // 0 / 1 / 2
    void blip(uint16_t ms = 2500);    // aktivite flaşı
    void loop();

private:
    void write(bool on);

    uint8_t  m_pin = 255;
    bool     m_activeLow = false;
    uint8_t  m_base = 0;
    uint32_t m_blipUntil = 0;
    uint32_t m_lastToggle = 0;
    bool     m_on = false;
};
