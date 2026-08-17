#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

#define PROJECT_NAME    "Không Gian Xanh"
#define FIRMWARE_VERSION "1.3.2"

#define I2C_SDA_PIN     21
#define I2C_SCL_PIN     22
#define OLED_ADDRESS    0x3C
#define OLED_WIDTH      128
#define OLED_HEIGHT     64
#define SHT31_ADDRESS   0x44
#define ENS160_ADDRESS  0x53
#define LED_STATUS_PIN  25
#define LED_WIFI_PIN    2
#define BUZZER_PIN      26

#define SENSOR_READ_INTERVAL_MS     2000
#define DISPLAY_UPDATE_INTERVAL_MS  2000
#define WEBSOCKET_PUSH_INTERVAL_MS  2000
#define WIFI_RECONNECT_INTERVAL_MS  10000
#define AP_GRACE_AFTER_CONNECT_MS   120000UL
#define SENSOR_ERROR_RETRY_MS       5000
#define BLINK_INTERVAL_MS           300
#define NTP_SYNC_INTERVAL_MS        3600000UL

#define TEMP_WARNING_THRESHOLD      35.0f
#define HUMIDITY_WARNING_THRESHOLD  80.0f
#define ECO2_WARNING_THRESHOLD      1200
#define TVOC_WARNING_THRESHOLD      500

#define AP_SSID         "KhongGianXanh-Setup"
#define AP_PASSWORD     "12345678"

#define NTP_SERVER_1    "pool.ntp.org"
#define NTP_SERVER_2    "time.nist.gov"
#define GMT_OFFSET_SEC  (7 * 3600)
#define DAYLIGHT_OFFSET_SEC 0

#define HISTORY_MAX_RECORDS  1000
#define HISTORY_LOG_INTERVAL_MS 60000UL

#define PREF_NAMESPACE       "airmon"
#define PREF_KEY_SSID        "wifi_ssid"
#define PREF_KEY_PASS        "wifi_pass"
#define PREF_KEY_CLOUD_URL   "cloud_url"
#define PREF_KEY_CLOUD_TOKEN "cloud_tok"
#define PREF_KEY_CLOUD_ON    "cloud_on"

#define CLOUD_SYNC_INTERVAL_MS   60000UL
#define CLOUD_SYNC_TIMEOUT_MS    8000UL

#endif // CONFIG_H
