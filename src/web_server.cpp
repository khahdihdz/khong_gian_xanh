#include "web_server.h"
#include "config.h"
#include "wifi_manager.h"
#include "storage.h"

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
    time_t now;
    time(&now);
    json += "\"timestamp\":" + String((uint32_t)now) + ",\"time\":\"" + timeStr + "\"}";
    return json;
}

static String currentTimeString() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 100)) return "Chua dong bo";
    char buf[24];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
    return String(buf);
}

static void onWsEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len) {
    if (type == WS_EVT_CONNECT) Serial.printf("[WS] Client #%u đã kết nối\n", client->id());
    else if (type == WS_EVT_DISCONNECT) Serial.printf("[WS] Client #%u đã ngắt kết nối\n", client->id());
    (void)server; (void)arg; (void)data; (void)len;
}

void webServerInit() {
    if (!LittleFS.begin(true)) Serial.println("[WEB] Lỗi mount LittleFS!");

    ws.onEvent(onWsEvent);
    server.addHandler(&ws);
    server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

    auto sendData = [](AsyncWebServerRequest* request) {
        extern SensorData g_sensorData;
        request->send(200, "application/json", buildDataJson(g_sensorData, wifiManagerGetStateText(), currentTimeString()));
    };

    server.on("/api/data", HTTP_GET, sendData);
    // Alias tương thích với các dashboard/web client cũ.
    server.on("/api/status", HTTP_GET, sendData);

    server.on("/api/history", HTTP_GET, [](AsyncWebServerRequest* request) {
        uint8_t hours = request->hasParam("hours") ? request->getParam("hours")->value().toInt() : 0;
        if (hours > 24) hours = 24;
        request->send(200, "application/json", storageGetHistoryJson(hours));
    });

    server.on("/api/history/csv", HTTP_GET, [](AsyncWebServerRequest* request) {
        AsyncWebServerResponse* response = request->beginResponse(200, "text/csv", storageGetHistoryCsv());
        response->addHeader("Content-Disposition", "attachment; filename=lich_su_moi_truong.csv");
        request->send(response);
    });

    server.on("/api/info", HTTP_GET, [](AsyncWebServerRequest* request) {
        String json = "{";
        json += "\"ok\":true,\"project\":\"" + String(PROJECT_NAME) + "\",\"chip\":\"ESP32\",\"free_heap\":" + String(ESP.getFreeHeap()) + ",\"uptime_ms\":" + String(millis()) + ",";
        json += "\"wifi_status\":\"" + wifiManagerGetStateText() + "\",\"ip\":\"" + wifiManagerGetIP() + "\",\"rssi\":" + String(wifiManagerGetRSSI()) + ",";
        json += "\"record_count\":" + String((int)storageGetRecordCount()) + ",\"firmware_version\":\"" + String(FIRMWARE_VERSION) + "\",";
        json += "\"history_limit\":" + String(HISTORY_MAX_RECORDS) + ",\"history_interval_ms\":" + String(HISTORY_LOG_INTERVAL_MS) + ",";
        json += "\"transport\":\"HTTP/WebSocket\",\"cloud_enabled\":false,\"cloud_status\":\"disabled\"}";
        request->send(200, "application/json", json);
    });

    server.on("/api/wifi-config", HTTP_POST, [](AsyncWebServerRequest* request) {
        String ssid, pass;
        if (request->hasParam("ssid", true)) ssid = request->getParam("ssid", true)->value();
        if (request->hasParam("password", true)) pass = request->getParam("password", true)->value();
        if (!ssid.length()) {
            request->send(400, "application/json", "{\"ok\":false,\"message\":\"Thiếu SSID\"}");
            return;
        }
        bool ok = wifiManagerSaveCredentials(ssid, pass);
        request->send(ok ? 200 : 400, "application/json", ok ? "{\"ok\":true,\"message\":\"Đã lưu WiFi và bắt đầu kết nối.\"}" : "{\"ok\":false,\"message\":\"Không lưu được WiFi.\"}");
    });

    server.on("/api/wifi-reset", HTTP_POST, [](AsyncWebServerRequest* request) {
        wifiManagerReset();
        request->send(200, "application/json", "{\"ok\":true,\"message\":\"Đã xóa cấu hình WiFi và chuyển sang AP.\"}");
    });

    server.onNotFound([](AsyncWebServerRequest* request) {
        request->send(404, "text/plain", "Khong tim thay trang.");
    });

    ElegantOTA.onStart([]() {
        Serial.println("[OTA] Bắt đầu upload OTA...");
        s_otaRebootPending = false;
    });

    ElegantOTA.onEnd([](bool success) {
        if (success) {
            Serial.println("[OTA] Upload thành công. Chờ response HTTP rồi reboot...");
            s_otaSuccessMs = millis();
            s_otaRebootPending = true;
        } else {
            Serial.println("[OTA] Upload thất bại.");
            s_otaRebootPending = false;
        }
    });

    ElegantOTA.setAutoReboot(false);
    ElegantOTA.begin(&server);
    server.begin();
    Serial.println("[WEB] Web server đã khởi động. HTTP/WebSocket + OTA: /update");
}

void webServerLoop(const SensorData& data, const String& wifiStatus, const String& timeStr) {
    ElegantOTA.loop();
    unsigned long now = millis();

    if (now - s_lastPushMs >= WEBSOCKET_PUSH_INTERVAL_MS) {
        s_lastPushMs = now;
        if (ws.count() > 0) ws.textAll(buildDataJson(data, wifiStatus, timeStr));
    }

    if (now - s_lastCleanupMs >= 5000) {
        s_lastCleanupMs = now;
        ws.cleanupClients();
    }

    if (s_otaRebootPending && now - s_otaSuccessMs >= 3000) {
        s_otaRebootPending = false;
        Serial.println("[OTA] Reboot sau khi response HTTP đã được gửi.");
        delay(50);
        ESP.restart();
    }
}

void webServerCleanupClients() {
    ws.cleanupClients();
}
