#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include "sensor.h"

// Khởi tạo web server, WebSocket, các route API, trang cấu hình WiFi và OTA.
// Yêu cầu LittleFS đã được mount (chứa index.html, wifi_config.html, ...)
void webServerInit();

// Gọi định kỳ trong loop() để đẩy dữ liệu realtime qua WebSocket
void webServerLoop(const SensorData& data, const String& wifiStatus, const String& timeStr);

// Dọn dẹp các client WebSocket đã ngắt kết nối (nên gọi định kỳ)
void webServerCleanupClients();

#endif // WEB_SERVER_H
