#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>

enum WiFiState {
    WIFI_STATE_DISCONNECTED,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_CONNECTED,
    WIFI_STATE_AP_MODE
};

// Khởi tạo WiFi: đọc SSID/Password đã lưu trong Preferences,
// nếu có thì thử kết nối, nếu không (hoặc thất bại) thì chuyển sang AP mode.
void wifiManagerInit();

// Gọi liên tục trong loop() - xử lý reconnect không chặn chương trình (dùng millis())
void wifiManagerLoop();

// Lưu SSID/Password mới vào Preferences và thử kết nối lại
bool wifiManagerSaveCredentials(const String& ssid, const String& password);

// Xoá cấu hình WiFi đã lưu, quay lại chế độ AP
void wifiManagerReset();

// Trạng thái hiện tại
WiFiState wifiManagerGetState();
String    wifiManagerGetStateText();  // "Đã kết nối" / "Đang kết nối" / "Chế độ AP" / "Mất kết nối"
String    wifiManagerGetIP();
int       wifiManagerGetRSSI();

#endif // WIFI_MANAGER_H
