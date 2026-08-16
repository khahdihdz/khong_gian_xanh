#include "mqtt_client.h"
#include "config.h"
#include "wifi_manager.h"

#include <WiFi.h>
#include <espMqttClient.h>
#include <Preferences.h>
#include <time.h>

// MQTT secure client: TLS được xác thực bằng CA do người dùng cấu hình.
static espMqttClientSecure s_mqtt;
static Preferences s_prefs;
static String s_host;
static uint16_t s_port = 8883;
static String s_user;
static String s_pass;
static String s_ca;
static bool s_enabled = false;
static bool s_tls = true;
static unsigned long s_lastReconnectMs = 0;
static unsigned long s_lastPublishMs = 0;
static String s_statusText = "Chưa cấu hình MQTT";
static String s_deviceId;

static String topicBase() { return String("khonggianxanh/") + s_deviceId; }

static void loadConfig() {
    s_prefs.begin(PREF_NAMESPACE, true);
    s_host = s_prefs.getString(PREF_KEY_MQTT_HOST, "");
    s_port = s_prefs.getUShort(PREF_KEY_MQTT_PORT, 8883);
    s_user = s_prefs.getString(PREF_KEY_MQTT_USER, "");
    s_pass = s_prefs.getString(PREF_KEY_MQTT_PASS, "");
    s_ca = s_prefs.getString(PREF_KEY_MQTT_CA, "");
    s_enabled = s_prefs.getBool(PREF_KEY_MQTT_ON, false);
    s_tls = s_prefs.getBool(PREF_KEY_MQTT_TLS, true);
    s_prefs.end();
}

static String makeTelemetry(const SensorData& d) {
    time_t now; time(&now);
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
    json += "\"timestamp\":" + String((uint32_t)now) + "}";
    return json;
}

static void onConnect(bool sessionPresent) {
    String base = topicBase();
    s_mqtt.publish((base + "/status").c_str(), 1, true, "online");
    s_mqtt.subscribe((base + "/command").c_str(), 1);
    s_mqtt.subscribe((base + "/config").c_str(), 1);
    s_statusText = sessionPresent ? "MQTT TLS đã kết nối (session cũ)" : "MQTT TLS đã kết nối";
}

static void onDisconnect(espMqttClientTypes::DisconnectReason reason) {
    s_statusText = "MQTT ngắt kết nối: " + String((int)reason);
}

static void onMessage(const espMqttClientTypes::MessageProperties& properties, const char* topic,
                     const uint8_t* payload, size_t len, size_t index, size_t total) {
    if (index != 0 || len != total) return;
    String command;
    command.reserve(len);
    for (size_t i = 0; i < len; ++i) command += (char)payload[i];
    Serial.printf("[MQTT] Lệnh %s: %s\n", topic, command.c_str());
    String response = "{\"device_id\":\"" + s_deviceId + "\",\"command\":\"" + command + "\",\"status\":\"received\"}";
    s_mqtt.publish((topicBase() + "/response").c_str(), 1, false, response.c_str());
    (void)properties;
}

static bool configureClient() {
    if (!s_host.length()) return false;

    s_mqtt.setClientId(s_deviceId.c_str());
    s_mqtt.setCleanSession(false);
    s_mqtt.setKeepAlive(30);
    s_mqtt.setTimeout(10);
    s_mqtt.setServer(s_host.c_str(), s_port);
    s_mqtt.setCredentials(s_user.c_str(), s_pass.c_str());

    String willTopic = topicBase() + "/status";
    s_mqtt.setWill(willTopic.c_str(), 1, true, "offline");

    if (s_tls) {
        if (!s_ca.length()) {
            s_statusText = "MQTT TLS chưa có CA certificate";
            return false;
        }
        s_mqtt.setCACert(s_ca.c_str());
    }

    s_mqtt.onConnect(onConnect);
    s_mqtt.onDisconnect(onDisconnect);
    s_mqtt.onMessage(onMessage);
    return true;
}

static bool connectMqtt() {
    if (!s_enabled || !s_host.length()) return false;
    if (WiFi.status() != WL_CONNECTED) return false;
    if (!configureClient()) return false;
    bool ok = s_mqtt.connect();
    if (!ok) s_statusText = "MQTT không thể bắt đầu kết nối";
    else s_statusText = "MQTT đang kết nối...";
    return ok;
}

void mqttClientInit() {
    uint64_t mac = ESP.getEfuseMac();
    s_deviceId = String("esp32-") + String((uint32_t)(mac >> 24), HEX) + String((uint32_t)mac, HEX);
    s_deviceId.toLowerCase();
    loadConfig();
    s_statusText = (s_enabled && s_host.length()) ? "MQTT đã cấu hình, chờ kết nối" : "MQTT đang tắt hoặc chưa cấu hình";
    if (s_enabled && s_host.length()) connectMqtt();
}

bool mqttClientReloadConfig() {
    if (s_mqtt.connected()) {
        s_mqtt.publish((topicBase() + "/status").c_str(), 1, true, "offline");
        s_mqtt.disconnect(true);
    }
    loadConfig();
    s_lastReconnectMs = 0;
    s_lastPublishMs = millis();
    if (!s_enabled || !s_host.length()) {
        s_statusText = "MQTT đang tắt hoặc chưa cấu hình";
        return true;
    }
    return connectMqtt();
}

bool mqttClientReconnectNow() {
    if (s_mqtt.connected()) s_mqtt.disconnect(true);
    s_lastReconnectMs = 0;
    return connectMqtt();
}

void mqttClientLoop(const SensorData& data) {
    if (!s_enabled || !s_host.length()) return;
    if (wifiManagerGetState() != WIFI_STATE_CONNECTED) return;

    if (!s_mqtt.connected()) {
        unsigned long now = millis();
        if (now - s_lastReconnectMs < MQTT_RECONNECT_INTERVAL_MS) return;
        s_lastReconnectMs = now;
        connectMqtt();
        return;
    }

    s_mqtt.loop();
    unsigned long now = millis();
    if (now - s_lastPublishMs >= MQTT_PUBLISH_INTERVAL_MS) {
        s_lastPublishMs = now;
        String payload = makeTelemetry(data);
        if (s_mqtt.publish((topicBase() + "/telemetry").c_str(), 1, false, payload.c_str())) {
            s_statusText = "MQTT TLS: đã gửi telemetry QoS 1";
        }
    }
}

bool mqttClientIsConnected() { return s_mqtt.connected(); }
String mqttClientGetStatusText() { return s_statusText; }
String mqttClientGetDeviceId() { return s_deviceId; }
