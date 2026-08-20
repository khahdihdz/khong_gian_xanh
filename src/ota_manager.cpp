#include "ota_manager.h"
#include "config.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>
#include <Preferences.h>

static Preferences prefs;
static String s_status = "Chưa kiểm tra";
static String s_remoteTag;
static String s_remoteVersion;
static int s_remoteBuild = -1;
static String s_firmwareUrl;
static unsigned long s_lastCheckMs = 0;
static bool s_checking = false;
static bool s_updateInProgress = false;

static String jsonValue(const String& json, const String& key) {
    String needle = "\"" + key + "\"";
    int p = json.indexOf(needle);
    if (p < 0) return "";
    p = json.indexOf(':', p + needle.length());
    if (p < 0) return "";
    p++;
    while (p < (int)json.length() && (json[p] == ' ' || json[p] == '\n' || json[p] == '\r' || json[p] == '\t')) p++;
    if (p >= (int)json.length()) return "";
    if (json[p] == '"') {
        p++;
        int e = p;
        while (e < (int)json.length()) {
            if (json[e] == '"' && json[e - 1] != '\\') break;
            e++;
        }
        return json.substring(p, e);
    }
    int e = p;
    while (e < (int)json.length() && json[e] != ',' && json[e] != '}') e++;
    return json.substring(p, e);
}

static String versionFromTag(String tag) {
    if (tag.startsWith("v")) tag.remove(0, 1);
    int dash = tag.indexOf('-');
    if (dash >= 0) tag = tag.substring(0, dash);
    return tag;
}

static int buildFromName(const String& name) {
    int p = name.indexOf("build");
    if (p < 0) return -1;
    p += 5;
    while (p < (int)name.length() && !isDigit(name[p])) p++;
    if (p >= (int)name.length()) return -1;
    int e = p;
    while (e < (int)name.length() && isDigit(name[e])) e++;
    return name.substring(p, e).toInt();
}

static bool newerRelease(const String& tag) {
    String installed = prefs.getString("release", "");
    if (installed.length() == 0) return tag != String("v") + FIRMWARE_VERSION;
    return tag != installed;
}

void otaManagerInit() {
    prefs.begin("auto_ota", false);
    s_status = "Chờ kiểm tra";
}

bool otaManagerCheckNow() {
    if (WiFi.status() != WL_CONNECTED) {
        s_status = "Không có Internet";
        return false;
    }
    s_checking = true;
    s_status = "Đang kiểm tra bản mới...";

    WiFiClientSecure client;
    client.setInsecure(); // TEST: sẽ thay bằng CA certificate trước khi merge main.
    client.setTimeout(12);
    HTTPClient http;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(15000);
    if (!http.begin(client, "https://api.github.com/repos/khahdihdz/khong_gian_xanh/releases/latest")) {
        s_checking = false; s_status = "Không mở được GitHub"; return false;
    }
    http.addHeader("User-Agent", "KhongGianXanh-ESP32");
    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        http.end(); s_checking = false; s_status = "GitHub HTTP " + String(code); return false;
    }
    String body = http.getString();
    http.end();

    s_remoteTag = jsonValue(body, "tag_name");
    String releaseName = jsonValue(body, "name");
    s_remoteVersion = versionFromTag(s_remoteTag);
    s_remoteBuild = buildFromName(releaseName);

    int search = 0;
    s_firmwareUrl = "";
    while ((search = body.indexOf("browser_download_url", search)) >= 0) {
        int end = body.indexOf('}', search);
        String chunk = body.substring(search, end > 0 ? end : body.length());
        String url = jsonValue("{" + chunk + "}", "browser_download_url");
        if (url.indexOf("firmware-v") >= 0 && url.endsWith(".bin")) { s_firmwareUrl = url; break; }
        search += 20;
    }

    if (s_remoteTag.length() == 0 || s_firmwareUrl.length() == 0) {
        s_checking = false; s_status = "Release không có firmware.bin"; return false;
    }

    bool newer = newerRelease(s_remoteTag);
    s_status = newer ? ("Có bản mới: " + s_remoteTag) : ("Đã là bản mới nhất: " + s_remoteTag);
    s_checking = false;
    s_lastCheckMs = millis();
    return true;
}

bool otaManagerUpdateNow() {
    if (s_updateInProgress || s_firmwareUrl.length() == 0 || WiFi.status() != WL_CONNECTED) return false;
    s_updateInProgress = true;
    s_status = "Đang tải firmware...";

    WiFiClientSecure client;
    client.setInsecure(); // TEST only.
    client.setTimeout(20);
    HTTPClient http;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(30000);
    if (!http.begin(client, s_firmwareUrl)) {
        s_updateInProgress = false; s_status = "Không mở được firmware"; return false;
    }
    http.addHeader("User-Agent", "KhongGianXanh-ESP32");
    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        http.end(); s_updateInProgress = false; s_status = "Firmware HTTP " + String(code); return false;
    }
    int total = http.getSize();
    if (total <= 0) {
        http.end(); s_updateInProgress = false; s_status = "Firmware không hợp lệ"; return false;
    }
    if (!Update.begin((size_t)total)) {
        http.end(); s_updateInProgress = false; s_status = "Không đủ flash OTA"; return false;
    }

    WiFiClient* stream = http.getStreamPtr();
    uint8_t buffer[1024];
    int remaining = total;
    while (http.connected() && remaining > 0) {
        size_t available = stream->available();
        if (available) {
            size_t n = stream->readBytes(buffer, min(available, sizeof(buffer)));
            if (Update.write(buffer, n) != n) {
                Update.abort(); http.end(); s_updateInProgress = false; s_status = "Ghi firmware thất bại"; return false;
            }
            remaining -= (int)n;
        } else delay(1);
    }
    bool ok = Update.end(true) && Update.isFinished() && remaining <= 0;
    http.end();
    if (!ok) {
        s_updateInProgress = false; s_status = "OTA thất bại"; return false;
    }
    prefs.putString("release", s_remoteTag);
    s_status = "OTA thành công, đang khởi động lại...";
    delay(250);
    ESP.restart();
    return true;
}

void otaManagerLoop() {
    if (WiFi.status() != WL_CONNECTED || s_updateInProgress || s_checking) return;
    if (millis() - s_lastCheckMs < AUTO_OTA_CHECK_INTERVAL_MS) return;
    if (!otaManagerCheckNow()) return;
    if (newerRelease(s_remoteTag)) otaManagerUpdateNow();
}

String otaManagerGetStatus() { return s_status; }

String otaManagerGetInfoJson() {
    String installed = prefs.getString("release", "");
    String json = "{\"status\":\"" + s_status + "\",\"installed\":\"" + installed + "\",\"remote_tag\":\"" + s_remoteTag + "\",\"remote_version\":\"" + s_remoteVersion + "\",\"remote_build\":" + String(s_remoteBuild) + ",\"update_available\":" + String(newerRelease(s_remoteTag) ? "true" : "false") + "}";
    return json;
}
