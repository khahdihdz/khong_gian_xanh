/*
 * ============================================================
 *  DỰ ÁN: KHÔNG GIAN XANH
 *  Hệ thống theo dõi môi trường phòng thời gian thực
 *  Nền tảng: ESP32 DevKit V1
 *  Cảm biến: SHT31-D + ENS160+AHT21
 *  Hiển thị: OLED SSD1306 128x64
 *  Kết nối: WiFi + HTTP/WebSocket cục bộ
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

static unsigned long s_lastSensorReadMs = 0;
static unsigned long s_lastDisplayMs = 0;
static unsigned long s_lastBlinkMs = 0;
static unsigned long s_lastNtpSyncMs = 0;
static bool s_ledBlinkState = false;

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\n=== KHONG GIAN XANH - HTTP + LOCAL WEBSOCKET ===");

    pinMode(LED_STATUS_PIN, OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(LED_STATUS_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);

    displayInit();
    displaySplashEffect();

    if (!sensorInit()) {
        Serial.println("[MAIN] Cảnh báo: Không có cảm biến hoạt động lúc khởi động.");
        displayShowError("Không tìm thấy cảm biến");
    }

    storageInit();
    wifiManagerInit();

    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER_1, NTP_SERVER_2);
    s_lastNtpSyncMs = millis();

    webServerInit();
    Serial.println("[MAIN] Khởi động hoàn tất.\n");
}

static void handleWarning(const SensorData& data) {
    unsigned long now = millis();

    if (!data.warning) {
        digitalWrite(LED_STATUS_PIN, LOW);
        digitalWrite(BUZZER_PIN, LOW);
        s_ledBlinkState = false;
        return;
    }

    // Cảnh báo không chặn loop: LED và buzzer chỉ đổi trạng thái theo chu kỳ.
    if (now - s_lastBlinkMs >= BLINK_INTERVAL_MS) {
        s_lastBlinkMs = now;
        s_ledBlinkState = !s_ledBlinkState;
        digitalWrite(LED_STATUS_PIN, s_ledBlinkState ? HIGH : LOW);
        digitalWrite(BUZZER_PIN, s_ledBlinkState ? HIGH : LOW);
    }
}

static String getTimeString() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 50)) return "--:--:--";
    char buf[9];
    strftime(buf, sizeof(buf), "%H:%M:%S", &timeinfo);
    return String(buf);
}

void loop() {
    unsigned long now = millis();

    // Wi-Fi được xử lý không chặn; cảm biến và OLED vẫn hoạt động khi mất mạng.
    wifiManagerLoop();

    if (now - s_lastSensorReadMs >= SENSOR_READ_INTERVAL_MS) {
        s_lastSensorReadMs = now;
        sensorRetryIfNeeded();
        sensorRead();
        storageLoop(g_sensorData);
    }

    if (now - s_lastDisplayMs >= DISPLAY_UPDATE_INTERVAL_MS) {
        s_lastDisplayMs = now;
        displayUpdate(g_sensorData, wifiManagerGetStateText(), getTimeString());
    }

    if (now - s_lastNtpSyncMs >= NTP_SYNC_INTERVAL_MS) {
        s_lastNtpSyncMs = now;
        configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER_1, NTP_SERVER_2);
    }

    handleWarning(g_sensorData);
    webServerLoop(g_sensorData, wifiManagerGetStateText(), getTimeString());
}
