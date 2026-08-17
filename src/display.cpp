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
//  CHUYỂN TIẾNG VIỆT CÓ DẤU (UTF-8) -> ASCII GẦN ĐÚNG
// ------------------------------------------------------------
//  Font mặc định của Adafruit_GFX chỉ có bảng ASCII/CP437, KHÔNG
//  vẽ được ký tự có dấu tiếng Việt (mỗi ký tự dấu chiếm 2-3 byte
//  UTF-8, bị hiểu nhầm thành nhiều ký tự CP437 rác -> vỡ chữ trên
//  OLED). Hàm này giải mã UTF-8 và quy đổi về chữ cái ASCII không
//  dấu gần nhất trước khi in ra màn hình.
// ============================================================
static String vnToAscii(const String& in) {
    String out;
    out.reserve(in.length());
    size_t i = 0;
    const uint8_t* s = (const uint8_t*)in.c_str();
    size_t len = in.length();

    while (i < len) {
        uint8_t c = s[i];
        uint32_t cp;
        if (c < 0x80) {
            out += (char)c;
            i += 1;
            continue;
        } else if ((c & 0xE0) == 0xC0 && i + 1 < len) {
            cp = ((uint32_t)(c & 0x1F) << 6) | (s[i + 1] & 0x3F);
            i += 2;
        } else if ((c & 0xF0) == 0xE0 && i + 2 < len) {
            cp = ((uint32_t)(c & 0x0F) << 12) | ((uint32_t)(s[i + 1] & 0x3F) << 6) | (s[i + 2] & 0x3F);
            i += 3;
        } else {
            i += 1; // byte không hợp lệ, bỏ qua
            continue;
        }

        char base = 0;
        bool upper = false;

        // Khối Latin Extended Additional (U+1EA0-1EF9): các tổ hợp dấu
        // sắc/huyền/hỏi/ngã/nặng trên a,e,i,o,u,y (kể cả ă,â,ê,ô,ơ,ư).
        if (cp >= 0x1EA0 && cp <= 0x1EB7)      { base = 'a'; upper = (cp % 2 == 0); }
        else if (cp >= 0x1EB8 && cp <= 0x1EC7) { base = 'e'; upper = (cp % 2 == 0); }
        else if (cp >= 0x1EC8 && cp <= 0x1ECB) { base = 'i'; upper = (cp % 2 == 0); }
        else if (cp >= 0x1ECC && cp <= 0x1EE1) { base = 'o'; upper = (cp % 2 == 0); }
        else if (cp >= 0x1EE2 && cp <= 0x1EF1) { base = 'u'; upper = (cp % 2 == 0); }
        else if (cp >= 0x1EF2 && cp <= 0x1EF9) { base = 'y'; upper = (cp % 2 == 0); }
        else {
            switch (cp) {
                case 0x00C0: case 0x00C1: case 0x00C2: case 0x00C3: base = 'a'; upper = true;  break;
                case 0x00E0: case 0x00E1: case 0x00E2: case 0x00E3: base = 'a'; upper = false; break;
                case 0x00C8: case 0x00C9: case 0x00CA:              base = 'e'; upper = true;  break;
                case 0x00E8: case 0x00E9: case 0x00EA:              base = 'e'; upper = false; break;
                case 0x00CC: case 0x00CD:                           base = 'i'; upper = true;  break;
                case 0x00EC: case 0x00ED:                           base = 'i'; upper = false; break;
                case 0x00D2: case 0x00D3: case 0x00D4: case 0x00D5: base = 'o'; upper = true;  break;
                case 0x00F2: case 0x00F3: case 0x00F4: case 0x00F5: base = 'o'; upper = false; break;
                case 0x00D9: case 0x00DA:                           base = 'u'; upper = true;  break;
                case 0x00F9: case 0x00FA:                           base = 'u'; upper = false; break;
                case 0x00DD:                                        base = 'y'; upper = true;  break;
                case 0x00FD:                                        base = 'y'; upper = false; break;
                case 0x0102:                                        base = 'a'; upper = true;  break;
                case 0x0103:                                        base = 'a'; upper = false; break;
                case 0x0110:                                        base = 'd'; upper = true;  break; // Đ
                case 0x0111:                                        base = 'd'; upper = false; break; // đ
                case 0x01A0:                                        base = 'o'; upper = true;  break; // Ơ
                case 0x01A1:                                        base = 'o'; upper = false; break; // ơ
                case 0x01AF:                                        base = 'u'; upper = true;  break; // Ư
                case 0x01B0:                                        base = 'u'; upper = false; break; // ư
                default: break; // ký tự lạ không nhận diện -> bỏ qua, không in rác
            }
        }

        if (base) out += upper ? (char)toupper(base) : base;
    }
    return out;
}

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
    if (data.sht31Ok) {
        oled.printf("%.1fC", data.temperature);
    } else {
        oled.print("Loi");
    }

    oled.drawBitmap(2, 27, ICON_DROP, 8, 8, SSD1306_WHITE);
    oled.setCursor(13, 26);
    if (data.sht31Ok) {
        oled.printf("%.0f%%", data.humidity);
    } else {
        oled.print("Loi");
    }

    // --- Cột phải: AQI / TVOC / eCO2 ---
    oled.drawBitmap(68, 15, ICON_AIR, 8, 8, SSD1306_WHITE);
    oled.setCursor(79, 14);
    oled.print(data.ens160Ok ? vnToAscii(data.aqiLabel) : String("Loi"));

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
    oled.print(vnToAscii(wifiStatus));

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
    oled.println(vnToAscii(message));
    oled.display();
}
