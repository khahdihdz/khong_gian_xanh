#include "sensor.h"
#include "config.h"

#include <Wire.h>
#include <Adafruit_SHT31.h>    // Thư viện: "Adafruit SHT31 Library"
#include <SparkFun_ENS160.h>   // Thư viện: "SparkFun ENS160" (module ENS160+AHT21, chỉ dùng phần ENS160)

// ============================================================
//  ĐỐI TƯỢNG CẢM BIẾN
// ============================================================
static Adafruit_SHT31 sht31 = Adafruit_SHT31();
static SparkFun_ENS160 ens160;

// Biến toàn cục chia sẻ dữ liệu cảm biến
SensorData g_sensorData = {
    .temperature = NAN,
    .humidity = NAN,
    .tvoc = 0,
    .eco2 = 0,
    .aqi = 0,
    .aqiLabel = "Chưa có dữ liệu",
    .sht31Ok = false,
    .ens160Ok = false,
    .warning = false,
    .warningReason = "",
    .lastUpdateMs = 0
};

static unsigned long s_lastShtRetryMs = 0;
static unsigned long s_lastEnsRetryMs = 0;

// ------------------------------------------------------------
//  Khởi tạo SHT31-D (thử cả 2 địa chỉ I2C phổ biến)
// ------------------------------------------------------------
static bool initSht31() {
    if (sht31.begin(SHT31_ADDRESS)) return true;
    if (sht31.begin(0x45)) return true;
    return false;
}

// ------------------------------------------------------------
//  Khởi tạo ENS160 (thử cả 2 địa chỉ I2C phổ biến)
// ------------------------------------------------------------
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

// ------------------------------------------------------------
//  Khởi tạo toàn bộ cảm biến (gọi 1 lần trong setup())
// ------------------------------------------------------------
bool sensorInit() {
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    Wire.setClock(400000);

    g_sensorData.sht31Ok = initSht31();
    if (!g_sensorData.sht31Ok) {
        Serial.println("[SENSOR] Cảnh báo: Không tìm thấy SHT31-D, sẽ thử lại định kỳ.");
    }

    g_sensorData.ens160Ok = initEns160();
    if (!g_sensorData.ens160Ok) {
        Serial.println("[SENSOR] Cảnh báo: Không tìm thấy ENS160, sẽ thử lại định kỳ.");
    }

    s_lastShtRetryMs = millis();
    s_lastEnsRetryMs = millis();

    return g_sensorData.sht31Ok || g_sensorData.ens160Ok;
}

// ------------------------------------------------------------
//  Thử khởi tạo lại cảm biến lỗi (không chặn chương trình)
// ------------------------------------------------------------
void sensorRetryIfNeeded() {
    unsigned long now = millis();

    if (!g_sensorData.sht31Ok && (now - s_lastShtRetryMs >= SENSOR_ERROR_RETRY_MS)) {
        s_lastShtRetryMs = now;
        Serial.println("[SENSOR] Đang thử khởi tạo lại SHT31-D...");
        g_sensorData.sht31Ok = initSht31();
        if (g_sensorData.sht31Ok) {
            Serial.println("[SENSOR] SHT31-D đã kết nối lại thành công.");
        }
    }

    if (!g_sensorData.ens160Ok && (now - s_lastEnsRetryMs >= SENSOR_ERROR_RETRY_MS)) {
        s_lastEnsRetryMs = now;
        Serial.println("[SENSOR] Đang thử khởi tạo lại ENS160...");
        g_sensorData.ens160Ok = initEns160();
        if (g_sensorData.ens160Ok) {
            Serial.println("[SENSOR] ENS160 đã kết nối lại thành công.");
        }
    }
}

// ------------------------------------------------------------
//  Phân loại AQI -> nhãn tiếng Việt
// ------------------------------------------------------------
String sensorClassifyAQI(uint8_t aqiRaw, uint16_t tvoc, uint16_t eco2) {
    String label;
    switch (aqiRaw) {
        case 1:  label = "Rất tốt";   break;
        case 2:  label = "Tốt";       break;
        case 3:  label = "Trung bình"; break;
        case 4:  label = "Kém";       break;
        case 5:  label = "Ô nhiễm";   break;
        default: label = "Không xác định"; break;
    }

    if (tvoc >= TVOC_WARNING_THRESHOLD || eco2 >= ECO2_WARNING_THRESHOLD) {
        if (aqiRaw < 4) label = "Kém";
    }

    return label;
}

// ------------------------------------------------------------
//  Đánh giá điều kiện cảnh báo tổng hợp
// ------------------------------------------------------------
void sensorEvaluateWarning() {
    g_sensorData.warning = false;
    g_sensorData.warningReason = "";

    auto appendReason = [](const char* reason) {
        if (g_sensorData.warningReason.length() > 0) g_sensorData.warningReason += ", ";
        g_sensorData.warningReason += reason;
        g_sensorData.warning = true;
    };

    if (g_sensorData.sht31Ok && !isnan(g_sensorData.temperature) &&
        g_sensorData.temperature > TEMP_WARNING_THRESHOLD) {
        appendReason("Nhiệt độ cao");
    }
    if (g_sensorData.sht31Ok && !isnan(g_sensorData.humidity) &&
        g_sensorData.humidity > HUMIDITY_WARNING_THRESHOLD) {
        appendReason("Độ ẩm cao");
    }
    if (g_sensorData.ens160Ok && g_sensorData.aqi >= 4) {
        appendReason("Chất lượng không khí kém");
    }
    if (g_sensorData.ens160Ok && g_sensorData.tvoc >= TVOC_WARNING_THRESHOLD) {
        appendReason("TVOC cao");
    }
    if (g_sensorData.ens160Ok && g_sensorData.eco2 >= ECO2_WARNING_THRESHOLD) {
        appendReason("eCO2 cao");
    }
}

// ------------------------------------------------------------
//  Đọc toàn bộ cảm biến - KHÔNG BAO GIỜ làm treo chương trình
// ------------------------------------------------------------
void sensorRead() {
    if (g_sensorData.sht31Ok) {
        float t = sht31.readTemperature();
        float h = sht31.readHumidity();

        if (isnan(t) || isnan(h)) {
            Serial.println("[SENSOR] Lỗi đọc SHT31-D!");
            g_sensorData.sht31Ok = false;
        } else {
            g_sensorData.temperature = t;
            g_sensorData.humidity = h;
        }
    }

    if (g_sensorData.ens160Ok) {
        // Bù nhiệt độ/độ ẩm cho ENS160 bằng dữ liệu thực tế từ SHT31-D
        // (nếu không có, ENS160 sẽ dùng giá trị mặc định nội bộ 25°C/50% RH,
        // khiến AQI/TVOC/eCO2 kém chính xác).
        if (g_sensorData.sht31Ok && !isnan(g_sensorData.temperature) && !isnan(g_sensorData.humidity)) {
            ens160.setTempCompensationCelsius(g_sensorData.temperature);
            ens160.setRHCompensationFloat(g_sensorData.humidity);
        }

        if (ens160.checkDataStatus()) {
            g_sensorData.aqi  = ens160.getAQI();
            g_sensorData.tvoc = ens160.getTVOC();
            g_sensorData.eco2 = ens160.getECO2();
            g_sensorData.aqiLabel = sensorClassifyAQI(g_sensorData.aqi, g_sensorData.tvoc, g_sensorData.eco2);
        }
    } else {
        g_sensorData.aqiLabel = "Cảm biến lỗi";
    }

    sensorEvaluateWarning();
    g_sensorData.lastUpdateMs = millis();
}
