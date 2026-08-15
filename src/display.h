#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include "sensor.h"

// Khởi tạo màn hình OLED SSD1306. Trả về true nếu tìm thấy màn hình.
bool displayInit();

// Vẽ giao diện chính: nhiệt độ, độ ẩm, AQI, TVOC, eCO2, trạng thái WiFi
void displayUpdate(const SensorData& data, const String& wifiStatus, const String& timeStr);

// Vẽ màn hình lỗi khi cảm biến không phản hồi
void displayShowError(const String& message);

// Hiệu ứng refresh nhẹ (quét ngang) khi khởi động
void displaySplashEffect();

#endif // DISPLAY_H
