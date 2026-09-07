#pragma once
// ============================================================================
//  Logger — Serial + MQTT (ev/<dev>/log) aynası.
//  - Serial'e her zaman (m_serialLevel'e göre)
//  - MQTT'ye publisher bağlıysa ve seviye >= m_mqttLevel ise
//  - Rate-limit: saniyede en fazla m_ratePerSec MQTT mesajı; taşan damlalar sayılır
//  Bağımlılık yok (ArduinoJson bile değil) — JSON elle + escape.
// ============================================================================

#include <Arduino.h>
#include <functional>
#include <stdarg.h>

enum class LogLevel : uint8_t { DEBUG = 0, INFO = 1, WARN = 2, ERROR = 3 };

class Logger {
public:
    using Publisher = std::function<bool(const char* topic, const char* payload)>;

    void begin(const char* logTopic,
               LogLevel serialLevel = LogLevel::DEBUG,
               LogLevel mqttLevel   = LogLevel::INFO);

    void attachPublisher(Publisher pub) { m_publish = pub; }

    void setMqttLevel(LogLevel lvl)   { m_mqttLevel = lvl; }
    void setSerialLevel(LogLevel lvl) { m_serialLevel = lvl; }
    LogLevel mqttLevel() const        { return m_mqttLevel; }

    static bool        parseLevel(const String& s, LogLevel& out);
    static const char* levelName(LogLevel lvl);

    void log(LogLevel lvl, const char* fmt, ...);
    void debug(const char* fmt, ...);
    void info(const char* fmt, ...);
    void warn(const char* fmt, ...);
    void error(const char* fmt, ...);

    // loop içinde sık çağır — token yenileme + damla raporu
    void tick();

private:
    void vlog(LogLevel lvl, const char* fmt, va_list ap);
    void emit(LogLevel lvl, const char* msg);
    static size_t jsonEscape(const char* in, char* out, size_t outSize);

    const char* m_logTopic     = nullptr;
    LogLevel    m_serialLevel   = LogLevel::DEBUG;
    LogLevel    m_mqttLevel     = LogLevel::INFO;
    Publisher   m_publish       = nullptr;

    uint8_t  m_ratePerSec = 5;
    int16_t  m_tokens     = 5;
    uint32_t m_lastRefill = 0;
    uint16_t m_dropped    = 0;
};
