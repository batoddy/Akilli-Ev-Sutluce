#include "IngestClient.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

IngestClient::IngestClient(Logger& logger, const char* url, const char* token, const char* deviceId)
    : m_log(logger), m_url(url), m_token(token), m_device(deviceId) {}

bool IngestClient::post(const char* kind, const String& dataJson, uint32_t epoch) {
    if (WiFi.status() != WL_CONNECTED) return false;

    String body;
    body.reserve(dataJson.length() + 160);
    body  = "{\"secret\":\"";  body += m_token;
    body += "\",\"kind\":\"";  body += kind;
    body += "\",\"device\":\"";body += m_device;
    body += "\",\"ts\":";      body += String(epoch);
    body += ",\"data\":";      body += dataJson;
    body += "}";

    WiFiClientSecure tls;
    tls.setInsecure();
    HTTPClient http;
    http.setConnectTimeout(4000);
    http.setTimeout(6000);

    if (!http.begin(tls, m_url)) {
        m_log.warn("ingest: http.begin basarisiz");
        return false;
    }
    http.addHeader("Content-Type", "application/json");
    int code = http.POST(body);
    http.end();

    if (code >= 200 && code < 300) {
        m_log.debug("ingest %s -> %d", kind, code);
        return true;
    }
    m_log.warn("ingest %s -> %d", kind, code);
    return false;
}
