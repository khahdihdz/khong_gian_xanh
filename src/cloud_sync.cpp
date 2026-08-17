#include "cloud_sync.h"
#include "config.h"
#include "wifi_manager.h"

#include <Preferences.h>
#include <WebSocketsClient.h>
#include <WiFi.h>

static Preferences s_prefs;
static WebSocketsClient s_ws;
static String s_url;
static String s_token;
static bool s_enabled = false;
static bool s_connected = false;
static bool s_wsStarted = false;
static unsigned long s_lastPushMs = 0;
static String s_statusText = "Chưa cấu hình";
static String s_deviceId;

static String deviceId() {
    uint64_t mac = ESP.getEfuseMac();
    char buf[17];
    snprintf(buf, sizeof(buf), "%04X%08X", (uint16_t)(mac >> 32), (uint32_t)mac);
    return String(buf);
}

static bool parseWsUrl(const String& url, String& host, uint16_t& port, String& path, bool& secure) {
    String u = url;
    u.trim();
    secure = u.startsWith("wss://");
    const String prefix = secure ? "wss://" : "ws://";
    if (!u.startsWith(prefix)) return false;
    u.remove(0, prefix.length());

    int slash = u.indexOf('/');
    String authority = slash >= 0 ? u.substring(0, slash) : u;
    path = slash >= 0 ? u.substring(slash) : "/";

    int colon = authority.lastIndexOf(':');
    if (colon > 0 && authority.indexOf(']') < 0) {
        host = authority.substring(0, colon);
        port = (uint16_t)authority.substring(colon + 1).toInt();
    } else {
        host = authority;
        port = secure ? 443 : 80;
    }
    return host.length() > 0;
}

static void cloudWsEvent(WStype_t type, uint8_t* payload, size_t length) {
    (void)payload;
    (void)length;
    switch (type) {
        case WStype_CONNECTED:
            s_connected = true;
            s_statusText = "Đã kết nối Cloud WebSocket";
            Serial.println("[CLOUD] WebSocket Cloud đã kết nối");
            break;
        case WStype_DISCONNECTED:
            s_connected = false;
            s_statusText = "Cloud WebSocket đang kết nối lại...";
            Serial.println("[CLOUD] WebSocket Cloud mất kết nối");
            break;
        case WStype_ERROR:
            s_connected = false;
            s_statusText = "Lỗi Cloud WebSocket";
            Serial.println("[CLOUD] Lỗi WebSocket");
            break;
        default:
            break;
    }
}

static void startWebSocket() {
    if (!s_enabled || s_url.length() == 0 || s_wsStarted) return;

    String host, path;
    uint16_t port = 443;
    bool secure = true;
    if (!parseWsUrl(s_url, host, port, path, secure)) {
        s_statusText = "URL WebSocket không hợp lệ";
        return;
    }

    path += (path.indexOf('?') >= 0 ? "&" : "?");
    path += "device=" + s_deviceId;

    s_ws.onEvent(cloudWsEvent);
    s_ws.setReconnectInterval(CLOUD_WS_RECONNECT_MS);
    s_ws.enableHeartbeat(15000, 3000, 2);

    String headers = "X-Device-Token: " + s_token + "\r\n";
    s_ws.setExtraHeaders(headers.c_str());

    if (secure) s_ws.beginSSL(host.c_str(), port, path.c_str());
    else s_ws.begin(host.c_str(), port, path.c_str());

    s_wsStarted = true;
    Serial.println("[CLOUD] Kết nối WSS tới: " + s_url);
}

void cloudSyncInit() {
    s_deviceId = deviceId();
    s_prefs.begin(PREF_NAMESPACE, true);
    s_url = s_prefs.getString(PREF_KEY_CLOUD_URL, "");
    s_token = s_prefs.getString(PREF_KEY_CLOUD_TOKEN, "");
    s_enabled = s_prefs.getBool(PREF_KEY_CLOUD_ON, false);
    s_prefs.end();

    s_lastPushMs = millis() - CLOUD_SYNC_INTERVAL_MS;
    s_wsStarted = false;
    s_connected = false;

    Serial.println("[CLOUD] Device ID: " + s_deviceId);
    if (s_enabled && s_url.length()) startWebSocket();
    else Serial.println("[CLOUD] Cloud WebSocket đang TẮT.");
}

void cloudSyncSaveConfig(const String& url, const String& token, bool enabled) {
    s_url = url;
    s_token = token;
    s_enabled = enabled;

    s_ws.disconnect();
    s_wsStarted = false;
    s_connected = false;

    s_prefs.begin(PREF_NAMESPACE, false);
    s_prefs.putString(PREF_KEY_CLOUD_URL, s_url);
    s_prefs.putString(PREF_KEY_CLOUD_TOKEN, s_token);
    s_prefs.putBool(PREF_KEY_CLOUD_ON, s_enabled);
    s_prefs.end();

    s_statusText = enabled ? "Đã lưu, đang kết nối Cloud WebSocket..." : "Đã tắt Cloud WebSocket";
    s_lastPushMs = millis() - CLOUD_SYNC_INTERVAL_MS;
    if (enabled) startWebSocket();
}

bool cloudSyncIsEnabled()     { return s_enabled && s_url.length() > 0; }
String cloudSyncGetUrl()      { return s_url; }
String cloudSyncGetToken()    { return s_token; }
bool cloudSyncHasToken()      { return s_token.length() > 0; }
String cloudSyncGetStatusText(){ return s_statusText; }
String cloudSyncGetDeviceId() { return s_deviceId; }

static String buildPayload(const SensorData& d) {
    time_t now;
    time(&now);
    String json = "{";
    json += "\"type\":\"sensor\",";
    json += "\"device\":\"" + s_deviceId + "\",";
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

void cloudSyncLoop(const SensorData& data) {
    if (!cloudSyncIsEnabled()) return;
    if (wifiManagerGetState() != WIFI_STATE_CONNECTED) return;

    if (!s_wsStarted) startWebSocket();
    s_ws.loop();

    unsigned long now = millis();
    if (!s_connected || now - s_lastPushMs < CLOUD_SYNC_INTERVAL_MS) return;
    s_lastPushMs = now;

    String payload = buildPayload(data);
    if (s_ws.sendTXT(payload)) {
        s_statusText = "Đã đồng bộ Cloud WebSocket";
    } else {
        s_statusText = "Không gửi được dữ liệu Cloud";
    }
}
