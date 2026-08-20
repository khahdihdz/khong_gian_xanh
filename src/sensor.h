#ifndef SENSOR_H
#define SENSOR_H

#include <Arduino.h>

struct SensorData {
    float temperature;
    float humidity;
    uint16_t tvoc;
    uint16_t eco2;
    uint8_t  aqi;
    String   aqiLabel;
    bool     sht31Ok;
    bool     ens160Ok;
    bool     warning;
    String   warningReason;
    unsigned long lastUpdateMs;
};

bool sensorInit();
void sensorRetryIfNeeded();
void sensorRead();
String sensorClassifyAQI(uint8_t aqiRaw, uint16_t tvoc, uint16_t eco2);
void sensorEvaluateWarning();

float sensorGetTemperatureOffset();
float sensorGetHumidityOffset();
bool sensorSetCalibration(float tempOffset, float humidityOffset);
void sensorResetCalibration();

extern SensorData g_sensorData;

#endif // SENSOR_H
