#include "StatusLed.h"

void StatusLed::begin(uint8_t pin, bool activeLow) {
    m_pin = pin;
    m_activeLow = activeLow;
    pinMode(m_pin, OUTPUT);
    write(false);
}

void StatusLed::base(uint8_t mode) { m_base = mode; }

void StatusLed::blip(uint16_t ms) { m_blipUntil = millis() + ms; }

void StatusLed::write(bool on) {
    if (m_pin == 255) return;
    digitalWrite(m_pin, (on ^ m_activeLow) ? HIGH : LOW);
    m_on = on;
}

void StatusLed::loop() {
    if (m_pin == 255) return;
    uint32_t now = millis();

    // Aktivite (blip) — hızlı yanıp sönme, base'i ezer
    if (m_blipUntil != 0 && (int32_t)(now - m_blipUntil) < 0) {
        if (now - m_lastToggle >= 90) { m_lastToggle = now; write(!m_on); }
        return;
    }

    if (m_base == 2)      { write(true); }
    else if (m_base == 1) { if (now - m_lastToggle >= 400) { m_lastToggle = now; write(!m_on); } }
    else                  { write(false); }
}
