#include "display.h"
#include "config.h"

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

static Adafruit_SSD1306 oled(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);
static bool s_oledOk = false;

// ============================================================
//  ICON ĐƠN GIẢN 8x8 PIXEL (bitmap 1-bit)
// ============================================================
// Icon nhiệt kế
static const unsigned char ICON_TEMP[] PROGMEM = {
    0x18, 0x24, 0x24, 0x24, 0x24, 0x3C, 0x3C, 0x18
};
// Icon giọt nước (độ ẩm)
static const unsigned char ICON_DROP[] PROGMEM = {
    0x08, 0x1C, 0x1C, 0x3E, 0x3E, 0x3E, 0x1C, 0x00
};
// Icon sóng WiFi
static const unsigned char ICON_WIFI[] PROGMEM = {
    0x00, 0x3C, 0x42, 0x00, 0x18, 0x24, 0x00, 0x18
};
// Icon đám mây (chất lượng không khí)
static const unsigned char ICON_AIR[] PROGMEM = {
    0x00, 0x1C, 0x3E, 0x7F, 0x7F, 0x3E, 0x00, 0x00
};

// ============================================================
bool displayInit() {
    s_oledOk = oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS);
    if (!s_oledOk) {
        Serial.println("[DISPLAY] Không tìm thấy màn hình OLED!");
        return false;
    }
    oled.clearDisplay();
    oled.setTextColor(SSD1306_WHITE);
    oled.display();
    return true;
}

// ------------------------------------------------------------
//  Hiệu ứng quét ngang khi khởi động
// ------------------------------------------------------------
void displaySplashEffect() {
    if (!s_oledOk) return;
    oled.clearDisplay();
    oled.setTextSize(2);
    oled.setCursor(4, 8);
    oled.println("KHONG GIAN");
    oled.setCursor(36, 26);
    oled.println("XANH");
    oled.setTextSize(1);
    oled.setCursor(14, 46);
    oled.println("Giam sat moi truong");
    oled.display();

    for (int x = 0; x <= OLED_WIDTH; x += 8) {
        oled.drawFastVLine(x, 58, 6, SSD1306_WHITE);
        oled.display();
        delay(15); // chỉ dùng delay ngắn 1 lần lúc khởi động, không ảnh hưởng vòng lặp chính
    }
}

// ------------------------------------------------------------
//  Vẽ thanh trạng thái AQI (mức 1-5)
// ------------------------------------------------------------
static void drawAqiBar(int x, int y, int w, int h, uint8_t aqi) {
    oled.drawRect(x, y, w, h, SSD1306_WHITE);
    if (aqi == 0) return;
    int fillW = (w - 2) * aqi / 5;
    oled.fillRect(x + 1, y + 1, fillW, h - 2, SSD1306_WHITE);
}

// ------------------------------------------------------------
//  Vẽ giao diện chính
// ------------------------------------------------------------
void displayUpdate(const SensorData& data, const String& wifiStatus, const String& timeStr) {
    if (!s_oledOk) return;

    oled.clearDisplay();

    // --- Header ---
    oled.drawFastHLine(0, 10, OLED_WIDTH, SSD1306_WHITE);
    oled.setTextSize(1);
    oled.setCursor(2, 0);
    oled.print("KHONG GIAN XANH");

    // Chấm "heartbeat" nhấp nháy góc phải trên báo hiệu hệ thống đang chạy
    if ((millis() / 500) % 2 == 0) {
        oled.fillCircle(122, 4, 2, SSD1306_WHITE);
    }

    // --- Cột trái: Nhiệt độ & Độ ẩm ---
    oled.drawBitmap(2, 15, ICON_TEMP, 8, 8, SSD1306_WHITE);
    oled.setCursor(13, 14);
    if (data.dhtOk) {
        oled.printf("%.1fC", data.temperature);
    } else {
        oled.print("Loi");
    }

    oled.drawBitmap(2, 27, ICON_DROP, 8, 8, SSD1306_WHITE);
    oled.setCursor(13, 26);
    if (data.dhtOk) {
        oled.printf("%.0f%%", data.humidity);
    } else {
        oled.print("Loi");
    }

    // --- Cột phải: AQI / TVOC / eCO2 ---
    oled.drawBitmap(68, 15, ICON_AIR, 8, 8, SSD1306_WHITE);
    oled.setCursor(79, 14);
    oled.print(data.ens160Ok ? data.aqiLabel : String("Loi"));

    oled.setCursor(68, 27);
    if (data.ens160Ok) {
        oled.printf("TVOC:%dppb", data.tvoc);
    } else {
        oled.print("TVOC: --");
    }

    // --- Thanh mức AQI ---
    drawAqiBar(2, 38, 60, 6, data.ens160Ok ? data.aqi : 0);
    oled.setCursor(66, 38);
    if (data.ens160Ok) {
        oled.printf("CO2:%dppm", data.eco2);
    } else {
        oled.print("CO2: --");
    }

    // --- Footer: WiFi + giờ ---
    oled.drawFastHLine(0, 48, OLED_WIDTH, SSD1306_WHITE);
    oled.drawBitmap(2, 52, ICON_WIFI, 8, 8, SSD1306_WHITE);
    oled.setCursor(13, 52);
    oled.print(wifiStatus);

    oled.setCursor(72, 52);
    oled.print(timeStr);

    // --- Cảnh báo nhấp nháy toàn màn hình viền ngoài ---
    if (data.warning && (millis() / 400) % 2 == 0) {
        oled.drawRect(0, 0, OLED_WIDTH, OLED_HEIGHT, SSD1306_WHITE);
    }

    oled.display();
}

// ------------------------------------------------------------
void displayShowError(const String& message) {
    if (!s_oledOk) return;
    oled.clearDisplay();
    oled.setTextSize(1);
    oled.setCursor(0, 20);
    oled.println("LOI CAM BIEN:");
    oled.setCursor(0, 35);
    oled.println(message);
    oled.display();
}
