#include "sensor.h"
#include "config.h"

#include <Wire.h>
#include <Preferences.h>
#include <Adafruit_SHT31.h>
#include <SparkFun_ENS160.h>

static Adafruit_SHT31 sht31 = Adafruit_SHT31();
static SparkFun_ENS160 ens160;
static Preferences calibrationPrefs;
static float s_tempOffset = HARD_CALIBRATION_ENABLED ? HARD_TEMP_OFFSET_C : 0.0f;
static float s_humidityOffset = HARD_CALIBRATION_ENABLED ? HARD_HUMIDITY_OFFSET_RH : 0.0f;
static float s_filteredTemp = NAN;
static float s_filteredHumidity = NAN;

SensorData g_sensorData = {
    .temperature = NAN, .humidity = NAN, .tvoc = 0, .eco2 = 0, .aqi = 0,
    .aqiLabel = "Chưa có dữ liệu", .sht31Ok = false, .ens160Ok = false,
    .warning = false, .warningReason = "", .lastUpdateMs = 0
};

static unsigned long s_lastShtRetryMs = 0;
static unsigned long s_lastEnsRetryMs = 0;

static bool initSht31() {
    if (sht31.begin(SHT31_ADDRESS)) return true;
    if (sht31.begin(0x45)) return true;
    return false;
}

static bool initEns160() {
    if (ens160.begin(Wire, ENS160_ADDRESS)) {
        ens160.setOperatingMode(SFE_ENS160_STANDARD);
        return true;
    }
    if (ens160.begin(Wire, 0x52)) {
        ens160.setOperatingMode(SFE_ENS160_STANDARD);
        return true;
    }
    return false;
}

static bool validSample(float t, float h) {
    return isfinite(t) && isfinite(h) && t >= TEMP_MIN_VALID && t <= TEMP_MAX_VALID &&
           h >= HUMIDITY_MIN_VALID && h <= HUMIDITY_MAX_VALID;
}

static float ema(float previous, float current, float alpha) {
    return isnan(previous) ? current : (previous + alpha * (current - previous));
}

bool sensorInit() {
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    Wire.setClock(400000);

    calibrationPrefs.begin(CALIBRATION_NAMESPACE, false);
#if !HARD_CALIBRATION_ENABLED
    s_tempOffset = calibrationPrefs.getFloat(CALIBRATION_KEY_TEMP, 0.0f);
    s_humidityOffset = calibrationPrefs.getFloat(CALIBRATION_KEY_HUM, 0.0f);
#endif

    g_sensorData.sht31Ok = initSht31();
    if (!g_sensorData.sht31Ok) Serial.println("[SENSOR] Cảnh báo: Không tìm thấy SHT31-D, sẽ thử lại định kỳ.");
    g_sensorData.ens160Ok = initEns160();
    if (!g_sensorData.ens160Ok) Serial.println("[SENSOR] Cảnh báo: Không tìm thấy ENS160, sẽ thử lại định kỳ.");

    s_lastShtRetryMs = millis();
    s_lastEnsRetryMs = millis();
    return g_sensorData.sht31Ok || g_sensorData.ens160Ok;
}

void sensorRetryIfNeeded() {
    unsigned long now = millis();
    if (!g_sensorData.sht31Ok && now - s_lastShtRetryMs >= SENSOR_ERROR_RETRY_MS) {
        s_lastShtRetryMs = now;
        g_sensorData.sht31Ok = initSht31();
        if (g_sensorData.sht31Ok) Serial.println("[SENSOR] SHT31-D đã kết nối lại thành công.");
    }
    if (!g_sensorData.ens160Ok && now - s_lastEnsRetryMs >= SENSOR_ERROR_RETRY_MS) {
        s_lastEnsRetryMs = now;
        g_sensorData.ens160Ok = initEns160();
        if (g_sensorData.ens160Ok) Serial.println("[SENSOR] ENS160 đã kết nối lại thành công.");
    }
}

String sensorClassifyAQI(uint8_t aqiRaw, uint16_t tvoc, uint16_t eco2) {
    String label;
    switch (aqiRaw) {
        case 1: label = "Rất tốt"; break;
        case 2: label = "Tốt"; break;
        case 3: label = "Trung bình"; break;
        case 4: label = "Kém"; break;
        case 5: label = "Ô nhiễm"; break;
        default: label = "Không xác định"; break;
    }
    if (tvoc >= TVOC_WARNING_THRESHOLD || eco2 >= ECO2_WARNING_THRESHOLD) if (aqiRaw < 4) label = "Kém";
    return label;
}

void sensorEvaluateWarning() {
    g_sensorData.warning = false;
    g_sensorData.warningReason = "";
    auto appendReason = [](const char* reason) {
        if (g_sensorData.warningReason.length() > 0) g_sensorData.warningReason += ", ";
        g_sensorData.warningReason += reason;
        g_sensorData.warning = true;
    };
    if (g_sensorData.sht31Ok && isfinite(g_sensorData.temperature) && g_sensorData.temperature > TEMP_WARNING_THRESHOLD) appendReason("Nhiệt độ cao");
    if (g_sensorData.sht31Ok && isfinite(g_sensorData.humidity) && g_sensorData.humidity > HUMIDITY_WARNING_THRESHOLD) appendReason("Độ ẩm cao");
    if (g_sensorData.ens160Ok && g_sensorData.aqi >= 4) appendReason("Chất lượng không khí kém");
    if (g_sensorData.ens160Ok && g_sensorData.tvoc >= TVOC_WARNING_THRESHOLD) appendReason("TVOC cao");
    if (g_sensorData.ens160Ok && g_sensorData.eco2 >= ECO2_WARNING_THRESHOLD) appendReason("eCO2 cao");
}

void sensorRead() {
    if (g_sensorData.sht31Ok) {
        float rawT = sht31.readTemperature();
        float rawH = sht31.readHumidity();
        if (!validSample(rawT, rawH)) {
            Serial.println("[SENSOR] Mẫu SHT31 không hợp lệ, giữ giá trị ổn định trước đó.");
            if (isnan(s_filteredTemp) || isnan(s_filteredHumidity)) {
                g_sensorData.sht31Ok = false;
                g_sensorData.temperature = NAN;
                g_sensorData.humidity = NAN;
                s_lastShtRetryMs = millis();
            }
        } else {
            float t = rawT + s_tempOffset;
            float h = rawH + s_humidityOffset;
            if (isfinite(s_filteredTemp) && fabsf(t - s_filteredTemp) > MAX_TEMP_STEP_C) t = s_filteredTemp;
            if (isfinite(s_filteredHumidity) && fabsf(h - s_filteredHumidity) > MAX_HUMIDITY_STEP_RH) h = s_filteredHumidity;
            t = constrain(t, TEMP_MIN_VALID, TEMP_MAX_VALID);
            h = constrain(h, HUMIDITY_MIN_VALID, HUMIDITY_MAX_VALID);
            s_filteredTemp = ema(s_filteredTemp, t, TEMP_FILTER_ALPHA);
            s_filteredHumidity = ema(s_filteredHumidity, h, HUMIDITY_FILTER_ALPHA);
            g_sensorData.temperature = s_filteredTemp;
            g_sensorData.humidity = s_filteredHumidity;
        }
    }

    if (g_sensorData.ens160Ok) {
        if (g_sensorData.sht31Ok && isfinite(g_sensorData.temperature) && isfinite(g_sensorData.humidity)) {
            ens160.setTempCompensationCelsius(g_sensorData.temperature);
            ens160.setRHCompensationFloat(g_sensorData.humidity);
        }
        if (ens160.checkDataStatus()) {
            g_sensorData.aqi = ens160.getAQI();
            g_sensorData.tvoc = ens160.getTVOC();
            g_sensorData.eco2 = ens160.getECO2();
            g_sensorData.aqiLabel = sensorClassifyAQI(g_sensorData.aqi, g_sensorData.tvoc, g_sensorData.eco2);
        }
    } else {
        g_sensorData.aqi = 0; g_sensorData.tvoc = 0; g_sensorData.eco2 = 0; g_sensorData.aqiLabel = "Cảm biến lỗi";
    }
    sensorEvaluateWarning();
    g_sensorData.lastUpdateMs = millis();
}

float sensorGetTemperatureOffset() { return s_tempOffset; }
float sensorGetHumidityOffset() { return s_humidityOffset; }

bool sensorSetCalibration(float tempOffset, float humidityOffset) {
#if HARD_CALIBRATION_ENABLED
    // Hiệu chỉnh cứng: dashboard/NVS không được phép thay đổi offset firmware.
    (void)tempOffset;
    (void)humidityOffset;
    s_tempOffset = HARD_TEMP_OFFSET_C;
    s_humidityOffset = HARD_HUMIDITY_OFFSET_RH;
    s_filteredTemp = NAN;
    s_filteredHumidity = NAN;
    return true;
#else
    if (!isfinite(tempOffset) || !isfinite(humidityOffset) || tempOffset < -20.0f || tempOffset > 20.0f || humidityOffset < -30.0f || humidityOffset > 30.0f) return false;
    s_tempOffset = tempOffset;
    s_humidityOffset = humidityOffset;
    calibrationPrefs.putFloat(CALIBRATION_KEY_TEMP, s_tempOffset);
    calibrationPrefs.putFloat(CALIBRATION_KEY_HUM, s_humidityOffset);
    s_filteredTemp = NAN;
    s_filteredHumidity = NAN;
    return true;
#endif
}

void sensorResetCalibration() {
#if HARD_CALIBRATION_ENABLED
    s_tempOffset = HARD_TEMP_OFFSET_C;
    s_humidityOffset = HARD_HUMIDITY_OFFSET_RH;
    s_filteredTemp = NAN;
    s_filteredHumidity = NAN;
#else
    sensorSetCalibration(0.0f, 0.0f);
#endif
}
