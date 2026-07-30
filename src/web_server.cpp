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

// ------------------------------------------------------------
//  Tạo chuỗi JSON dữ liệu hiện tại (dùng chung cho API và WebSocket)
// ------------------------------------------------------------
static String buildDataJson(const SensorData& data, const String& wifiStatus, const String& timeStr) {
    String json = "{";
    json += "\"temperature\":" + String(data.dhtOk ? data.temperature : -99, 1) + ",";
    json += "\"humidity\":" + String(data.dhtOk ? data.humidity : -1, 1) + ",";
    json += "\"tvoc\":" + String(data.tvoc) + ",";
    json += "\"eco2\":" + String(data.eco2) + ",";
    json += "\"aqi\":" + String(data.aqi) + ",";
    json += "\"status\":\"" + data.aqiLabel + "\",";
    json += "\"dht_ok\":" + String(data.dhtOk ? "true" : "false") + ",";
    json += "\"ens160_ok\":" + String(data.ens160Ok ? "true" : "false") + ",";
    json += "\"warning\":" + String(data.warning ? "true" : "false") + ",";
    json += "\"warning_reason\":\"" + data.warningReason + "\",";
    json += "\"wifi_status\":\"" + wifiStatus + "\",";
    json += "\"time\":\"" + timeStr + "\"";
    json += "}";
    return json;
}

// ------------------------------------------------------------
//  Lấy chuỗi thời gian hiện tại "yyyy-MM-dd HH:mm:ss"
// ------------------------------------------------------------
static String currentTimeString() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 100)) {
        return "Chua dong bo";
    }
    char buf[24];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
    return String(buf);
}

// ------------------------------------------------------------
//  Xử lý sự kiện WebSocket (chỉ cần theo dõi kết nối, không cần nhận lệnh)
// ------------------------------------------------------------
static void onWsEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                       AwsEventType type, void* arg, uint8_t* data, size_t len) {
    if (type == WS_EVT_CONNECT) {
        Serial.printf("[WS] Client #%u đã kết nối\n", client->id());
    } else if (type == WS_EVT_DISCONNECT) {
        Serial.printf("[WS] Client #%u đã ngắt kết nối\n", client->id());
    }
}

// ------------------------------------------------------------
void webServerInit() {
    if (!LittleFS.begin(true)) {
        Serial.println("[WEB] Lỗi mount LittleFS!");
    }

    ws.onEvent(onWsEvent);
    server.addHandler(&ws);

    // ---------- Trang tĩnh (dashboard, cấu hình wifi) ----------
    server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

    // ---------- API: dữ liệu hiện tại ----------
    server.on("/api/data", HTTP_GET, [](AsyncWebServerRequest* request) {
        extern SensorData g_sensorData; // dữ liệu mới nhất từ sensor.cpp
        String json = buildDataJson(g_sensorData, wifiManagerGetStateText(), currentTimeString());
        request->send(200, "application/json", json);
    });

    // ---------- API: lịch sử dữ liệu (?hours=1|6|12|24, 0 = tất cả) ----------
    server.on("/api/history", HTTP_GET, [](AsyncWebServerRequest* request) {
        uint8_t hours = 0;
        if (request->hasParam("hours")) {
            hours = request->getParam("hours")->value().toInt();
        }
        String json = storageGetHistoryJson(hours);
        request->send(200, "application/json", json);
    });

    // ---------- API: tải xuống lịch sử CSV ----------
    server.on("/api/history/csv", HTTP_GET, [](AsyncWebServerRequest* request) {
        String csv = storageGetHistoryCsv();
        AsyncWebServerResponse* response = request->beginResponse(200, "text/csv", csv);
        response->addHeader("Content-Disposition", "attachment; filename=lich_su_moi_truong.csv");
        request->send(response);
    });

    // ---------- API: thông tin thiết bị ----------
    server.on("/api/info", HTTP_GET, [](AsyncWebServerRequest* request) {
        String json = "{";
        json += "\"project\":\"" + String(PROJECT_NAME) + "\",";
        json += "\"chip\":\"ESP32\",";
        json += "\"free_heap\":" + String(ESP.getFreeHeap()) + ",";
        json += "\"uptime_ms\":" + String(millis()) + ",";
        json += "\"wifi_status\":\"" + wifiManagerGetStateText() + "\",";
        json += "\"ip\":\"" + wifiManagerGetIP() + "\",";
        json += "\"rssi\":" + String(wifiManagerGetRSSI()) + ",";
        json += "\"record_count\":" + String((int)storageGetRecordCount()) + ",";
        json += "\"firmware_version\":\"" + String(FIRMWARE_VERSION) + "\"";
        json += "}";
        request->send(200, "application/json", json);
    });

    // ---------- API: cấu hình WiFi mới (POST form: ssid, password) ----------
    server.on("/api/wifi-config", HTTP_POST, [](AsyncWebServerRequest* request) {
        String ssid, pass;
        if (request->hasParam("ssid", true)) ssid = request->getParam("ssid", true)->value();
        if (request->hasParam("password", true)) pass = request->getParam("password", true)->value();

        if (ssid.length() == 0) {
            request->send(400, "application/json", "{\"ok\":false,\"message\":\"Thiếu SSID\"}");
            return;
        }
        wifiManagerSaveCredentials(ssid, pass);
        request->send(200, "application/json", "{\"ok\":true,\"message\":\"Đã lưu, đang thử kết nối...\"}");
    });

    // ---------- API: reset cấu hình WiFi ----------
    server.on("/api/wifi-reset", HTTP_POST, [](AsyncWebServerRequest* request) {
        wifiManagerReset();
        request->send(200, "application/json", "{\"ok\":true}");
    });

    // ---------- 404 ----------
    server.onNotFound([](AsyncWebServerRequest* request) {
        request->send(404, "text/plain", "Khong tim thay trang.");
    });

    // ---------- OTA (cập nhật firmware qua trang web tại /update) ----------
    ElegantOTA.begin(&server);

    server.begin();
    Serial.println("[WEB] Web server đã khởi động.");
}

// ------------------------------------------------------------
void webServerLoop(const SensorData& data, const String& wifiStatus, const String& timeStr) {
    ElegantOTA.loop();

    unsigned long now = millis();
    if (now - s_lastPushMs >= WEBSOCKET_PUSH_INTERVAL_MS) {
        s_lastPushMs = now;
        if (ws.count() > 0) {
            String json = buildDataJson(data, wifiStatus, timeStr);
            ws.textAll(json);
        }
    }

    // Dọn dẹp client rớt kết nối định kỳ (khuyến nghị của ESPAsyncWebServer)
    if (now - s_lastCleanupMs >= 5000) {
        s_lastCleanupMs = now;
        ws.cleanupClients();
    }
}

void webServerCleanupClients() {
    ws.cleanupClients();
}
