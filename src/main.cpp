/*
 * ============================================================
 *  DỰ ÁN: KHÔNG GIAN XANH
 *  Hệ thống theo dõi môi trường phòng thời gian thực
 *  Nền tảng: ESP32 DevKit V1
 *  Cảm biến: SHT31-D (nhiệt độ/độ ẩm) + module ENS160+AHT21 (TVOC/eCO2/AQI)
 *  Hiển thị: OLED SSD1306 128x64 (I2C)
 *  Kết nối: WiFi (STA + AP fallback), Web Dashboard (WebSocket realtime)
 * ============================================================
 */

#include <Arduino.h>
#include <time.h>

#include "config.h"
#include "sensor.h"
#include "display.h"
#include "wifi_manager.h"
#include "web_server.h"
#include "storage.h"

// ============================================================
//  BIẾN THỜI GIAN (non-blocking, dùng millis())
// ============================================================
static unsigned long s_lastSensorReadMs   = 0;
static unsigned long s_lastDisplayMs      = 0;
static unsigned long s_lastBlinkMs        = 0;
static unsigned long s_lastNtpSyncMs      = 0;
static bool          s_ledBlinkState      = false;

// ============================================================
//  KHỞI TẠO
// ============================================================
void setup() {
    Serial.begin(115200);
    delay(200); // chỉ chờ ổn định cổng Serial lúc khởi động, không lặp lại trong loop()

    Serial.println("\n=== KHONG GIAN XANH - He thong giam sat moi truong phong ===");

    // --- Chân LED / Buzzer ---
    pinMode(LED_STATUS_PIN, OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(LED_STATUS_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);

    // --- Màn hình OLED ---
    displayInit();
    displaySplashEffect();

    // --- Cảm biến ---
    if (!sensorInit()) {
        Serial.println("[MAIN] Cảnh báo: Không có cảm biến nào hoạt động lúc khởi động.");
        displayShowError("Khong tim thay cam bien");
    }

    // --- Lịch sử dữ liệu ---
    storageInit();

    // --- WiFi ---
    wifiManagerInit();

    // --- NTP (chỉ có tác dụng khi đã có WiFi STA) ---
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER_1, NTP_SERVER_2);
    s_lastNtpSyncMs = millis();

    // --- Web Server ---
    webServerInit();

    Serial.println("[MAIN] Khởi động hoàn tất. Bắt đầu vòng lặp chính.\n");
}

// ============================================================
//  XỬ LÝ CẢNH BÁO (LED nhấp nháy + Buzzer)
// ============================================================
static void handleWarning(const SensorData& data) {
    unsigned long now = millis();

    if (data.warning) {
        if (now - s_lastBlinkMs >= BLINK_INTERVAL_MS) {
            s_lastBlinkMs = now;
            s_ledBlinkState = !s_ledBlinkState;
            digitalWrite(LED_STATUS_PIN, s_ledBlinkState ? HIGH : LOW);
            digitalWrite(BUZZER_PIN, s_ledBlinkState ? HIGH : LOW);
        }
    } else {
        digitalWrite(LED_STATUS_PIN, LOW);
        digitalWrite(BUZZER_PIN, LOW);
        s_ledBlinkState = false;
    }
}

// ============================================================
//  LẤY CHUỖI THỜI GIAN HIỆN TẠI
// ============================================================
static String getTimeString() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 50)) {
        return "--:--:--";
    }
    char buf[9];
    strftime(buf, sizeof(buf), "%H:%M:%S", &timeinfo);
    return String(buf);
}

// ============================================================
//  VÒNG LẶP CHÍNH - KHÔNG DÙNG delay()
// ============================================================
void loop() {
    unsigned long now = millis();

    // --- WiFi (non-blocking) ---
    wifiManagerLoop();

    // --- Đọc cảm biến mỗi 2 giây ---
    if (now - s_lastSensorReadMs >= SENSOR_READ_INTERVAL_MS) {
        s_lastSensorReadMs = now;
        sensorRetryIfNeeded();
        sensorRead();
        storageLoop(g_sensorData);
    }

    // --- Cập nhật OLED mỗi 2 giây ---
    if (now - s_lastDisplayMs >= DISPLAY_UPDATE_INTERVAL_MS) {
        s_lastDisplayMs = now;
        displayUpdate(g_sensorData, wifiManagerGetStateText(), getTimeString());
    }

    // --- Đồng bộ lại NTP định kỳ ---
    if (now - s_lastNtpSyncMs >= NTP_SYNC_INTERVAL_MS) {
        s_lastNtpSyncMs = now;
        configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER_1, NTP_SERVER_2);
    }

    // --- Cảnh báo LED/Buzzer ---
    handleWarning(g_sensorData);

    // --- Web server: đẩy dữ liệu WebSocket + xử lý OTA ---
    webServerLoop(g_sensorData, wifiManagerGetStateText(), getTimeString());

    // Không có delay() nào ở đây -> vòng lặp phản hồi nhanh, đa nhiệm mượt mà
}
