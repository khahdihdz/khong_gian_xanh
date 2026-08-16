#include "web_server.h"
#include "config.h"
#include "wifi_manager.h"
#include "storage.h"
#include "cloud_sync.h"
#include "mqtt_client.h"

#include <LittleFS.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <ElegantOTA.h>
#include <Preferences.h>
#include <time.h>

static AsyncWebServer server(80);
static AsyncWebSocket ws("/ws");
static unsigned long s_lastPushMs = 0;
static unsigned long s_lastCleanupMs = 0;

static String buildDataJson(const SensorData& data, const String& wifiStatus, const String& timeStr) {
    String json = "{";
    json += "\"temperature\":" + String(data.sht31Ok ? data.temperature : -99, 1) + ",";
    json += "\"humidity\":" + String(data.sht31Ok ? data.humidity : -1, 1) + ",";
    json += "\"tvoc\":" + String(data.tvoc) + ",\"eco2\":" + String(data.eco2) + ",\"aqi\":" + String(data.aqi) + ",";
    json += "\"status\":\"" + data.aqiLabel + "\",\"sht31_ok\":" + String(data.sht31Ok ? "true" : "false") + ",";
    json += "\"ens160_ok\":" + String(data.ens160Ok ? "true" : "false") + ",\"warning\":" + String(data.warning ? "true" : "false") + ",";
    json += "\"warning_reason\":\"" + data.warningReason + "\",\"wifi_status\":\"" + wifiStatus + "\",\"time\":\"" + timeStr + "\"}";
    return json;
}

static String currentTimeString() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 100)) return "Chua dong bo";
    char buf[24]; strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo); return String(buf);
}

static void onWsEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len) {
    if (type == WS_EVT_CONNECT) Serial.printf("[WS] Client #%u đã kết nối\n", client->id());
    else if (type == WS_EVT_DISCONNECT) Serial.printf("[WS] Client #%u đã ngắt kết nối\n", client->id());
}

void webServerInit() {
    if (!LittleFS.begin(true)) Serial.println("[WEB] Lỗi mount LittleFS!");
    ws.onEvent(onWsEvent); server.addHandler(&ws); server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

    server.on("/api/data", HTTP_GET, [](AsyncWebServerRequest* request) {
        extern SensorData g_sensorData; request->send(200, "application/json", buildDataJson(g_sensorData, wifiManagerGetStateText(), currentTimeString()));
    });
    server.on("/api/history", HTTP_GET, [](AsyncWebServerRequest* request) {
        uint8_t hours = request->hasParam("hours") ? request->getParam("hours")->value().toInt() : 0; request->send(200, "application/json", storageGetHistoryJson(hours));
    });
    server.on("/api/history/csv", HTTP_GET, [](AsyncWebServerRequest* request) {
        AsyncWebServerResponse* response = request->beginResponse(200, "text/csv", storageGetHistoryCsv()); response->addHeader("Content-Disposition", "attachment; filename=lich_su_moi_truong.csv"); request->send(response);
    });
    server.on("/api/info", HTTP_GET, [](AsyncWebServerRequest* request) {
        String json = "{";
        json += "\"project\":\"" + String(PROJECT_NAME) + "\",\"chip\":\"ESP32\",\"free_heap\":" + String(ESP.getFreeHeap()) + ",\"uptime_ms\":" + String(millis()) + ",";
        json += "\"wifi_status\":\"" + wifiManagerGetStateText() + "\",\"ip\":\"" + wifiManagerGetIP() + "\",\"rssi\":" + String(wifiManagerGetRSSI()) + ",";
        json += "\"record_count\":" + String((int)storageGetRecordCount()) + ",\"firmware_version\":\"" + String(FIRMWARE_VERSION) + "\",\"cloud_enabled\":" + String(cloudSyncIsEnabled() ? "true" : "false") + ",";
        json += "\"cloud_status\":\"" + cloudSyncGetStatusText() + "\",\"mqtt_connected\":" + String(mqttClientIsConnected() ? "true" : "false") + ",\"mqtt_device_id\":\"" + mqttClientGetDeviceId() + "\",\"mqtt_status\":\"" + mqttClientGetStatusText() + "\"}";
        request->send(200, "application/json", json);
    });

    server.on("/api/wifi-config", HTTP_POST, [](AsyncWebServerRequest* request) {
        String ssid, pass; if (request->hasParam("ssid", true)) ssid = request->getParam("ssid", true)->value(); if (request->hasParam("password", true)) pass = request->getParam("password", true)->value();
        if (!ssid.length()) { request->send(400, "application/json", "{\"ok\":false,\"message\":\"Thiếu SSID\"}"); return; }
        wifiManagerSaveCredentials(ssid, pass); request->send(200, "application/json", "{\"ok\":true,\"message\":\"Đã lưu, đang thử kết nối...\"}");
    });
    server.on("/api/wifi-reset", HTTP_POST, [](AsyncWebServerRequest* request) { wifiManagerReset(); request->send(200, "application/json", "{\"ok\":true}"); });

    server.on("/api/cloud-config", HTTP_GET, [](AsyncWebServerRequest* request) {
        String json = "{\"enabled\":" + String(cloudSyncIsEnabled() ? "true" : "false") + ",\"url\":\"" + cloudSyncGetUrl() + "\",\"has_token\":" + String(cloudSyncHasToken() ? "true" : "false") + ",\"status\":\"" + cloudSyncGetStatusText() + "\"}";
        request->send(200, "application/json", json);
    });
    server.on("/api/cloud-config", HTTP_POST, [](AsyncWebServerRequest* request) {
        String url, token; bool enabled = false; if (request->hasParam("url", true)) url = request->getParam("url", true)->value(); if (request->hasParam("token", true)) token = request->getParam("token", true)->value(); if (request->hasParam("enabled", true)) enabled = request->getParam("enabled", true)->value() == "1";
        if (!token.length()) token = cloudSyncGetToken(); if (enabled && !url.length()) { request->send(400, "application/json", "{\"ok\":false,\"message\":\"Thiếu URL relay\"}"); return; }
        cloudSyncSaveConfig(url, token, enabled); request->send(200, "application/json", "{\"ok\":true,\"message\":\"Đã lưu cấu hình đồng bộ cloud\"}");
    });

    server.on("/api/mqtt-config", HTTP_GET, [](AsyncWebServerRequest* request) {
        Preferences prefs; prefs.begin(PREF_NAMESPACE, true); String host = prefs.getString(PREF_KEY_MQTT_HOST, ""); uint16_t port = prefs.getUShort(PREF_KEY_MQTT_PORT, 1883); String user = prefs.getString(PREF_KEY_MQTT_USER, ""); bool enabled = prefs.getBool(PREF_KEY_MQTT_ON, false); bool hasPassword = prefs.getString(PREF_KEY_MQTT_PASS, "").length() > 0; prefs.end();
        String json = "{\"enabled\":" + String(enabled ? "true" : "false") + ",\"host\":\"" + host + "\",\"port\":" + String(port) + ",\"username\":\"" + user + "\",\"has_password\":" + String(hasPassword ? "true" : "false") + ",\"connected\":" + String(mqttClientIsConnected() ? "true" : "false") + ",\"device_id\":\"" + mqttClientGetDeviceId() + "\",\"status\":\"" + mqttClientGetStatusText() + "\"}";
        request->send(200, "application/json", json);
    });
    server.on("/api/mqtt-config", HTTP_POST, [](AsyncWebServerRequest* request) {
        String host, user, pass; uint16_t port = 1883; bool enabled = false;
        if (request->hasParam("host", true)) host = request->getParam("host", true)->value(); if (request->hasParam("port", true)) port = request->getParam("port", true)->value().toInt(); if (request->hasParam("username", true)) user = request->getParam("username", true)->value(); if (request->hasParam("password", true)) pass = request->getParam("password", true)->value(); if (request->hasParam("enabled", true)) enabled = request->getParam("enabled", true)->value() == "1";
        if (!host.length()) { request->send(400, "application/json", "{\"ok\":false,\"message\":\"Thiếu MQTT Broker\"}"); return; }
        if (!port) { request->send(400, "application/json", "{\"ok\":false,\"message\":\"Port MQTT không hợp lệ\"}"); return; }
        Preferences prefs; prefs.begin(PREF_NAMESPACE, false); prefs.putString(PREF_KEY_MQTT_HOST, host); prefs.putUShort(PREF_KEY_MQTT_PORT, port); prefs.putString(PREF_KEY_MQTT_USER, user); if (pass.length()) prefs.putString(PREF_KEY_MQTT_PASS, pass); prefs.putBool(PREF_KEY_MQTT_ON, enabled); prefs.end();
        bool connected = mqttClientReloadConfig();
        String message = connected ? "Đã lưu và kết nối MQTT thành công." : (enabled ? "Đã lưu. MQTT đang thử kết nối, kiểm tra trạng thái bên dưới." : "Đã lưu và tắt MQTT.");
        String json = "{\"ok\":true,\"connected\":" + String(connected ? "true" : "false") + ",\"message\":\"" + message + "\"}";
        request->send(200, "application/json", json);
    });

    server.onNotFound([](AsyncWebServerRequest* request) { request->send(404, "text/plain", "Khong tim thay trang."); });
    ElegantOTA.begin(&server); server.begin(); Serial.println("[WEB] Web server đã khởi động.");
}

void webServerLoop(const SensorData& data, const String& wifiStatus, const String& timeStr) {
    ElegantOTA.loop(); unsigned long now = millis();
    if (now - s_lastPushMs >= WEBSOCKET_PUSH_INTERVAL_MS) { s_lastPushMs = now; if (ws.count() > 0) ws.textAll(buildDataJson(data, wifiStatus, timeStr)); }
    if (now - s_lastCleanupMs >= 5000) { s_lastCleanupMs = now; ws.cleanupClients(); }
}
void webServerCleanupClients() { ws.cleanupClients(); }
