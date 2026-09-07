#pragma once

#include <Arduino.h>

class Actuator {
public:
    explicit Actuator(uint8_t pin, bool activeLow = false);
    
    void begin();
    void turnOn();
    void turnOff();
    void toggle();
    void setState(bool state);
    bool getState() const;

private:
    uint8_t m_pin;
    bool m_activeLow;
    bool m_currentState;

    void applyHardwareState();
};