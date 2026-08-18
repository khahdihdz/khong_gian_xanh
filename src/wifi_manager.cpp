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
static bool s_pendingConnect = false;
static unsigned long s_pendingConnectAtMs = 0;

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

    // Nếu AP đang phát, giữ AP song song trong lúc thử kết nối STA.
    // Việc chuyển WiFi được trì hoãn sau khi HTTP response đã có thời gian
    // gửi về trình duyệt, tránh POST /api/wifi-config bị treo.
    WiFi.mode(s_apModeActive ? WIFI_AP_STA : WIFI_STA);
    WiFi.begin(s_savedSsid.c_str(), s_savedPass.c_str());
    s_state = WIFI_STATE_CONNECTING;
    s_lastAttemptMs = millis();
    s_pendingConnect = false;
    s_pendingConnectAtMs = 0;
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

    // Chờ ít nhất 1 giây sau khi nhận POST /api/wifi-config rồi mới gọi
    // WiFi.begin(). Khoảng đệm này giúp AsyncWebServer hoàn tất việc gửi
    // HTTP response trước khi ESP32 thay đổi trạng thái mạng.
    if (s_pendingConnect && (long)(now - s_pendingConnectAtMs) >= 0) {
        tryConnectSTA();
    }

    if (s_state == WIFI_STATE_CONNECTING) {
        if (WiFi.status() == WL_CONNECTED) {
            s_state = WIFI_STATE_CONNECTED;
            s_connectedAtMs = now;
            digitalWrite(LED_WIFI_PIN, HIGH);
            Serial.print("[WIFI] Đã kết nối. IP: ");
            Serial.println(WiFi.localIP());
        } else if (now - s_lastAttemptMs > 15000UL) {
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
        digitalWrite(LED_WIFI_PIN, (now / 500) % 2);

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

    // Không gọi WiFi.begin() trong HTTP callback.
    // Chờ 1 giây rồi mới kết nối để POST có thể hoàn tất bình thường.
    s_pendingConnect = true;
    s_pendingConnectAtMs = millis() + 1000UL;
    s_lastAttemptMs = millis();
    return true;
}

// ------------------------------------------------------------
void wifiManagerReset() {
    prefs.remove(PREF_KEY_SSID);
    prefs.remove(PREF_KEY_PASS);
    s_savedSsid = "";
    s_savedPass = "";
    s_pendingConnect = false;
    s_pendingConnectAtMs = 0;
    startAPMode();
}

// ------------------------------------------------------------
WiFiState wifiManagerGetState() { return s_state; }

String wifiManagerGetStateText() {
    switch (s_state) {
        case WIFI_STATE_CONNECTED:  return "Đã kết nối";
        case WIFI_STATE_CONNECTING: return "Đang kết nối";
        case WIFI_STATE_AP_MODE:    return "Chế độ AP";
        default:                    return "Mất kết nối";
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
