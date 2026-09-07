#pragma once
// ============================================================================
//  Telemetry — MIMARI.md §4 / Karar #4
//  - 60 sn: ev/<dev>/telemetry  (retained, MQTT)  — DB'ye yazılmaz
//  - 5 dk : /api/ingest kind=telemetry            — geçmiş/grafik
//  - 24 sa: INFO heartbeat log                    — logs tablosuna 1 satır
// ============================================================================

#include <Arduino.h>
#include "NetworkManager.h"
#include "Logger.h"
#include "IngestClient.h"

class Telemetry {
public:
    Telemetry(NetworkManager& net, Logger& log, IngestClient& ingest);
    void loop();

private:
    String snapshot();          // {"rssi":..,"heap":..,"uptime":..,"ip":".."}

    NetworkManager& m_net;
    Logger&         m_log;
    IngestClient&   m_ingest;

    uint32_t m_lastLive = 0;
    uint32_t m_lastHist = 0;
    uint32_t m_lastBeat = 0;
    bool     m_firstBeatDone = false;
};
