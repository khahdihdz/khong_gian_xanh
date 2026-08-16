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
    Serial.printf("[MQTT] ĐÃ KẾT NỐI HiveMQ: %s:%u\n", s_host.c_str(), s_port);
    Serial.printf("[MQTT] Device ID: %s\n", s_deviceId.c_str());
}

static void onDisconnect(espMqttClientTypes::DisconnectReason reason) {
    s_statusText = "MQTT ngắt kết nối, mã lỗi: " + String((int)reason);
    Serial.printf("[MQTT] NGẮT KẾT NỐI, mã lỗi: %d\n", (int)reason);
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
    if (!s_host.length()) { s_statusText = "Thiếu MQTT Broker"; return false; }

    // Đăng ký callback trước khi connect để không bỏ lỡ sự kiện.
    s_mqtt.onConnect(onConnect);
    s_mqtt.onDisconnect(onDisconnect);
    s_mqtt.onMessage(onMessage);

    s_mqtt.setClientId(s_deviceId.c_str());
    s_mqtt.setCleanSession(false);
    s_mqtt.setKeepAlive(30);
    s_mqtt.setTimeout(10);
    s_mqtt.setServer(s_host.c_str(), s_port);
    s_mqtt.setCredentials(s_user.c_str(), s_pass.c_str());

    String willTopic = topicBase() + "/status";
    s_mqtt.setWill(willTopic.c_str(), 1, true, "offline");

    if (s_tls) {
        if (!s_ca.length()) { s_statusText = "MQTT TLS chưa có CA certificate"; return false; }
        if (!s_ca.startsWith("-----BEGIN CERTIFICATE-----")) {
            s_statusText = "CA certificate không đúng định dạng PEM";
            return false;
        }
        s_mqtt.setCACert(s_ca.c_str());
    }
    return true;
}

static bool connectMqtt() {
    if (!s_enabled || !s_host.length()) return false;
    if (WiFi.status() != WL_CONNECTED) { s_statusText = "Chờ WiFi kết nối"; return false; }

    // connected() chỉ true sau CONNACK; connect() chỉ bắt đầu quá trình kết nối.
    // disconnected() == false còn có thể là trạng thái đang kết nối, vì vậy
    // không được coi nó là kết nối thành công để xử lý nút Kiểm tra.
    if (!s_mqtt.disconnected()) {
        s_statusText = "MQTT đang kết nối...";
        return true;
    }
    if (!configureClient()) return false;

    s_statusText = "MQTT đang kết nối...";
    bool started = s_mqtt.connect();
    if (!started) {
        s_statusText = "MQTT không thể bắt đầu kết nối";
        Serial.println("[MQTT] Không thể bắt đầu connect()");
        return false;
    }
    Serial.printf("[MQTT] Đã bắt đầu kết nối tới %s:%u (TLS=%s)\n", s_host.c_str(), s_port, s_tls ? "ON" : "OFF");
    return true;
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
    // Khi reload cấu hình, client có thể đang ở trạng thái connecting. Phải
    // ngắt hẳn phiên cũ trước khi bắt đầu phiên mới, nếu không connect() mới
    // sẽ bị bỏ qua và nút "Lưu & kết nối" báo thành công giả.
    if (!s_mqtt.disconnected()) {
        s_mqtt.disconnect(true);
        delay(50);
    }
    loadConfig();
    s_lastReconnectMs = 0;
    s_lastPublishMs = millis();
    if (!s_enabled || !s_host.length()) {
        s_statusText = "MQTT đang tắt hoặc chưa cấu hình";
        return true;
    }
    // true ở đây chỉ có nghĩa là đã khởi động quá trình kết nối.
    // Trạng thái kết nối thật được cập nhật bởi onConnect().
    return connectMqtt();
}

bool mqttClientReconnectNow() {
    // Luôn tạo một phiên kết nối mới, kể cả khi client đang ở trạng thái
    // connecting. Đây là trường hợp làm nút "Kết nối / kiểm tra" trước đó
    // có thể không thực sự gọi connect() lần mới.
    if (!s_mqtt.disconnected()) {
        s_mqtt.disconnect(true);
        delay(50);
    }
    s_lastReconnectMs = 0;
    return connectMqtt();
}

void mqttClientLoop(const SensorData& data) {
    if (!s_enabled || !s_host.length()) return;
    if (wifiManagerGetState() != WIFI_STATE_CONNECTED) return;

    if (!s_mqtt.connected()) {
        // Không gọi connect() lặp khi client đang connecting/disconnecting.
        if (!s_mqtt.disconnected()) return;
        unsigned long now = millis();
        if (now - s_lastReconnectMs < MQTT_RECONNECT_INTERVAL_MS) return;
        s_lastReconnectMs = now;
        connectMqtt();
        return;
    }

    // espMqttClient trên ESP32 mặc định dùng internal task.
    unsigned long now = millis();
    if (now - s_lastPublishMs >= MQTT_PUBLISH_INTERVAL_MS) {
        s_lastPublishMs = now;
        String payload = makeTelemetry(data);
        if (s_mqtt.publish((topicBase() + "/telemetry").c_str(), 1, false, payload.c_str())) {
            s_statusText = "MQTT TLS: đã gửi telemetry QoS 1";
            Serial.println("[MQTT] Đã publish telemetry QoS 1");
        } else {
            s_statusText = "MQTT đã kết nối nhưng publish thất bại";
        }
    }
}

bool mqttClientIsConnected() { return s_mqtt.connected(); }
String mqttClientGetStatusText() { return s_statusText; }
String mqttClientGetDeviceId() { return s_deviceId; }
