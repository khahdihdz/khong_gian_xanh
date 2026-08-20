#include "web_server.h"
#include "config.h"
#include "wifi_manager.h"
#include "storage.h"
#include "sensor.h"
#include "ota_manager.h"

#include <LittleFS.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <ElegantOTA.h>
#include <time.h>

static AsyncWebServer server(80);
static AsyncWebSocket ws("/ws");
static unsigned long s_lastPushMs = 0;
static unsigned long s_lastCleanupMs = 0;
static volatile bool s_otaRebootPending = false;
static unsigned long s_otaSuccessMs = 0;

static String buildDataJson(const SensorData& data, const String& wifiStatus, const String& timeStr) {
    String json = "{";
    json += "\"temperature\":" + String(data.sht31Ok ? data.temperature : -99, 1) + ",\"humidity\":" + String(data.sht31Ok ? data.humidity : -1, 1) + ",";
    json += "\"tvoc\":" + String(data.ens160Ok ? data.tvoc : 0) + ",\"eco2\":" + String(data.ens160Ok ? data.eco2 : 0) + ",\"aqi\":" + String(data.ens160Ok ? data.aqi : 0) + ",";
    json += "\"status\":\"" + data.aqiLabel + "\",\"sht31_ok\":" + String(data.sht31Ok ? "true" : "false") + ",\"ens160_ok\":" + String(data.ens160Ok ? "true" : "false") + ",";
    json += "\"warning\":" + String(data.warning ? "true" : "false") + ",\"warning_reason\":\"" + data.warningReason + "\",\"wifi_status\":\"" + wifiStatus + "\",\"wifi_rssi\":" + String(wifiManagerGetRSSI()) + ",";
    json += "\"temp_offset\":" + String(sensorGetTemperatureOffset(), 2) + ",\"humidity_offset\":" + String(sensorGetHumidityOffset(), 2) + ",";
    time_t now; time(&now);
    json += "\"timestamp\":" + String((uint32_t)now) + ",\"time\":\"" + timeStr + "\"}";
    return json;
}

static String currentTimeString() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 100)) return "Chua dong bo";
    char buf[24]; strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo); return String(buf);
}

static void serveHtmlWithSharedChrome(AsyncWebServerRequest* request) {
    String path = request->url(); int query = path.indexOf('?'); if (query >= 0) path = path.substring(0, query);
    if (path == "/" || path.length() == 0) path = "/index.html";
    if (!path.endsWith(".html") || path.indexOf("..") >= 0) { request->send(404, "text/plain; charset=utf-8", "Không tìm thấy trang."); return; }
    File file = LittleFS.open(path, "r");
    if (!file) { request->send(404, "text/plain; charset=utf-8", "Không tìm thấy trang."); return; }
    String html = file.readString(); file.close();
    if (html.indexOf("/site-common.css") < 0) { int head = html.indexOf("</head>"); if (head >= 0) html = html.substring(0, head) + "<link rel=\"stylesheet\" href=\"/site-common.css\">\n" + html.substring(head); }
    if (html.indexOf("/site-common.js") < 0) { int bodyEnd = html.lastIndexOf("</body>"); const String script = "<script src=\"/site-common.js\"></script>\n"; if (bodyEnd >= 0) html = html.substring(0, bodyEnd) + script + html.substring(bodyEnd); else html += script; }
    AsyncWebServerResponse* response = request->beginResponse(200, "text/html; charset=utf-8", html); response->addHeader("Cache-Control", "no-store"); request->send(response);
}

static void onWsEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len) {
    if (type == WS_EVT_CONNECT) Serial.printf("[WS] Client #%u đã kết nối\n", client->id());
    else if (type == WS_EVT_DISCONNECT) Serial.printf("[WS] Client #%u đã ngắt kết nối\n", client->id());
    (void)server; (void)arg; (void)data; (void)len;
}

void webServerInit() {
    if (!LittleFS.begin(true)) Serial.println("[WEB] Lỗi mount LittleFS!");
    ws.onEvent(onWsEvent); server.addHandler(&ws);
    server.on("/", HTTP_GET, serveHtmlWithSharedChrome); server.on("/*.html", HTTP_GET, serveHtmlWithSharedChrome); server.serveStatic("/", LittleFS, "/");

    auto sendData = [](AsyncWebServerRequest* request) { request->send(200, "application/json", buildDataJson(g_sensorData, wifiManagerGetStateText(), currentTimeString())); };
    server.on("/api/data", HTTP_GET, sendData); server.on("/api/status", HTTP_GET, sendData);

    server.on("/api/calibration", HTTP_GET, [](AsyncWebServerRequest* request) {
        String json = "{\"ok\":true,\"temperature_offset\":" + String(sensorGetTemperatureOffset(), 2) + ",\"humidity_offset\":" + String(sensorGetHumidityOffset(), 2) + "}";
        request->send(200, "application/json", json);
    });
    server.on("/api/calibration", HTTP_POST, [](AsyncWebServerRequest* request) {
        if (!request->hasParam("temperature_offset", true) || !request->hasParam("humidity_offset", true)) { request->send(400, "application/json", "{\"ok\":false,\"message\":\"Thiếu thông số hiệu chỉnh.\"}"); return; }
        float t = request->getParam("temperature_offset", true)->value().toFloat();
        float h = request->getParam("humidity_offset", true)->value().toFloat();
        bool ok = sensorSetCalibration(t, h);
        request->send(ok ? 200 : 400, "application/json", ok ? "{\"ok\":true,\"message\":\"Đã lưu hiệu chỉnh nhiệt độ/độ ẩm.\"}" : "{\"ok\":false,\"message\":\"Giá trị hiệu chỉnh không hợp lệ.\"}");
    });
    server.on("/api/calibration/reset", HTTP_POST, [](AsyncWebServerRequest* request) { sensorResetCalibration(); request->send(200, "application/json", "{\"ok\":true,\"message\":\"Đã đưa hiệu chỉnh về 0.\"}"); });

    server.on("/api/history", HTTP_GET, [](AsyncWebServerRequest* request) { uint8_t hours = request->hasParam("hours") ? request->getParam("hours")->value().toInt() : 0; if (hours > 24) hours = 24; request->send(200, "application/json", storageGetHistoryJson(hours)); });
    server.on("/api/history/csv", HTTP_GET, [](AsyncWebServerRequest* request) { AsyncWebServerResponse* response = request->beginResponse(200, "text/csv", storageGetHistoryCsv()); response->addHeader("Content-Disposition", "attachment; filename=lich_su_moi_truong.csv"); request->send(response); });

    server.on("/api/info", HTTP_GET, [](AsyncWebServerRequest* request) {
        String json = "{"; json += "\"ok\":true,\"project\":\"" + String(PROJECT_NAME) + "\",\"chip\":\"ESP32\",\"free_heap\":" + String(ESP.getFreeHeap()) + ",\"uptime_ms\":" + String(millis()) + ",";
        json += "\"wifi_status\":\"" + wifiManagerGetStateText() + "\",\"ip\":\"" + wifiManagerGetIP() + "\",\"rssi\":" + String(wifiManagerGetRSSI()) + ",\"record_count\":" + String((int)storageGetRecordCount()) + ",\"firmware_version\":\"" + String(FIRMWARE_VERSION) + "\",";
        json += "\"history_limit\":" + String(HISTORY_MAX_RECORDS) + ",\"history_interval_ms\":" + String(HISTORY_LOG_INTERVAL_MS) + ",\"transport\":\"HTTP/WebSocket\",\"cloud_enabled\":false,\"cloud_status\":\"disabled\",\"auto_ota\":" + otaManagerGetInfoJson() + "}";
        request->send(200, "application/json", json);
    });

    server.on("/api/ota/check", HTTP_POST, [](AsyncWebServerRequest* request) { bool ok = otaManagerCheckNow(); request->send(ok ? 200 : 503, "application/json", otaManagerGetInfoJson()); });
    server.on("/api/ota/update", HTTP_POST, [](AsyncWebServerRequest* request) { bool ok = otaManagerUpdateNow(); request->send(ok ? 200 : 503, "application/json", otaManagerGetInfoJson()); });
    server.on("/api/ota/status", HTTP_GET, [](AsyncWebServerRequest* request) { request->send(200, "application/json", otaManagerGetInfoJson()); });

    server.on("/api/wifi-config", HTTP_POST, [](AsyncWebServerRequest* request) { String ssid, pass; if (request->hasParam("ssid", true)) ssid = request->getParam("ssid", true)->value(); if (request->hasParam("password", true)) pass = request->getParam("password", true)->value(); if (!ssid.length()) { request->send(400, "application/json", "{\"ok\":false,\"message\":\"Thiếu SSID\"}"); return; } bool ok = wifiManagerSaveCredentials(ssid, pass); request->send(ok ? 200 : 400, "application/json", ok ? "{\"ok\":true,\"message\":\"Đã lưu WiFi và bắt đầu kết nối.\"}" : "{\"ok\":false,\"message\":\"Không lưu được WiFi.\"}"); });
    server.on("/api/wifi-reset", HTTP_POST, [](AsyncWebServerRequest* request) { wifiManagerReset(); request->send(200, "application/json", "{\"ok\":true,\"message\":\"Đã xóa cấu hình WiFi và chuyển sang AP.\"}"); });
    server.onNotFound([](AsyncWebServerRequest* request) { request->send(404, "text/plain", "Không tìm thấy trang."); });

    ElegantOTA.onStart([]() { Serial.println("[OTA] Bắt đầu upload OTA..."); s_otaRebootPending = false; });
    ElegantOTA.onEnd([](bool success) { if (success) { Serial.println("[OTA] Upload thành công. Chờ response HTTP rồi reboot..."); s_otaSuccessMs = millis(); s_otaRebootPending = true; } else { Serial.println("[OTA] Upload thất bại."); s_otaRebootPending = false; } });
    ElegantOTA.setAutoReboot(false); ElegantOTA.begin(&server); server.begin(); Serial.println("[WEB] Web server đã khởi động. HTTP/WebSocket + OTA: /update");
}

void webServerLoop(const SensorData& data, const String& wifiStatus, const String& timeStr) {
    ElegantOTA.loop(); unsigned long now = millis();
    if (now - s_lastPushMs >= WEBSOCKET_PUSH_INTERVAL_MS) { s_lastPushMs = now; if (ws.count() > 0) ws.textAll(buildDataJson(data, wifiStatus, timeStr)); }
    if (now - s_lastCleanupMs >= 5000) { s_lastCleanupMs = now; ws.cleanupClients(); }
    if (s_otaRebootPending && now - s_otaSuccessMs >= 3000) { s_otaRebootPending = false; Serial.println("[OTA] Reboot sau khi response HTTP đã được gửi."); delay(50); ESP.restart(); }
}

void webServerCleanupClients() { ws.cleanupClients(); }
