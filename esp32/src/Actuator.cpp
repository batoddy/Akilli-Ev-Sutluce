#include "Actuator.h"

Actuator::Actuator(uint8_t pin, bool activeLow)
    : m_pin(pin), m_activeLow(activeLow), m_currentState(false) {}

void Actuator::begin() {
    pinMode(m_pin, OUTPUT);
    applyHardwareState();
}

void Actuator::turnOn() {
    setState(true);
}

void Actuator::turnOff() {
    setState(false);
}

void Actuator::toggle() {
    setState(!m_currentState);
}

void Actuator::setState(bool state) {
    m_currentState = state;
    applyHardwareState();
}

bool Actuator::getState() const {
    return m_currentState;
}

void Actuator::applyHardwareState() {
    // Aktif low röleler için ters lojik koruması
    bool rawPinLevel = m_activeLow ? !m_currentState : m_currentState;
    digitalWrite(m_pin, rawPinLevel ? HIGH : LOW);
}