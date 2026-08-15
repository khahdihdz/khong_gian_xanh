#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ============================================================
//  THÔNG TIN DỰ ÁN
// ============================================================
#define PROJECT_NAME    "Không Gian Xanh"
#define FIRMWARE_VERSION "1.1.0"

// ============================================================
//  CẤU HÌNH CHÂN (PIN) - ESP32 DevKit V1 (30 chân)
// ============================================================

// --- I2C dùng chung cho OLED, SHT31-D và module ENS160+AHT21 ---
// Toàn bộ cảm biến trong bản thiết kế này đều giao tiếp qua I2C
// (không còn chân data rời như DHT22 kiểu cũ).
#define I2C_SDA_PIN     21
#define I2C_SCL_PIN     22

#define OLED_ADDRESS    0x3C
#define OLED_WIDTH      128
#define OLED_HEIGHT     64

// --- SHT31-D (Nhiệt độ / Độ ẩm, I2C) ---
#define SHT31_ADDRESS   0x44   // Một số module dùng 0x45 nếu chân ADDR nối VCC, tự dò trong sensor.cpp

// --- Module ENS160 + AHT21 (chất lượng không khí) ---
// Chỉ dùng phần ENS160 (TVOC/eCO2/AQI); AHT21 tích hợp sẵn trên module
// không được đọc vì nhiệt độ/độ ẩm đã lấy từ SHT31-D chính xác hơn.
#define ENS160_ADDRESS  0x53   // Một số board dùng 0x52, tự dò trong sensor.cpp

// --- LED trạng thái & Buzzer ---
#define LED_STATUS_PIN  25     // LED đỏ báo cảnh báo
#define LED_WIFI_PIN    2      // LED onboard báo trạng thái WiFi
#define BUZZER_PIN      26

// ============================================================
//  THỜI GIAN CẬP NHẬT (không dùng delay, dùng millis())
// ============================================================
#define SENSOR_READ_INTERVAL_MS     2000    // Đọc cảm biến mỗi 2 giây
#define DISPLAY_UPDATE_INTERVAL_MS  2000    // Cập nhật OLED mỗi 2 giây
#define WEBSOCKET_PUSH_INTERVAL_MS  2000    // Đẩy dữ liệu realtime qua WebSocket
#define WIFI_RECONNECT_INTERVAL_MS  10000   // Thử kết nối lại WiFi mỗi 10 giây
#define AP_GRACE_AFTER_CONNECT_MS   120000UL // Giữ AP thêm 2 phút sau khi kết nối STA thành công (đủ thời gian để trang cấu hình đọc IP mới)
#define SENSOR_ERROR_RETRY_MS       5000    // Thử khởi tạo lại cảm biến lỗi mỗi 5 giây
#define BLINK_INTERVAL_MS           300     // Nhấp nháy LED cảnh báo
#define NTP_SYNC_INTERVAL_MS        3600000UL // Đồng bộ lại NTP mỗi 1 giờ

// ============================================================
//  NGƯỠNG CẢNH BÁO
// ============================================================
#define TEMP_WARNING_THRESHOLD      35.0f   // °C
#define HUMIDITY_WARNING_THRESHOLD  80.0f   // %
#define ECO2_WARNING_THRESHOLD      1200    // ppm
#define TVOC_WARNING_THRESHOLD      500     // ppb (theo khuyến nghị chung)

// ============================================================
//  WIFI AP MODE (Chế độ phát WiFi khi chưa cấu hình)
// ============================================================
#define AP_SSID         "KhongGianXanh-Setup"
#define AP_PASSWORD     "12345678"

// ============================================================
//  NTP - MÚI GIỜ VIỆT NAM (GMT+7)
// ============================================================
#define NTP_SERVER_1    "pool.ntp.org"
#define NTP_SERVER_2    "time.nist.gov"
#define GMT_OFFSET_SEC  (7 * 3600)
#define DAYLIGHT_OFFSET_SEC 0

// ============================================================
//  LỊCH SỬ DỮ LIỆU
// ============================================================
#define HISTORY_MAX_RECORDS  1000
#define HISTORY_LOG_INTERVAL_MS 60000UL   // Ghi 1 bản ghi lịch sử mỗi 60 giây

// ============================================================
//  PREFERENCES (LƯU TRỮ CẤU HÌNH KHÔNG MẤT KHI MẤT ĐIỆN)
// ============================================================
#define PREF_NAMESPACE   "airmon"
#define PREF_KEY_SSID    "wifi_ssid"
#define PREF_KEY_PASS    "wifi_pass"
#define PREF_KEY_CLOUD_URL   "cloud_url"
#define PREF_KEY_CLOUD_TOKEN "cloud_tok"
#define PREF_KEY_CLOUD_ON    "cloud_on"

// ============================================================
//  ĐỒNG BỘ CLOUD (xem dữ liệu từ xa, không cần cùng mạng LAN)
//  Xem thư mục cloud-relay/ để tự triển khai Cloudflare Worker relay.
// ============================================================
#define CLOUD_SYNC_INTERVAL_MS   60000UL   // Đẩy dữ liệu lên cloud mỗi 60 giây
#define CLOUD_SYNC_TIMEOUT_MS    8000UL    // Timeout cho mỗi lần gọi HTTP

#endif // CONFIG_H
