#ifndef CLOUD_SYNC_H
#define CLOUD_SYNC_H

#include <Arduino.h>
#include "sensor.h"

// ============================================================
// CLOUD WEBSOCKET - không MQTT, không cần thiết bị trung gian LAN.
// ESP32 chủ động mở WSS tới Cloudflare Worker; Worker dùng Durable
// Object để chuyển tiếp dữ liệu tới dashboard từ xa.
// ============================================================

void cloudSyncInit();
void cloudSyncLoop(const SensorData& data);
void cloudSyncSaveConfig(const String& url, const String& token, bool enabled);

bool   cloudSyncIsEnabled();
String cloudSyncGetUrl();
String cloudSyncGetToken();
bool   cloudSyncHasToken();
String cloudSyncGetStatusText();
String cloudSyncGetDeviceId();

#endif // CLOUD_SYNC_H
