#include "wifi_manager.h"
#include "config.h"

#include <WiFi.h>
#include <Preferences.h>

static Preferences prefs;
static WiFiState s_state = WIFI_STATE_DISCONNECTED;
static unsigned long s_lastAttemptMs = 0;
static String s_savedSsid;
static String s_savedPass;
static bool s_apModeActive = false;
static unsigned long s_connectedAtMs = 0;

// ------------------------------------------------------------
static void startAPMode() {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASSWORD);
    s_apModeActive = true;
    s_state = WIFI_STATE_AP_MODE;
    Serial.print("[WIFI] Chế độ AP đã bật. SSID: ");
    Serial.print(AP_SSID);
    Serial.print(" | IP: ");
    Serial.println(WiFi.softAPIP());
}

// ------------------------------------------------------------
static void tryConnectSTA() {
    if (s_savedSsid.length() == 0) {
        startAPMode();
        return;
    }
    // Nếu AP đang phát (vd. người dùng vừa lưu WiFi từ trang cấu hình khi đang
    // nối vào AP KhongGianXanh-Setup), giữ AP sống song song trong lúc thử kết
    // nối STA, để trang cấu hình có thể tiếp tục hỏi /api/info qua AP và hiển
    // thị địa chỉ IP mới ngay khi kết nối thành công, thay vì mất kết nối ngay
    // lập tức.
    WiFi.mode(s_apModeActive ? WIFI_AP_STA : WIFI_STA);
    WiFi.begin(s_savedSsid.c_str(), s_savedPass.c_str());
    s_state = WIFI_STATE_CONNECTING;
    s_lastAttemptMs = millis();
    Serial.print("[WIFI] Đang kết nối tới: ");
    Serial.println(s_savedSsid);
}

// ------------------------------------------------------------
void wifiManagerInit() {
    prefs.begin(PREF_NAMESPACE, false);
    s_savedSsid = prefs.getString(PREF_KEY_SSID, "");
    s_savedPass = prefs.getString(PREF_KEY_PASS, "");

    pinMode(LED_WIFI_PIN, OUTPUT);
    digitalWrite(LED_WIFI_PIN, LOW);

    if (s_savedSsid.length() > 0) {
        tryConnectSTA();
    } else {
        startAPMode();
    }
}

// ------------------------------------------------------------
void wifiManagerLoop() {
    unsigned long now = millis();

    if (s_state == WIFI_STATE_CONNECTING) {
        if (WiFi.status() == WL_CONNECTED) {
            s_state = WIFI_STATE_CONNECTED;
            s_connectedAtMs = now;
            digitalWrite(LED_WIFI_PIN, HIGH);
            Serial.print("[WIFI] Đã kết nối. IP: ");
            Serial.println(WiFi.localIP());
            // Không tắt AP ngay (nếu đang giữ song song) - để trang cấu hình
            // còn kịp đọc IP mới; AP sẽ tự tắt sau AP_GRACE_AFTER_CONNECT_MS.
        } else if (now - s_lastAttemptMs > 15000UL) {
            // Quá 15s không kết nối được -> chuyển sang AP mode để người dùng cấu hình lại
            Serial.println("[WIFI] Kết nối thất bại, chuyển sang chế độ AP.");
            startAPMode();
        }
    } else if (s_state == WIFI_STATE_CONNECTED) {
        digitalWrite(LED_WIFI_PIN, HIGH);
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("[WIFI] Mất kết nối, sẽ thử lại...");
            s_state = WIFI_STATE_DISCONNECTED;
            s_apModeActive = false;
            s_lastAttemptMs = now;
        } else if (s_apModeActive && now - s_connectedAtMs >= AP_GRACE_AFTER_CONNECT_MS) {
            // Đã kết nối ổn định và đủ thời gian để trang cấu hình đọc được
            // IP mới -> tắt AP để giải phóng tài nguyên / giảm nhiễu sóng.
            WiFi.softAPdisconnect(true);
            WiFi.mode(WIFI_STA);
            s_apModeActive = false;
            Serial.println("[WIFI] Đã tắt chế độ AP sau khi kết nối ổn định.");
        }
    } else if (s_state == WIFI_STATE_DISCONNECTED) {
        digitalWrite(LED_WIFI_PIN, LOW);
        if (now - s_lastAttemptMs >= WIFI_RECONNECT_INTERVAL_MS) {
            tryConnectSTA();
        }
    } else if (s_state == WIFI_STATE_AP_MODE) {
        // Nhấp nháy nhẹ LED để báo đang ở chế độ AP chờ cấu hình
        digitalWrite(LED_WIFI_PIN, (now / 500) % 2);

        // Nếu đã có SSID lưu trước đó, thử kết nối lại định kỳ trong khi vẫn giữ AP
        if (s_savedSsid.length() > 0 && now - s_lastAttemptMs >= WIFI_RECONNECT_INTERVAL_MS) {
            s_lastAttemptMs = now;
            WiFi.mode(WIFI_AP_STA);
            WiFi.begin(s_savedSsid.c_str(), s_savedPass.c_str());
            s_state = WIFI_STATE_CONNECTING;
        }
    }
}

// ------------------------------------------------------------
bool wifiManagerSaveCredentials(const String& ssid, const String& password) {
    if (ssid.length() == 0) return false;
    prefs.putString(PREF_KEY_SSID, ssid);
    prefs.putString(PREF_KEY_PASS, password);
    s_savedSsid = ssid;
    s_savedPass = password;
    tryConnectSTA();
    return true;
}

// ------------------------------------------------------------
void wifiManagerReset() {
    prefs.remove(PREF_KEY_SSID);
    prefs.remove(PREF_KEY_PASS);
    s_savedSsid = "";
    s_savedPass = "";
    startAPMode();
}

// ------------------------------------------------------------
WiFiState wifiManagerGetState() { return s_state; }

String wifiManagerGetStateText() {
    switch (s_state) {
        case WIFI_STATE_CONNECTED:    return "Đã kết nối";
        case WIFI_STATE_CONNECTING:   return "Đang kết nối";
        case WIFI_STATE_AP_MODE:      return "Chế độ AP";
        default:                      return "Mất kết nối";
    }
}

String wifiManagerGetIP() {
    if (s_state == WIFI_STATE_CONNECTED) return WiFi.localIP().toString();
    if (s_state == WIFI_STATE_AP_MODE)   return WiFi.softAPIP().toString();
    return "0.0.0.0";
}

int wifiManagerGetRSSI() {
    if (s_state == WIFI_STATE_CONNECTED) return WiFi.RSSI();
    return 0;
}
