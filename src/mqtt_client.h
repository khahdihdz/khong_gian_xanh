#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include <Arduino.h>
#include "sensor.h"

void mqttClientInit();
void mqttClientLoop(const SensorData& data);
bool mqttClientIsConnected();
String mqttClientGetStatusText();
String mqttClientGetDeviceId();

#endif
