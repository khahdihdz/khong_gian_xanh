#include "sensor.h"
#include "config.h"

#include <Wire.h>
#include <DHT.h>
#include <SparkFun_ENS160.h>   // Thư viện: "SparkFun ENS160" (quản lý bởi Library Manager)

// ============================================================
//  ĐỐI TƯỢNG CẢM BIẾN
// ============================================================
static DHT dht(DHT_PIN, DHT_TYPE);
static SparkFun_ENS160 ens160;

// Biến toàn cục chia sẻ dữ liệu cảm biến
SensorData g_sensorData = {
    .temperature = NAN,
    .humidity = NAN,
    .tvoc = 0,
    .eco2 = 0,
    .aqi = 0,
    .aqiLabel = "Chưa có dữ liệu",
    .dhtOk = false,
    .ens160Ok = false,
    .warning = false,
    .warningReason = "",
    .lastUpdateMs = 0
};

static unsigned long s_lastDhtRetryMs = 0;
static unsigned long s_lastEnsRetryMs = 0;

// ------------------------------------------------------------
//  Khởi tạo ENS160 (thử cả 2 địa chỉ I2C phổ biến)
// ------------------------------------------------------------
static bool initEns160() {
    if (ens160.begin(Wire, ENS160_ADDRESS)) {
        ens160.setOperatingMode(SFE_ENS160_STANDARD);
        return true;
    }
    // Thử địa chỉ phụ 0x52 nếu 0x53 không phản hồi
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

    dht.begin();
    g_sensorData.dhtOk = true; // DHT22 không có hàm kiểm tra begin(), coi như OK, sẽ tự phát hiện khi đọc lỗi

    g_sensorData.ens160Ok = initEns160();
    if (!g_sensorData.ens160Ok) {
        Serial.println("[SENSOR] Cảnh báo: Không tìm thấy ENS160, sẽ thử lại định kỳ.");
    }

    s_lastDhtRetryMs = millis();
    s_lastEnsRetryMs = millis();

    return g_sensorData.dhtOk || g_sensorData.ens160Ok;
}

// ------------------------------------------------------------
//  Thử khởi tạo lại cảm biến lỗi (không chặn chương trình)
// ------------------------------------------------------------
void sensorRetryIfNeeded() {
    unsigned long now = millis();

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
    // ENS160 trả AQI theo thang UBA 1-5
    String label;
    switch (aqiRaw) {
        case 1:  label = "Rất tốt";   break;
        case 2:  label = "Tốt";       break;
        case 3:  label = "Trung bình"; break;
        case 4:  label = "Kém";       break;
        case 5:  label = "Ô nhiễm";   break;
        default: label = "Không xác định"; break;
    }

    // Nếu TVOC hoặc eCO2 vượt ngưỡng nguy hiểm, nâng mức cảnh báo dù AQI thô còn thấp
    if (tvoc >= TVOC_WARNING_THRESHOLD || eco2 >= ECO2_WARNING_THRESHOLD) {
        if (aqiRaw < 4) label = "Kém"; // ép mức tối thiểu là "Kém" khi vượt ngưỡng
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

    if (g_sensorData.dhtOk && !isnan(g_sensorData.temperature) &&
        g_sensorData.temperature > TEMP_WARNING_THRESHOLD) {
        appendReason("Nhiệt độ cao");
    }
    if (g_sensorData.dhtOk && !isnan(g_sensorData.humidity) &&
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
    // --- Đọc DHT22 ---
    float t = dht.readTemperature();
    float h = dht.readHumidity();

    if (isnan(t) || isnan(h)) {
        // Đọc lỗi: đánh dấu lỗi, KHÔNG ghi đè dữ liệu cũ để dashboard không bị nhảy giá trị
        if (g_sensorData.dhtOk) {
            Serial.println("[SENSOR] Lỗi đọc DHT22!");
        }
        g_sensorData.dhtOk = false;
    } else {
        g_sensorData.temperature = t;
        g_sensorData.humidity = h;
        g_sensorData.dhtOk = true;
    }

    // --- Đọc ENS160 ---
    if (g_sensorData.ens160Ok) {
        if (ens160.checkDataStatus()) {
            g_sensorData.aqi  = ens160.getAQI();
            g_sensorData.tvoc = ens160.getTVOC();
            g_sensorData.eco2 = ens160.getECO2();
            g_sensorData.aqiLabel = sensorClassifyAQI(g_sensorData.aqi, g_sensorData.tvoc, g_sensorData.eco2);
        }
        // Nếu không có dữ liệu mới (checkDataStatus == false) thì giữ nguyên giá trị cũ, không lỗi.
    } else {
        g_sensorData.aqiLabel = "Cảm biến lỗi";
    }

    sensorEvaluateWarning();
    g_sensorData.lastUpdateMs = millis();
}
