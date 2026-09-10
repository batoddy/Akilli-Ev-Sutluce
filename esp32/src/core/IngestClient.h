#pragma once
// ============================================================================
//  IngestClient — önemli olayları Vercel /api/ingest'e HTTPS POST eder.
//  post() KUYRUĞA ATAR ve hemen döner (bloklamaz). Gerçek POST'lar ayrı bir
//  FreeRTOS görevinde yapılır → ana döngü / komut / röle asla beklemez.
// ============================================================================

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include "Logger.h"

class IngestClient {
public:
    IngestClient(Logger& logger, const char* url, const char* token, const char* deviceId);

    void begin();     // arka plan görevini + kuyruğu başlatır
    bool post(const char* kind, const String& dataJson, uint32_t epoch);  // kuyruğa at

private:
    struct Job { char kind[20]; char* data; uint32_t ts; };

    static void taskEntry(void* self);
    void run();
    bool doPost(const char* kind, const char* data, uint32_t epoch);

    Logger&       m_log;
    const char*   m_url;
    const char*   m_token;
    const char*   m_device;
    QueueHandle_t m_q = nullptr;
};
