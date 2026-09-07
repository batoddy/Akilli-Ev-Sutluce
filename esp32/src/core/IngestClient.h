#pragma once
// ============================================================================
//  IngestClient — önemli olayları Vercel /api/ingest'e HTTPS POST eder.
//  Gövde: {"secret","kind","device","ts","data":{...}}
//  Not: HTTPClient bloklar (~1-3 sn TLS). Sadece seyrek olaylar için çağır
//  (event, 5 dk telemetri, ack, WARN/ERROR log). MQTT keepalive 30 sn -> güvenli.
// ============================================================================

#include <Arduino.h>
#include "Logger.h"

class IngestClient {
public:
    IngestClient(Logger& logger, const char* url, const char* token, const char* deviceId);

    // dataJson: süslü parantezli GEÇERLİ bir JSON obje metni, örn: {"type":"motion"}
    bool post(const char* kind, const String& dataJson, uint32_t epoch);

private:
    Logger&     m_log;
    const char* m_url;
    const char* m_token;
    const char* m_device;
};
