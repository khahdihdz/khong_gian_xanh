#include "storage.h"
#include "config.h"
#include <time.h>

// ============================================================
//  BỘ ĐỆM VÒNG (RING BUFFER) LƯU TRONG RAM
//  Lưu ý: dữ liệu lịch sử sẽ mất khi ESP32 khởi động lại.
//  Nếu cần lưu bền vững, có thể mở rộng ghi ra LittleFS theo lô.
// ============================================================
static HistoryRecord s_history[HISTORY_MAX_RECORDS];
static size_t s_count = 0;      // Số bản ghi hiện có (tối đa HISTORY_MAX_RECORDS)
static size_t s_head  = 0;      // Vị trí ghi tiếp theo (vòng tròn)
static unsigned long s_lastLogMs = 0;

void storageInit() {
    s_count = 0;
    s_head = 0;
    s_lastLogMs = millis();
}

void storageAddRecord(const SensorData& data) {
    time_t now;
    time(&now);

    HistoryRecord rec;
    rec.epochTime   = (uint32_t)now;
    rec.temperature = data.temperature;
    rec.humidity    = data.humidity;
    rec.tvoc        = data.tvoc;
    rec.eco2        = data.eco2;
    rec.aqi         = data.aqi;

    s_history[s_head] = rec;
    s_head = (s_head + 1) % HISTORY_MAX_RECORDS;
    if (s_count < HISTORY_MAX_RECORDS) s_count++;
}

void storageLoop(const SensorData& data) {
    unsigned long now = millis();
    if (now - s_lastLogMs >= HISTORY_LOG_INTERVAL_MS) {
        s_lastLogMs = now;
        storageAddRecord(data);
    }
}

size_t storageGetRecordCount() { return s_count; }

// Trả về chỉ số bản ghi cũ nhất
static size_t oldestIndex() {
    if (s_count < HISTORY_MAX_RECORDS) return 0;
    return s_head; // khi đầy, head chính là vị trí cũ nhất sẽ bị ghi đè tiếp theo
}

String storageGetHistoryJson(uint8_t hoursFilter) {
    String json = "[";
    time_t now;
    time(&now);
    uint32_t cutoff = 0;
    if (hoursFilter > 0) {
        cutoff = (uint32_t)now - (uint32_t)hoursFilter * 3600UL;
    }

    size_t start = oldestIndex();
    bool first = true;
    for (size_t i = 0; i < s_count; i++) {
        size_t idx = (start + i) % HISTORY_MAX_RECORDS;
        HistoryRecord& r = s_history[idx];
        if (hoursFilter > 0 && r.epochTime < cutoff) continue;

        if (!first) json += ",";
        first = false;

        json += "{";
        json += "\"time\":" + String(r.epochTime) + ",";
        json += "\"temperature\":" + String(r.temperature, 1) + ",";
        json += "\"humidity\":" + String(r.humidity, 1) + ",";
        json += "\"tvoc\":" + String(r.tvoc) + ",";
        json += "\"eco2\":" + String(r.eco2) + ",";
        json += "\"aqi\":" + String(r.aqi);
        json += "}";
    }
    json += "]";
    return json;
}

String storageGetHistoryCsv() {
    String csv = "time_unix,thoi_gian,nhiet_do_C,do_am_pct,tvoc_ppb,eco2_ppm,aqi\n";
    size_t start = oldestIndex();
    for (size_t i = 0; i < s_count; i++) {
        size_t idx = (start + i) % HISTORY_MAX_RECORDS;
        HistoryRecord& r = s_history[idx];

        time_t t = (time_t)r.epochTime;
        struct tm timeinfo;
        localtime_r(&t, &timeinfo);
        char buf[24];
        strftime(buf, sizeof(buf), "%d/%m/%Y %H:%M:%S", &timeinfo);

        csv += String(r.epochTime) + ",";
        csv += String(buf) + ",";
        csv += String(r.temperature, 1) + ",";
        csv += String(r.humidity, 1) + ",";
        csv += String(r.tvoc) + ",";
        csv += String(r.eco2) + ",";
        csv += String(r.aqi) + "\n";
    }
    return csv;
}
