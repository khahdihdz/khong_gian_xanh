#include "cloud_sync.h"
#include "config.h"
#include "wifi_manager.h"

#include <Preferences.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h>

static Preferences s_prefs;
static String s_url;
static String s_token;
static bool   s_enabled = false;
static unsigned long s_lastPushMs = 0;
static String s_statusText = "Chưa cấu hình";

// ------------------------------------------------------------
void cloudSyncInit() {
    s_prefs.begin(PREF_NAMESPACE, true); // chỉ đọc
    s_url     = s_prefs.getString(PREF_KEY_CLOUD_URL, "");
    s_token   = s_prefs.getString(PREF_KEY_CLOUD_TOKEN, "");
    s_enabled = s_prefs.getBool(PREF_KEY_CLOUD_ON, false);
    s_prefs.end();

    // Cho phép đẩy ngay trong vòng lặp đầu tiên nếu đủ điều kiện,
    // thay vì phải đợi đủ CLOUD_SYNC_INTERVAL_MS sau khi khởi động.
    s_lastPushMs = millis() - CLOUD_SYNC_INTERVAL_MS;

    if (s_enabled && s_url.length() > 0) {
        Serial.println("[CLOUD] Đồng bộ cloud đang BẬT, relay: " + s_url);
    } else {
        Serial.println("[CLOUD] Đồng bộ cloud đang TẮT (chưa cấu hình hoặc bị tắt).");
    }
}

// ------------------------------------------------------------
void cloudSyncSaveConfig(const String& url, const String& token, bool enabled) {
    s_url = url;
    s_token = token;
    s_enabled = enabled;

    s_prefs.begin(PREF_NAMESPACE, false); // ghi
    s_prefs.putString(PREF_KEY_CLOUD_URL, s_url);
    s_prefs.putString(PREF_KEY_CLOUD_TOKEN, s_token);
    s_prefs.putBool(PREF_KEY_CLOUD_ON, s_enabled);
    s_prefs.end();

    s_statusText = "Đã lưu cấu hình, chờ lần đồng bộ tiếp theo...";
    s_lastPushMs = millis() - CLOUD_SYNC_INTERVAL_MS; // thử đẩy ngay lần tới
}

bool   cloudSyncIsEnabled()    { return s_enabled && s_url.length() > 0; }
String cloudSyncGetUrl()       { return s_url; }
String cloudSyncGetToken()     { return s_token; }
bool   cloudSyncHasToken()     { return s_token.length() > 0; }
String cloudSyncGetStatusText(){ return s_statusText; }

// ------------------------------------------------------------
//  Gói dữ liệu hiện tại thành JSON để gửi lên relay (giống buildDataJson
//  trong web_server.cpp nhưng thêm timestamp dạng epoch cho lịch sử).
// ------------------------------------------------------------
static String buildPayload(const SensorData& d) {
    time_t now;
    time(&now);

    String json = "{";
    json += "\"temperature\":" + String(d.sht31Ok ? d.temperature : -99, 1) + ",";
    json += "\"humidity\":" + String(d.sht31Ok ? d.humidity : -1, 1) + ",";
    json += "\"tvoc\":" + String(d.tvoc) + ",";
    json += "\"eco2\":" + String(d.eco2) + ",";
    json += "\"aqi\":" + String(d.aqi) + ",";
    json += "\"status\":\"" + d.aqiLabel + "\",";
    json += "\"sht31_ok\":" + String(d.sht31Ok ? "true" : "false") + ",";
    json += "\"ens160_ok\":" + String(d.ens160Ok ? "true" : "false") + ",";
    json += "\"warning\":" + String(d.warning ? "true" : "false") + ",";
    json += "\"warning_reason\":\"" + d.warningReason + "\",";
    json += "\"time\":" + String((uint32_t)now);
    json += "}";
    return json;
}

// ------------------------------------------------------------
void cloudSyncLoop(const SensorData& data) {
    if (!cloudSyncIsEnabled()) return;
    if (wifiManagerGetState() != WIFI_STATE_CONNECTED) return; // chỉ đẩy khi đã có mạng internet (STA), không đẩy lúc ở AP mode

    unsigned long now = millis();
    if (now - s_lastPushMs < CLOUD_SYNC_INTERVAL_MS) return;
    s_lastPushMs = now;

    String url = s_url;
    while (url.endsWith("/")) url.remove(url.length() - 1);
    url += "/ingest";

    HTTPClient http;
    bool began;
    WiFiClientSecure sclient; // phải sống hết vòng đời request (khai báo cùng scope)

    if (url.startsWith("https://")) {
        sclient.setInsecure(); // ESP32 không có sẵn kho chứng chỉ CA đầy đủ; relay do người dùng tự triển khai nên bỏ qua kiểm tra CA cho gọn nhẹ
        began = http.begin(sclient, url);
    } else {
        began = http.begin(url); // cho phép relay tự lưu trữ (self-hosted) qua http:// thuần nếu người dùng muốn
    }

    if (!began) {
        s_statusText = "Lỗi: không khởi tạo được kết nối tới relay";
        return;
    }

    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-Device-Token", s_token);
    http.setTimeout(CLOUD_SYNC_TIMEOUT_MS);

    int code = http.POST(buildPayload(data));

    struct tm timeinfo;
    char timeBuf[9] = "--:--:--";
    if (getLocalTime(&timeinfo, 50)) strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", &timeinfo);

    if (code == 200) {
        s_statusText = String("Đồng bộ thành công lúc ") + timeBuf;
    } else if (code > 0) {
        s_statusText = String("Lỗi đồng bộ (mã HTTP ") + code + ") lúc " + timeBuf;
        Serial.printf("[CLOUD] Đẩy dữ liệu thất bại, mã HTTP: %d\n", code);
    } else {
        s_statusText = String("Không kết nối được tới relay (mã ") + code + ") lúc " + timeBuf;
        Serial.printf("[CLOUD] Không kết nối được tới relay, mã lỗi: %d (%s)\n", code, http.errorToString(code).c_str());
    }

    http.end();
}
