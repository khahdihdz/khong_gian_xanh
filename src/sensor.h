#ifndef SENSOR_H
#define SENSOR_H

#include <Arduino.h>

// ============================================================
//  CẤU TRÚC DỮ LIỆU CẢM BIẾN
// ============================================================
struct SensorData {
    float temperature;      // °C
    float humidity;         // %
    uint16_t tvoc;           // ppb
    uint16_t eco2;           // ppm
    uint8_t  aqi;             // 1-5 (theo chuẩn ENS160)
    String   aqiLabel;       // Rất tốt / Tốt / Trung bình / Kém / Ô nhiễm
    bool     dhtOk;           // Trạng thái DHT22
    bool     ens160Ok;        // Trạng thái ENS160
    bool     warning;         // Có cảnh báo hay không
    String   warningReason;  // Lý do cảnh báo (ghép chuỗi)
    unsigned long lastUpdateMs;
};

// ============================================================
//  KHỞI TẠO / ĐỌC CẢM BIẾN
// ============================================================

// Khởi tạo I2C, DHT22, ENS160. Trả về true nếu ít nhất 1 cảm biến hoạt động.
bool sensorInit();

// Cố gắng khởi tạo lại các cảm biến đang lỗi (gọi định kỳ trong loop, không chặn)
void sensorRetryIfNeeded();

// Đọc toàn bộ cảm biến, cập nhật vào biến toàn cục g_sensorData.
// Hàm này KHÔNG BAO GIỜ làm treo chương trình dù cảm biến lỗi.
void sensorRead();

// Phân loại AQI dựa trên AQI thô + TVOC + eCO2, trả về nhãn tiếng Việt
String sensorClassifyAQI(uint8_t aqiRaw, uint16_t tvoc, uint16_t eco2);

// Đánh giá điều kiện cảnh báo (nhiệt độ, độ ẩm, AQI, TVOC, eCO2)
void sensorEvaluateWarning();

// Dữ liệu cảm biến toàn cục dùng chung cho display/web/storage
extern SensorData g_sensorData;

#endif // SENSOR_H
