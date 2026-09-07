#include "Logger.h"

static const char* kLevelNames[] = { "DEBUG", "INFO", "WARN", "ERROR" };

void Logger::begin(const char* logTopic, LogLevel serialLevel, LogLevel mqttLevel) {
    m_logTopic    = logTopic;
    m_serialLevel = serialLevel;
    m_mqttLevel   = mqttLevel;
    m_tokens      = m_ratePerSec;
    m_lastRefill  = millis();
}

const char* Logger::levelName(LogLevel lvl) {
    return kLevelNames[(uint8_t)lvl];
}

bool Logger::parseLevel(const String& s, LogLevel& out) {
    String u = s; u.toUpperCase();
    if (u == "DEBUG") { out = LogLevel::DEBUG; return true; }
    if (u == "INFO")  { out = LogLevel::INFO;  return true; }
    if (u == "WARN")  { out = LogLevel::WARN;  return true; }
    if (u == "ERROR") { out = LogLevel::ERROR; return true; }
    return false;
}

void Logger::log(LogLevel lvl, const char* fmt, ...) { va_list ap; va_start(ap, fmt); vlog(lvl, fmt, ap); va_end(ap); }
void Logger::debug(const char* fmt, ...)             { va_list ap; va_start(ap, fmt); vlog(LogLevel::DEBUG, fmt, ap); va_end(ap); }
void Logger::info(const char* fmt, ...)              { va_list ap; va_start(ap, fmt); vlog(LogLevel::INFO,  fmt, ap); va_end(ap); }
void Logger::warn(const char* fmt, ...)              { va_list ap; va_start(ap, fmt); vlog(LogLevel::WARN,  fmt, ap); va_end(ap); }
void Logger::error(const char* fmt, ...)             { va_list ap; va_start(ap, fmt); vlog(LogLevel::ERROR, fmt, ap); va_end(ap); }

void Logger::vlog(LogLevel lvl, const char* fmt, va_list ap) {
    char buf[240];
    vsnprintf(buf, sizeof(buf), fmt, ap);
    emit(lvl, buf);
}

void Logger::emit(LogLevel lvl, const char* msg) {
    // --- Serial ---
    if ((uint8_t)lvl >= (uint8_t)m_serialLevel) {
        Serial.printf("[%lu][%s] %s\n", millis(), levelName(lvl), msg);
    }

    // --- MQTT ---
    if (!m_publish || !m_logTopic) return;
    if ((uint8_t)lvl < (uint8_t)m_mqttLevel) return;

    if (m_tokens <= 0) { if (m_dropped < 0xFFFF) m_dropped++; return; }
    m_tokens--;

    char esc[300];
    jsonEscape(msg, esc, sizeof(esc));
    char payload[360];
    snprintf(payload, sizeof(payload), "{\"lvl\":\"%s\",\"msg\":\"%s\"}", levelName(lvl), esc);
    m_publish(m_logTopic, payload);
}

void Logger::tick() {
    uint32_t now = millis();
    if (now - m_lastRefill >= 1000) {
        m_lastRefill = now;
        m_tokens = m_ratePerSec;
        if (m_dropped > 0 && m_publish && m_logTopic) {
            char payload[96];
            snprintf(payload, sizeof(payload),
                     "{\"lvl\":\"WARN\",\"msg\":\"%u log damlasi (rate-limit)\"}", m_dropped);
            m_publish(m_logTopic, payload);
            m_dropped = 0;
            m_tokens--;
        }
    }
}

size_t Logger::jsonEscape(const char* in, char* out, size_t outSize) {
    size_t o = 0;
    for (const char* p = in; *p && o + 2 < outSize; ++p) {
        unsigned char c = (unsigned char)*p;
        switch (c) {
            case '"':  out[o++] = '\\'; out[o++] = '"';  break;
            case '\\': out[o++] = '\\'; out[o++] = '\\'; break;
            case '\n': out[o++] = '\\'; out[o++] = 'n';  break;
            case '\r': break;
            case '\t': out[o++] = ' '; break;
            default:
                if (c < 0x20) { /* diğer kontrol karakterleri at */ }
                else out[o++] = (char)c;
        }
    }
    out[o] = '\0';
    return o;
}
