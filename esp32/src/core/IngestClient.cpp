#include "IngestClient.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

IngestClient::IngestClient(Logger& logger, const char* url, const char* token, const char* deviceId)
    : m_log(logger), m_url(url), m_token(token), m_device(deviceId) {}

void IngestClient::begin() {
    m_q = xQueueCreate(8, sizeof(Job));
    xTaskCreatePinnedToCore(taskEntry, "ingest", 16384, this, 1, nullptr, tskNO_AFFINITY);
}

bool IngestClient::post(const char* kind, const String& dataJson, uint32_t epoch) {
    if (!m_q) return false;
    Job j;
    strlcpy(j.kind, kind, sizeof(j.kind));
    j.ts = epoch;
    j.data = strdup(dataJson.c_str());
    if (!j.data) return false;
    if (xQueueSend(m_q, &j, 0) != pdTRUE) {
        free(j.data);
        m_log.warn("ingest kuyrugu dolu, atlandi (%s)", kind);
        return false;
    }
    return true;
}

void IngestClient::taskEntry(void* self) { static_cast<IngestClient*>(self)->run(); }

void IngestClient::run() {
    Job j;
    for (;;) {
        if (xQueueReceive(m_q, &j, portMAX_DELAY) == pdTRUE) {
            doPost(j.kind, j.data, j.ts);
            free(j.data);
        }
    }
}

bool IngestClient::doPost(const char* kind, const char* data, uint32_t epoch) {
    if (WiFi.status() != WL_CONNECTED) return false;

    String body;
    body.reserve(strlen(data) + 160);
    body  = "{\"secret\":\"";   body += m_token;
    body += "\",\"kind\":\"";   body += kind;
    body += "\",\"device\":\""; body += m_device;
    body += "\",\"ts\":";       body += String(epoch);
    body += ",\"data\":";       body += data;
    body += "}";

    WiFiClientSecure tls;
    tls.setInsecure();
    HTTPClient http;
    http.setConnectTimeout(4000);
    http.setTimeout(6000);

    if (!http.begin(tls, m_url)) {
        Serial.println("[ingest] http.begin basarisiz");
        return false;
    }
    http.addHeader("Content-Type", "application/json");
    int code = http.POST(body);
    http.end();

    if (code >= 200 && code < 300) return true;
    Serial.printf("[ingest] %s -> %d\n", kind, code);
    return false;
}
