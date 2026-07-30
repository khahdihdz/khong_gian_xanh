#ifndef STORAGE_H
#define STORAGE_H

#include <Arduino.h>
#include "sensor.h"

struct HistoryRecord {
    uint32_t epochTime;   // Unix timestamp
    float    temperature;
    float    humidity;
    uint16_t tvoc;
    uint16_t eco2;
    uint8_t  aqi;
};

// Khởi tạo bộ nhớ lịch sử (LittleFS nếu khả dụng để lưu tạm, hiện tại dùng RAM ring-buffer)
void storageInit();

// Gọi định kỳ trong loop(); tự quyết định khi nào cần thêm bản ghi mới (mỗi HISTORY_LOG_INTERVAL_MS)
void storageLoop(const SensorData& data);

// Thêm 1 bản ghi thủ công vào lịch sử
void storageAddRecord(const SensorData& data);

// Lấy toàn bộ lịch sử dưới dạng chuỗi JSON (mảng object), lọc theo số giờ gần nhất (0 = tất cả)
String storageGetHistoryJson(uint8_t hoursFilter);

// Lấy toàn bộ lịch sử dưới dạng CSV để tải xuống
String storageGetHistoryCsv();

// Số bản ghi hiện có
size_t storageGetRecordCount();

#endif // STORAGE_H
