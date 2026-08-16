#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include <Arduino.h>
#include "sensor.h"

void mqttClientInit();
void mqttClientLoop(const SensorData& data);
// Nạp lại cấu hình MQTT từ Preferences và kết nối lại ngay, không cần reboot.
bool mqttClientReloadConfig();
// Kiểm tra kết nối MQTT hiện tại; có thể dùng sau khi lưu cấu hình.
bool mqttClientReconnectNow();
bool mqttClientIsConnected();
String mqttClientGetStatusText();
String mqttClientGetDeviceId();

#endif
