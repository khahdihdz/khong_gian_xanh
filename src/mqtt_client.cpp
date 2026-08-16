#include "mqtt_client.h"
#include "config.h"
#include "wifi_manager.h"

#include <WiFi.h>
#include <PubSubClient.h>
#include <Preferences.h>
#include <time.h>

static WiFiClient s_wifiClient;
static PubSubClient s_mqtt(s_wifiClient);
static Preferences s_prefs;
static String s_host;
static uint16_t s_port = 1883;
static String s_user;
static String s_pass;
static bool s_enabled = false;
static unsigned long s_lastReconnectMs = 0;
static unsigned long s_lastPublishMs = 0;
static String s_statusText = "Chưa cấu hình MQTT";
static String s_deviceId;

static String topicBase() {
    return String("khonggianxanh/") + s_deviceId;
}

static String makeTelemetry(const SensorData& d) {
    time_t now;
    time(&now);
    String json = "{";
    json += "\"device_id\":\"" + s_deviceId + "\",";
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
    json += "\"timestamp\":" + String((uint32_t)now);
    json += "}";
    return json;
}

static void mqttCallback(char* topic, byte* payload, unsigned int length) {
    String command;
    for (unsigned int i = 0; i < length; ++i) command += (char)payload[i];
    Serial.printf("[MQTT] Lệnh %s: %s\n", topic, command.c_str());

    String response = "{\"device_id\":\"" + s_deviceId + "\",\"command\":\"" + command + "\",\"status\":\"received\"}";
    s_mqtt.publish((topicBase() + "/response").c_str(), response.c_str(), false);
}

void mqttClientInit() {
    s_deviceId = String("esp32-") + String((uint32_t)(ESP.getEfuseMac() >> 24), HEX) + String((uint32_t)ESP.getEfuseMac(), HEX);
    s_deviceId.toLowerCase();

    s_prefs.begin(PREF_NAMESPACE, true);
    s_host = s_prefs.getString(PREF_KEY_MQTT_HOST, "");
    s_port = s_prefs.getUShort(PREF_KEY_MQTT_PORT, 1883);
    s_user = s_prefs.getString(PREF_KEY_MQTT_USER, "");
    s_pass = s_prefs.getString(PREF_KEY_MQTT_PASS, "");
    s_enabled = s_prefs.getBool(PREF_KEY_MQTT_ON, false);
    s_prefs.end();

    s_mqtt.setServer(s_host.c_str(), s_port);
    s_mqtt.setCallback(mqttCallback);
    s_mqtt.setBufferSize(1024);

    if (s_enabled && s_host.length()) {
        s_statusText = "MQTT đã cấu hình, chờ kết nối";
    } else {
        s_statusText = "MQTT đang tắt hoặc chưa cấu hình";
    }
}

void mqttClientLoop(const SensorData& data) {
    if (!s_enabled || !s_host.length()) return;
    if (wifiManagerGetState() != WIFI_STATE_CONNECTED) return;

    if (!s_mqtt.connected()) {
        unsigned long now = millis();
        if (now - s_lastReconnectMs < MQTT_RECONNECT_INTERVAL_MS) return;
        s_lastReconnectMs = now;

        String willTopic = topicBase() + "/status";
        String clientId = s_deviceId;
        bool connected;
        if (s_user.length()) {
            connected = s_mqtt.connect(clientId.c_str(), s_user.c_str(), s_pass.c_str(), willTopic.c_str(), 1, true, "offline");
        } else {
            connected = s_mqtt.connect(clientId.c_str(), willTopic.c_str(), 1, true, "offline");
        }

        if (!connected) {
            s_statusText = "MQTT lỗi kết nối: " + String(s_mqtt.state());
            return;
        }

        s_mqtt.publish(willTopic.c_str(), "online", true);
        s_mqtt.subscribe((topicBase() + "/command").c_str(), 1);
        s_mqtt.subscribe((topicBase() + "/config").c_str(), 1);
        s_statusText = "MQTT đã kết nối";
    }

    s_mqtt.loop();

    unsigned long now = millis();
    if (now - s_lastPublishMs >= MQTT_PUBLISH_INTERVAL_MS) {
        s_lastPublishMs = now;
        String payload = makeTelemetry(data);
        if (s_mqtt.publish((topicBase() + "/telemetry").c_str(), payload.c_str(), false)) {
            s_statusText = "MQTT: đã gửi dữ liệu";
        }
    }
}

bool mqttClientIsConnected() { return s_mqtt.connected(); }
String mqttClientGetStatusText() { return s_statusText; }
String mqttClientGetDeviceId() { return s_deviceId; }
