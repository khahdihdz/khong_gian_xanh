#include "display.h"
#include "config.h"

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

static Adafruit_SSD1306 oled(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);
static bool s_oledOk = false;

// Font 5x7 tối giản cho các ký tự tiếng Việt thường dùng.
// Mỗi glyph gồm 5 cột, mỗi cột 7 bit; bit 0 là hàng trên cùng.
// Các chữ có dấu được ghép từ glyph chữ cái nền + dấu.
struct Glyph5x7 { uint32_t cp; uint8_t col[5]; };

static const Glyph5x7 GLYPHS[] PROGMEM = {
    { 'A', {0x7E,0x09,0x09,0x09,0x7E} }, { 'B', {0x7F,0x49,0x49,0x49,0x36} },
    { 'C', {0x3E,0x41,0x41,0x41,0x22} }, { 'D', {0x7F,0x41,0x41,0x22,0x1C} },
    { 'E', {0x7F,0x49,0x49,0x49,0x41} }, { 'F', {0x7F,0x09,0x09,0x09,0x01} },
    { 'G', {0x3E,0x41,0x49,0x49,0x7A} }, { 'H', {0x7F,0x08,0x08,0x08,0x7F} },
    { 'I', {0x41,0x41,0x7F,0x41,0x41} }, { 'J', {0x20,0x40,0x41,0x3F,0x01} },
    { 'K', {0x7F,0x08,0x14,0x22,0x41} }, { 'L', {0x7F,0x40,0x40,0x40,0x40} },
    { 'M', {0x7F,0x02,0x0C,0x02,0x7F} }, { 'N', {0x7F,0x04,0x08,0x10,0x7F} },
    { 'O', {0x3E,0x41,0x41,0x41,0x3E} }, { 'P', {0x7F,0x09,0x09,0x09,0x06} },
    { 'Q', {0x3E,0x41,0x51,0x21,0x5E} }, { 'R', {0x7F,0x09,0x19,0x29,0x46} },
    { 'S', {0x46,0x49,0x49,0x49,0x31} }, { 'T', {0x01,0x01,0x7F,0x01,0x01} },
    { 'U', {0x3F,0x40,0x40,0x40,0x3F} }, { 'V', {0x1F,0x20,0x40,0x20,0x1F} },
    { 'W', {0x3F,0x40,0x38,0x40,0x3F} }, { 'X', {0x63,0x14,0x08,0x14,0x63} },
    { 'Y', {0x07,0x08,0x70,0x08,0x07} }, { 'Z', {0x61,0x51,0x49,0x45,0x43} },
    { 'a', {0x20,0x54,0x54,0x54,0x78} }, { 'b', {0x7F,0x48,0x44,0x44,0x38} },
    { 'c', {0x38,0x44,0x44,0x44,0x20} }, { 'd', {0x38,0x44,0x44,0x48,0x7F} },
    { 'e', {0x38,0x54,0x54,0x54,0x18} }, { 'f', {0x08,0x7E,0x09,0x01,0x02} },
    { 'g', {0x18,0xA4,0xA4,0xA4,0x7C} }, { 'h', {0x7F,0x08,0x04,0x04,0x78} },
    { 'i', {0x00,0x44,0x7D,0x40,0x00} }, { 'j', {0x40,0x80,0x84,0x7D,0x00} },
    { 'k', {0x7F,0x10,0x28,0x44,0x00} }, { 'l', {0x00,0x41,0x7F,0x40,0x00} },
    { 'm', {0x7C,0x04,0x18,0x04,0x78} }, { 'n', {0x7C,0x08,0x04,0x04,0x78} },
    { 'o', {0x38,0x44,0x44,0x44,0x38} }, { 'p', {0xFC,0x24,0x24,0x24,0x18} },
    { 'q', {0x18,0x24,0x24,0x24,0xFC} }, { 'r', {0x7C,0x08,0x04,0x04,0x08} },
    { 's', {0x48,0x54,0x54,0x54,0x24} }, { 't', {0x04,0x3F,0x44,0x40,0x20} },
    { 'u', {0x3C,0x40,0x40,0x20,0x7C} }, { 'v', {0x1C,0x20,0x40,0x20,0x1C} },
    { 'w', {0x3C,0x40,0x30,0x40,0x3C} }, { 'x', {0x44,0x28,0x10,0x28,0x44} },
    { 'y', {0x1C,0xA0,0xA0,0xA0,0x7C} }, { 'z', {0x44,0x64,0x54,0x4C,0x44} },
    { '0', {0x3E,0x51,0x49,0x45,0x3E} }, { '1', {0x00,0x42,0x7F,0x40,0x00} },
    { '2', {0x42,0x61,0x51,0x49,0x46} }, { '3', {0x21,0x41,0x45,0x4B,0x31} },
    { '4', {0x18,0x14,0x12,0x7F,0x10} }, { '5', {0x27,0x45,0x45,0x45,0x39} },
    { '6', {0x3C,0x4A,0x49,0x49,0x30} }, { '7', {0x01,0x71,0x09,0x05,0x03} },
    { '8', {0x36,0x49,0x49,0x49,0x36} }, { '9', {0x06,0x49,0x49,0x29,0x1E} },
    { '.', {0x00,0x60,0x60,0x00,0x00} }, { ':', {0x00,0x36,0x36,0x00,0x00} },
    { '%', {0x62,0x64,0x08,0x13,0x23} }, { '-', {0x08,0x08,0x08,0x08,0x08} },
    { '/', {0x20,0x10,0x08,0x04,0x02} }, { ' ', {0,0,0,0,0} }
};

static bool utf8Next(const char* s, size_t len, size_t& i, uint32_t& cp) {
    if (i >= len) return false;
    uint8_t c = (uint8_t)s[i++];
    if (c < 0x80) { cp = c; return true; }
    if ((c & 0xE0) == 0xC0 && i < len) { cp=((c&0x1F)<<6)|((uint8_t)s[i++]&0x3F); return true; }
    if ((c & 0xF0) == 0xE0 && i+1 < len) { cp=((c&0x0F)<<12)|(((uint8_t)s[i++]&0x3F)<<6)|((uint8_t)s[i++]&0x3F); return true; }
    if ((c & 0xF8) == 0xF0 && i+2 < len) { cp=((c&7)<<18)|(((uint8_t)s[i++]&0x3F)<<12)|(((uint8_t)s[i++]&0x3F)<<6)|((uint8_t)s[i++]&0x3F); return true; }
    cp='?'; return true;
}

// Trả về chữ cái nền và loại dấu tiếng Việt.
static char vnBase(uint32_t cp, uint8_t& mark, bool& upper) {
    mark=0; upper=false;
    switch(cp) {
        case 0x0110: upper=true; return 'D'; case 0x0111: return 'd';
        case 0x0102: upper=true; return 'A'; case 0x0103: return 'a';
        case 0x00C2: upper=true; return 'A'; case 0x00E2: return 'a';
        case 0x00CA: upper=true; return 'E'; case 0x00EA: return 'e';
        case 0x00D4: upper=true; return 'O'; case 0x00F4: return 'o';
        case 0x01A0: upper=true; return 'O'; case 0x01A1: return 'o';
        case 0x01AF: upper=true; return 'U'; case 0x01B0: return 'u';
        case 0x00C0: case 0x00C1: case 0x1EA2: case 0x00C3: case 0x1EA0: upper=true; return 'A';
        case 0x00E0: case 0x00E1: case 0x1EA3: case 0x00E3: case 0x1EA1: return 'a';
        case 0x00C8: case 0x00C9: case 0x1EBA: case 0x1EBC: case 0x1EB8: upper=true; return 'E';
        case 0x00E8: case 0x00E9: case 0x1EBB: case 0x1EBD: case 0x1EB9: return 'e';
        case 0x00CC: case 0x00CD: case 0x1EC8: case 0x0128: case 0x1ECA: upper=true; return 'I';
        case 0x00EC: case 0x00ED: case 0x1EC9: case 0x0129: case 0x1ECB: return 'i';
        case 0x00D2: case 0x00D3: case 0x1ECE: case 0x00D5: case 0x1ECC: upper=true; return 'O';
        case 0x00F2: case 0x00F3: case 0x1ECF: case 0x00F5: case 0x1ECD: return 'o';
        case 0x00D9: case 0x00DA: case 0x1EE6: case 0x0168: case 0x1EE4: upper=true; return 'U';
        case 0x00F9: case 0x00FA: case 0x1EE7: case 0x0169: case 0x1EE5: return 'u';
        case 0x00DD: case 0x1EF6: case 0x1EF8: case 0x1EF4: upper=true; return 'Y';
        case 0x00FD: case 0x1EF7: case 0x1EF9: case 0x1EF5: return 'y';
    }
    // Các ký tự ă/â/ê/ô/ơ/ư có dấu thanh: nhận dạng theo bảng Unicode.
    if ((cp>=0x1E00 && cp<=0x1EFF)) {
        struct M { uint32_t c; char b; uint8_t m; bool u; };
        static const M m[] PROGMEM = {
            {0x1EAF,'a',1,0},{0x1EB1,'a',2,0},{0x1EB3,'a',3,0},{0x1EB5,'a',4,0},{0x1EB7,'a',5,0},
            {0x1EAE,'A',1,1},{0x1EB0,'A',2,1},{0x1EB2,'A',3,1},{0x1EB4,'A',4,1},{0x1EB6,'A',5,1},
            {0x1EA5,'a',1,0},{0x1EA7,'a',2,0},{0x1EA9,'a',3,0},{0x1EAB,'a',4,0},{0x1EAD,'a',5,0},
            {0x1EA4,'A',1,1},{0x1EA6,'A',2,1},{0x1EA8,'A',3,1},{0x1EAA,'A',4,1},{0x1EAC,'A',5,1},
            {0x1EBF,'e',1,0},{0x1EC1,'e',2,0},{0x1EC3,'e',3,0},{0x1EC5,'e',4,0},{0x1EC7,'e',5,0},
            {0x1EBE,'E',1,1},{0x1EC0,'E',2,1},{0x1EC2,'E',3,1},{0x1EC4,'E',4,1},{0x1EC6,'E',5,1},
            {0x1ED1,'o',1,0},{0x1ED3,'o',2,0},{0x1ED5,'o',3,0},{0x1ED7,'o',4,0},{0x1ED9,'o',5,0},
            {0x1ED0,'O',1,1},{0x1ED2,'O',2,1},{0x1ED4,'O',3,1},{0x1ED6,'O',4,1},{0x1ED8,'O',5,1},
            {0x1EDB,'o',1,0},{0x1EDD,'o',2,0},{0x1EDF,'o',3,0},{0x1EE1,'o',4,0},{0x1EE3,'o',5,0},
            {0x1EDA,'O',1,1},{0x1EDC,'O',2,1},{0x1EDE,'O',3,1},{0x1EE0,'O',4,1},{0x1EE2,'O',5,1},
            {0x1EE9,'u',1,0},{0x1EEB,'u',2,0},{0x1EED,'u',3,0},{0x1EEF,'u',4,0},{0x1EF1,'u',5,0},
            {0x1EE8,'U',1,1},{0x1EEA,'U',2,1},{0x1EEC,'U',3,1},{0x1EEE,'U',4,1},{0x1EF0,'U',5,1}
        };
        for (auto &x:m) { if (pgm_read_dword(&x.c)==cp) { mark=pgm_read_byte(&x.m); upper=pgm_read_byte(&x.u); return (char)pgm_read_byte(&x.b); } }
    }
    return 0;
}

static const uint8_t* glyph(uint32_t cp) {
    for (size_t i=0;i<sizeof(GLYPHS)/sizeof(GLYPHS[0]);++i) if (pgm_read_dword(&GLYPHS[i].cp)==cp) return GLYPHS[i].col;
    return nullptr;
}

static void drawGlyph(int x,int y,char ch,uint8_t mark=0) {
    const uint8_t* g=glyph((uint32_t)ch); if(!g) return;
    for(int c=0;c<5;c++) { uint8_t bits=pgm_read_byte(&g[c]); for(int r=0;r<7;r++) if(bits&(1<<r)) oled.drawPixel(x+c,y+r,SSD1306_WHITE); }
    if(mark) {
        // Dấu thanh nhỏ phía trên glyph; ưu tiên không che chữ.
        if(mark==1) oled.drawPixel(x+2,y-2,SSD1306_WHITE), oled.drawPixel(x+3,y-3,SSD1306_WHITE);
        else if(mark==2) oled.drawPixel(x+2,y-2,SSD1306_WHITE), oled.drawPixel(x+1,y-3,SSD1306_WHITE);
        else if(mark==3) oled.drawPixel(x+1,y-3,SSD1306_WHITE), oled.drawPixel(x+3,y-3,SSD1306_WHITE), oled.drawPixel(x+2,y-2,SSD1306_WHITE);
        else if(mark==4) oled.drawPixel(x+1,y-3,SSD1306_WHITE), oled.drawPixel(x+3,y-3,SSD1306_WHITE);
        else if(mark==5) oled.drawFastHLine(x+1,y-2,x+3,SSD1306_WHITE);
    }
}

static int drawVietnamese(int x,int y,const String& text) {
    size_t i=0,len=text.length();
    while(i<len) {
        uint32_t cp; utf8Next(text.c_str(),len,i,cp);
        uint8_t mark; bool upper; char base=vnBase(cp,mark,upper);
        if(base) cp=(uint32_t)base;
        const uint8_t* g=glyph(cp);
        if(g) { drawGlyph(x,y,(char)cp,mark); x+=6; }
        else { x+=6; }
    }
    return x;
}

bool displayInit() {
    s_oledOk = oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS);
    if (!s_oledOk) { Serial.println("[DISPLAY] Không tìm thấy màn hình OLED!"); return false; }
    oled.clearDisplay(); oled.setTextColor(SSD1306_WHITE); oled.display(); return true;
}

void displaySplashEffect() {
    if (!s_oledOk) return;
    oled.clearDisplay();
    drawVietnamese(3,14,"KHÔNG GIAN XANH");
    drawVietnamese(18,27,"Giám sát môi trường");
    oled.display();
    for(int x=0;x<=OLED_WIDTH;x+=8){ oled.drawFastVLine(x,58,6,SSD1306_WHITE); oled.display(); delay(15); }
}

static void drawAqiBar(int x,int y,int w,int h,uint8_t aqi) {
    oled.drawRect(x,y,w,h,SSD1306_WHITE); if(aqi==0)return; int fillW=(w-2)*aqi/5; oled.fillRect(x+1,y+1,fillW,h-2,SSD1306_WHITE);
}

void displayUpdate(const SensorData& data,const String& wifiStatus,const String& timeStr) {
    if(!s_oledOk)return;
    oled.clearDisplay();
    oled.drawFastHLine(0,10,OLED_WIDTH,SSD1306_WHITE);
    drawVietnamese(2,1,"KHÔNG GIAN XANH");
    if((millis()/500)%2==0) oled.fillCircle(122,4,2,SSD1306_WHITE);

    drawVietnamese(2,14,"Nhiệt độ");
    if(data.sht31Ok) { oled.setCursor(54,14); oled.printf("%.1fC",data.temperature); } else drawVietnamese(54,14,"Lỗi");
    drawVietnamese(2,26,"Độ ẩm");
    if(data.sht31Ok) { oled.setCursor(54,26); oled.printf("%.0f%%",data.humidity); } else drawVietnamese(54,26,"Lỗi");

    drawVietnamese(2,38,"AQI:");
    if(data.ens160Ok) drawVietnamese(26,38,data.aqiLabel); else drawVietnamese(26,38,"Lỗi");
    oled.setCursor(68,38); if(data.ens160Ok) oled.printf("CO2:%d",data.eco2); else oled.print("CO2:--");
    drawAqiBar(2,47,60,6,data.ens160Ok?data.aqi:0);

    oled.drawFastHLine(0,55,OLED_WIDTH,SSD1306_WHITE);
    drawVietnamese(2,57,wifiStatus);
    oled.setCursor(78,57); oled.print(timeStr);
    if(data.warning && (millis()/400)%2==0) oled.drawRect(0,0,OLED_WIDTH,OLED_HEIGHT,SSD1306_WHITE);
    oled.display();
}

void displayShowError(const String& message) {
    if(!s_oledOk)return;
    oled.clearDisplay(); drawVietnamese(0,20,"LỖI CẢM BIẾN:"); drawVietnamese(0,35,message); oled.display();
}
