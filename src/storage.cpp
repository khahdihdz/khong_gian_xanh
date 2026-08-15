#include "storage.h"
#include "config.h"
#include <time.h>
#include <LittleFS.h>

// ============================================================
//  BỘ ĐỆM VÒNG (RING BUFFER) TRONG RAM + LƯU BỀN VỮNG TRÊN LittleFS
//  - RAM ring-buffer phục vụ đọc nhanh cho API.
//  - Mỗi bản ghi mới cũng được ghi nối tiếp (append) vào file
//    /history_log.csv trên LittleFS, và được nạp lại khi khởi động,
//    nên lịch sử KHÔNG bị mất khi ESP32 khởi động lại / mất điện.
//  - File được "dọn" (ghi đè lại, chỉ giữ HISTORY_MAX_RECORDS bản ghi
//    mới nhất) định kỳ để không phình to vô hạn.
// ============================================================
static HistoryRecord s_history[HISTORY_MAX_RECORDS];
static size_t s_count = 0;      // Số bản ghi hiện có (tối đa HISTORY_MAX_RECORDS)
static size_t s_head  = 0;      // Vị trí ghi tiếp theo (vòng tròn)
static unsigned long s_lastLogMs = 0;

static const char* HISTORY_FILE = "/history_log.csv";
static const uint16_t TRIM_EVERY_N_APPENDS = 200; // dọn file định kỳ (không dọn mỗi lần ghi để đỡ hao mòn flash)
static uint16_t s_appendsSinceTrim = 0;

// Ngưỡng epoch hợp lệ tối thiểu (~2023-11-14). Trước khi đồng bộ NTP xong,
// time(&now) trả về giá trị rất nhỏ (gần 0 = năm 1970) - nếu ghi vào lịch sử
// sẽ tạo ra các điểm dữ liệu "rác" trông giống dữ liệu mẫu/giả trên biểu đồ
// (thời gian sai lệch hàng chục năm). Bản ghi có epoch nhỏ hơn mốc này sẽ bị bỏ qua.
static const uint32_t MIN_VALID_EPOCH = 1700000000UL;

// Trả về chỉ số bản ghi cũ nhất (khai báo trước để dùng trong storageTrimFile)
static size_t oldestIndex() {
    if (s_count < HISTORY_MAX_RECORDS) return 0;
    return s_head; // khi đầy, head chính là vị trí cũ nhất sẽ bị ghi đè tiếp theo
}

// Ghi đè lại toàn bộ file bằng đúng nội dung RAM ring-buffer hiện tại
// (đã tự giới hạn tối đa HISTORY_MAX_RECORDS bản ghi mới nhất).
static void storageTrimFile() {
    File f = LittleFS.open(HISTORY_FILE, FILE_WRITE); // FILE_WRITE ghi đè từ đầu
    if (!f) {
        Serial.println("[STORAGE] Không thể mở file lịch sử để dọn dẹp.");
        return;
    }
    size_t start = oldestIndex();
    for (size_t i = 0; i < s_count; i++) {
        size_t idx = (start + i) % HISTORY_MAX_RECORDS;
        HistoryRecord& r = s_history[idx];
        f.printf("%u,%.1f,%.1f,%u,%u,%u\n",
                 (unsigned)r.epochTime, r.temperature, r.humidity,
                 (unsigned)r.tvoc, (unsigned)r.eco2, (unsigned)r.aqi);
    }
    f.close();
}

static void appendRecordToFile(const HistoryRecord& rec) {
    File f = LittleFS.open(HISTORY_FILE, FILE_APPEND);
    if (!f) {
        Serial.println("[STORAGE] Không thể mở file lịch sử để ghi thêm.");
        return;
    }
    f.printf("%u,%.1f,%.1f,%u,%u,%u\n",
             (unsigned)rec.epochTime, rec.temperature, rec.humidity,
             (unsigned)rec.tvoc, (unsigned)rec.eco2, (unsigned)rec.aqi);
    f.close();

    if (++s_appendsSinceTrim >= TRIM_EVERY_N_APPENDS) {
        s_appendsSinceTrim = 0;
        storageTrimFile();
    }
}

void storageInit() {
    s_count = 0;
    s_head = 0;
    s_lastLogMs = millis();
    s_appendsSinceTrim = 0;

    // LittleFS.begin() an toàn khi gọi nhiều lần (web_server.cpp cũng mount);
    // mount ở đây trước để có thể nạp lại lịch sử ngay từ setup().
    if (!LittleFS.begin(true)) {
        Serial.println("[STORAGE] Lỗi mount LittleFS - lịch sử sẽ chỉ lưu tạm trong RAM, mất khi khởi động lại.");
        return;
    }

    // --- Nạp lại lịch sử đã lưu từ lần chạy trước (nếu có) ---
    File f = LittleFS.open(HISTORY_FILE, FILE_READ);
    if (!f) {
        Serial.println("[STORAGE] Chưa có file lịch sử cũ, bắt đầu lịch sử mới.");
        return;
    }

    size_t loaded = 0, skipped = 0;
    while (f.available()) {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.length() == 0) continue;

        int p1 = line.indexOf(',');
        int p2 = line.indexOf(',', p1 + 1);
        int p3 = line.indexOf(',', p2 + 1);
        int p4 = line.indexOf(',', p3 + 1);
        int p5 = line.indexOf(',', p4 + 1);
        if (p1 < 0 || p2 < 0 || p3 < 0 || p4 < 0 || p5 < 0) { skipped++; continue; }

        HistoryRecord rec;
        rec.epochTime   = (uint32_t)line.substring(0, p1).toInt();
        rec.temperature = line.substring(p1 + 1, p2).toFloat();
        rec.humidity    = line.substring(p2 + 1, p3).toFloat();
        rec.tvoc        = (uint16_t)line.substring(p3 + 1, p4).toInt();
        rec.eco2        = (uint16_t)line.substring(p4 + 1, p5).toInt();
        rec.aqi         = (uint8_t)line.substring(p5 + 1).toInt();

        // Bỏ qua các bản ghi "rác" còn sót từ các bản firmware cũ (trước khi
        // có kiểm tra MIN_VALID_EPOCH) để không hiện lại dữ liệu sai lệch.
        if (rec.epochTime < MIN_VALID_EPOCH) { skipped++; continue; }

        s_history[s_head] = rec;
        s_head = (s_head + 1) % HISTORY_MAX_RECORDS;
        if (s_count < HISTORY_MAX_RECORDS) s_count++;
        loaded++;
    }
    f.close();

    Serial.printf("[STORAGE] Đã nạp lại %u bản ghi lịch sử từ lần chạy trước (%u dòng bị bỏ qua).\n",
                  (unsigned)loaded, (unsigned)skipped);

    // Nếu vừa lọc bỏ dữ liệu rác, ghi lại file ngay cho sạch
    if (skipped > 0) storageTrimFile();
}

void storageAddRecord(const SensorData& data) {
    time_t now;
    time(&now);

    // Chưa đồng bộ NTP xong (đồng hồ hệ thống vẫn ở gần epoch 0) -> bỏ qua,
    // không ghi bản ghi có timestamp sai lệch vào lịch sử.
    if ((uint32_t)now < MIN_VALID_EPOCH) return;

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

    appendRecordToFile(rec);
}

void storageLoop(const SensorData& data) {
    unsigned long now = millis();
    if (now - s_lastLogMs >= HISTORY_LOG_INTERVAL_MS) {
        s_lastLogMs = now;
        storageAddRecord(data);
    }
}

size_t storageGetRecordCount() { return s_count; }

String storageGetHistoryJson(uint8_t hoursFilter) {
    String json = "[";
    time_t now;
    time(&now);
    uint32_t cutoff = 0;
    if (hoursFilter > 0) {
        // Tránh underflow (uint32_t) khi chưa đồng bộ NTP (now còn nhỏ,
        // vd = 0 lúc mới boot) - nếu không sẽ ra số RẤT LỚN, loại bỏ
        // toàn bộ lịch sử thay vì hiển thị đầy đủ như mong đợi.
        uint32_t windowSec = (uint32_t)hoursFilter * 3600UL;
        cutoff = ((uint32_t)now > windowSec) ? ((uint32_t)now - windowSec) : 0;
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
