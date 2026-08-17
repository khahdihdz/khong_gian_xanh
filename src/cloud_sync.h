#ifndef CLOUD_SYNC_H
#define CLOUD_SYNC_H

#include <Arduino.h>
#include "sensor.h"

// ============================================================
//  ĐỒNG BỘ CLOUD - theo dõi từ xa, không cần cùng mạng WiFi với ESP32
//  Định kỳ đẩy dữ liệu hiện tại lên 1 relay (vd: Cloudflare Worker,
//  xem thư mục cloud-relay/) qua HTTPS POST. Dashboard từ xa
//  (cloud-dashboard/) đọc dữ liệu từ chính relay đó bằng HTTPS GET,
//  hoàn toàn không cần kết nối vào mạng LAN/WiFi nội bộ của ESP32.
// ============================================================

// Đọc cấu hình đã lưu trong Preferences (nếu có)
void cloudSyncInit();

// Gọi định kỳ trong loop(); tự quyết định khi nào cần đẩy dữ liệu
// (mỗi CLOUD_SYNC_INTERVAL_MS, chỉ khi đã bật và đang có WiFi STA)
void cloudSyncLoop(const SensorData& data);

// Lưu cấu hình mới (URL relay, token thiết bị, bật/tắt) vào Preferences
void cloudSyncSaveConfig(const String& url, const String& token, bool enabled);

bool   cloudSyncIsEnabled();
String cloudSyncGetUrl();
String cloudSyncGetToken();      // Chỉ dùng nội bộ khi build request, KHÔNG trả về cho client qua API
bool   cloudSyncHasToken();
String cloudSyncGetStatusText();  // Trạng thái lần đồng bộ gần nhất, hiển thị trên trang Chức năng

#endif // CLOUD_SYNC_H
