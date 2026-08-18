#include "display.h"
#include "config.h"

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

static Adafruit_SSD1306 oled(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);
static bool s_oledOk = false;

struct Glyph5x7 { char c; uint8_t p[5]; };

static const Glyph5x7 FONT[] PROGMEM = {
  {'A',{0x7E,0x09,0x09,0x09,0x7E}},{'B',{0x7F,0x49,0x49,0x49,0x36}},
  {'C',{0x3E,0x41,0x41,0x41,0x22}},{'D',{0x7F,0x41,0x41,0x22,0x1C}},
  {'E',{0x7F,0x49,0x49,0x49,0x41}},{'F',{0x7F,0x09,0x09,0x09,0x01}},
  {'G',{0x3E,0x41,0x49,0x49,0x7A}},{'H',{0x7F,0x08,0x08,0x08,0x7F}},
  {'I',{0x41,0x41,0x7F,0x41,0x41}},{'J',{0x20,0x40,0x41,0x3F,0x01}},
  {'K',{0x7F,0x08,0x14,0x22,0x41}},{'L',{0x7F,0x40,0x40,0x40,0x40}},
  {'M',{0x7F,0x02,0x0C,0x02,0x7F}},{'N',{0x7F,0x04,0x08,0x10,0x7F}},
  {'O',{0x3E,0x41,0x41,0x41,0x3E}},{'P',{0x7F,0x09,0x09,0x09,0x06}},
  {'Q',{0x3E,0x41,0x51,0x21,0x5E}},{'R',{0x7F,0x09,0x19,0x29,0x46}},
  {'S',{0x46,0x49,0x49,0x49,0x31}},{'T',{0x01,0x01,0x7F,0x01,0x01}},
  {'U',{0x3F,0x40,0x40,0x40,0x3F}},{'V',{0x1F,0x20,0x40,0x20,0x1F}},
  {'W',{0x3F,0x40,0x38,0x40,0x3F}},{'X',{0x63,0x14,0x08,0x14,0x63}},
  {'Y',{0x07,0x08,0x70,0x08,0x07}},{'Z',{0x61,0x51,0x49,0x45,0x43}},
  {'a',{0x20,0x54,0x54,0x54,0x78}},{'b',{0x7F,0x48,0x44,0x44,0x38}},
  {'c',{0x38,0x44,0x44,0x44,0x20}},{'d',{0x38,0x44,0x44,0x48,0x7F}},
  {'e',{0x38,0x54,0x54,0x54,0x18}},{'f',{0x08,0x7E,0x09,0x01,0x02}},
  {'g',{0x18,0xA4,0xA4,0xA4,0x7C}},{'h',{0x7F,0x08,0x04,0x04,0x78}},
  {'i',{0x00,0x44,0x7D,0x40,0x00}},{'j',{0x40,0x80,0x84,0x7D,0x00}},
  {'k',{0x7F,0x10,0x28,0x44,0x00}},{'l',{0x00,0x41,0x7F,0x40,0x00}},
  {'m',{0x7C,0x04,0x18,0x04,0x78}},{'n',{0x7C,0x08,0x04,0x04,0x78}},
  {'o',{0x38,0x44,0x44,0x44,0x38}},{'p',{0xFC,0x24,0x24,0x24,0x18}},
  {'q',{0x18,0x24,0x24,0x24,0xFC}},{'r',{0x7C,0x08,0x04,0x04,0x08}},
  {'s',{0x48,0x54,0x54,0x54,0x24}},{'t',{0x04,0x3F,0x44,0x40,0x20}},
  {'u',{0x3C,0x40,0x40,0x20,0x7C}},{'v',{0x1C,0x20,0x40,0x20,0x1C}},
  {'w',{0x3C,0x40,0x30,0x40,0x3C}},{'x',{0x44,0x28,0x10,0x28,0x44}},
  {'y',{0x1C,0xA0,0xA0,0xA0,0x7C}},{'z',{0x44,0x64,0x54,0x4C,0x44}},
  {'0',{0x3E,0x51,0x49,0x45,0x3E}},{'1',{0x00,0x42,0x7F,0x40,0x00}},
  {'2',{0x42,0x61,0x51,0x49,0x46}},{'3',{0x21,0x41,0x45,0x4B,0x31}},
  {'4',{0x18,0x14,0x12,0x7F,0x10}},{'5',{0x27,0x45,0x45,0x45,0x39}},
  {'6',{0x3C,0x4A,0x49,0x49,0x30}},{'7',{0x01,0x71,0x09,0x05,0x03}},
  {'8',{0x36,0x49,0x49,0x49,0x36}},{'9',{0x06,0x49,0x49,0x29,0x1E}},
  {'.',{0x00,0x60,0x60,0x00,0x00}},{':',{0x00,0x36,0x36,0x00,0x00}},
  {'%',{0x62,0x64,0x08,0x13,0x23}},{'-',{0x08,0x08,0x08,0x08,0x08}},
  {'/',{0x20,0x10,0x08,0x04,0x02}},{'?',{0x02,0x01,0x51,0x09,0x06}},
  {' ',{0,0,0,0,0}}
};

static const uint8_t* getGlyph(char c) {
  for (size_t i=0;i<sizeof(FONT)/sizeof(FONT[0]);++i)
    if ((char)pgm_read_byte(&FONT[i].c)==c) return FONT[i].p;
  return nullptr;
}

static bool decodeVn(uint32_t cp,char& base,uint8_t& form,uint8_t& tone) {
  base=(char)cp; form=0; tone=0;
  switch(cp) {
    case 0x0110: base='D'; return true; case 0x0111: base='d'; return true;
    case 0x0102: base='A'; form=1; return true; case 0x0103: base='a'; form=1; return true;
    case 0x00C2: base='A'; form=2; return true; case 0x00E2: base='a'; form=2; return true;
    case 0x00CA: base='E'; form=2; return true; case 0x00EA: base='e'; form=2; return true;
    case 0x00D4: base='O'; form=2; return true; case 0x00F4: base='o'; form=2; return true;
    case 0x01A0: base='O'; form=3; return true; case 0x01A1: base='o'; form=3; return true;
    case 0x01AF: base='U'; form=3; return true; case 0x01B0: base='u'; form=3; return true;

    case 0x00C1: base='A'; tone=1; return true; case 0x00C0: base='A'; tone=2; return true; case 0x1EA2: base='A'; tone=3; return true; case 0x00C3: base='A'; tone=4; return true; case 0x1EA0: base='A'; tone=5; return true;
    case 0x00E1: base='a'; tone=1; return true; case 0x00E0: base='a'; tone=2; return true; case 0x1EA3: base='a'; tone=3; return true; case 0x00E3: base='a'; tone=4; return true; case 0x1EA1: base='a'; tone=5; return true;
    case 0x00C9: base='E'; tone=1; return true; case 0x00C8: base='E'; tone=2; return true; case 0x1EBA: base='E'; tone=3; return true; case 0x1EBC: base='E'; tone=4; return true; case 0x1EB8: base='E'; tone=5; return true;
    case 0x00E9: base='e'; tone=1; return true; case 0x00E8: base='e'; tone=2; return true; case 0x1EBB: base='e'; tone=3; return true; case 0x1EBD: base='e'; tone=4; return true; case 0x1EB9: base='e'; tone=5; return true;
    case 0x00CD: base='I'; tone=1; return true; case 0x00CC: base='I'; tone=2; return true; case 0x1EC8: base='I'; tone=3; return true; case 0x0128: base='I'; tone=4; return true; case 0x1ECA: base='I'; tone=5; return true;
    case 0x00ED: base='i'; tone=1; return true; case 0x00EC: base='i'; tone=2; return true; case 0x1EC9: base='i'; tone=3; return true; case 0x0129: base='i'; tone=4; return true; case 0x1ECB: base='i'; tone=5; return true;
    case 0x00D3: base='O'; tone=1; return true; case 0x00D2: base='O'; tone=2; return true; case 0x1ECE: base='O'; tone=3; return true; case 0x00D5: base='O'; tone=4; return true; case 0x1ECC: base='O'; tone=5; return true;
    case 0x00F3: base='o'; tone=1; return true; case 0x00F2: base='o'; tone=2; return true; case 0x1ECF: base='o'; tone=3; return true; case 0x00F5: base='o'; tone=4; return true; case 0x1ECD: base='o'; tone=5; return true;
    case 0x00DA: base='U'; tone=1; return true; case 0x00D9: base='U'; tone=2; return true; case 0x1EE6: base='U'; tone=3; return true; case 0x0168: base='U'; tone=4; return true; case 0x1EE4: base='U'; tone=5; return true;
    case 0x00FA: base='u'; tone=1; return true; case 0x00F9: base='u'; tone=2; return true; case 0x1EE7: base='u'; tone=3; return true; case 0x0169: base='u'; tone=4; return true; case 0x1EE5: base='u'; tone=5; return true;
    case 0x00DD: base='Y'; tone=1; return true; case 0x1EF2: base='Y'; tone=2; return true; case 0x1EF6: base='Y'; tone=3; return true; case 0x1EF8: base='Y'; tone=4; return true; case 0x1EF4: base='Y'; tone=5; return true;
    case 0x00FD: base='y'; tone=1; return true; case 0x1EF3: base='y'; tone=2; return true; case 0x1EF7: base='y'; tone=3; return true; case 0x1EF9: base='y'; tone=4; return true; case 0x1EF5: base='y'; tone=5; return true;

    case 0x1EA4: base='A'; form=2; tone=1; return true; case 0x1EA6: base='A'; form=2; tone=2; return true; case 0x1EA8: base='A'; form=2; tone=3; return true; case 0x1EAA: base='A'; form=2; tone=4; return true; case 0x1EAC: base='A'; form=2; tone=5; return true;
    case 0x1EA5: base='a'; form=2; tone=1; return true; case 0x1EA7: base='a'; form=2; tone=2; return true; case 0x1EA9: base='a'; form=2; tone=3; return true; case 0x1EAB: base='a'; form=2; tone=4; return true; case 0x1EAD: base='a'; form=2; tone=5; return true;
    case 0x1EAE: base='A'; form=1; tone=1; return true; case 0x1EB0: base='A'; form=1; tone=2; return true; case 0x1EB2: base='A'; form=1; tone=3; return true; case 0x1EB4: base='A'; form=1; tone=4; return true; case 0x1EB6: base='A'; form=1; tone=5; return true;
    case 0x1EAF: base='a'; form=1; tone=1; return true; case 0x1EB1: base='a'; form=1; tone=2; return true; case 0x1EB3: base='a'; form=1; tone=3; return true; case 0x1EB5: base='a'; form=1; tone=4; return true; case 0x1EB7: base='a'; form=1; tone=5; return true;
    case 0x1EBE: base='E'; form=2; tone=1; return true; case 0x1EC0: base='E'; form=2; tone=2; return true; case 0x1EC2: base='E'; form=2; tone=3; return true; case 0x1EC4: base='E'; form=2; tone=4; return true; case 0x1EC6: base='E'; form=2; tone=5; return true;
    case 0x1EBF: base='e'; form=2; tone=1; return true; case 0x1EC1: base='e'; form=2; tone=2; return true; case 0x1EC3: base='e'; form=2; tone=3; return true; case 0x1EC5: base='e'; form=2; tone=4; return true; case 0x1EC7: base='e'; form=2; tone=5; return true;
    case 0x1ED0: base='O'; form=2; tone=1; return true; case 0x1ED2: base='O'; form=2; tone=2; return true; case 0x1ED4: base='O'; form=2; tone=3; return true; case 0x1ED6: base='O'; form=2; tone=4; return true; case 0x1ED8: base='O'; form=2; tone=5; return true;
    case 0x1ED1: base='o'; form=2; tone=1; return true; case 0x1ED3: base='o'; form=2; tone=2; return true; case 0x1ED5: base='o'; form=2; tone=3; return true; case 0x1ED7: base='o'; form=2; tone=4; return true; case 0x1ED9: base='o'; form=2; tone=5; return true;
    case 0x1EDA: base='O'; form=3; tone=1; return true; case 0x1EDC: base='O'; form=3; tone=2; return true; case 0x1EDE: base='O'; form=3; tone=3; return true; case 0x1EE0: base='O'; form=3; tone=4; return true; case 0x1EE2: base='O'; form=3; tone=5; return true;
    case 0x1EDB: base='o'; form=3; tone=1; return true; case 0x1EDD: base='o'; form=3; tone=2; return true; case 0x1EDF: base='o'; form=3; tone=3; return true; case 0x1EE1: base='o'; form=3; tone=4; return true; case 0x1EE3: base='o'; form=3; tone=5; return true;
    case 0x1EE8: base='U'; form=3; tone=1; return true; case 0x1EEA: base='U'; form=3; tone=2; return true; case 0x1EEC: base='U'; form=3; tone=3; return true; case 0x1EEE: base='U'; form=3; tone=4; return true; case 0x1EF0: base='U'; form=3; tone=5; return true;
    case 0x1EE9: base='u'; form=3; tone=1; return true; case 0x1EEB: base='u'; form=3; tone=2; return true; case 0x1EED: base='u'; form=3; tone=3; return true; case 0x1EEF: base='u'; form=3; tone=4; return true; case 0x1EF1: base='u'; form=3; tone=5; return true;
  }
  return false;
}

static void drawVnChar(int x,int y,char base,uint8_t form,uint8_t tone) {
  const uint8_t* g=getGlyph(base); if (!g) g=getGlyph('?'); if (!g) return;
  for (int col=0;col<5;++col) { uint8_t bits=pgm_read_byte(&g[col]); for (int row=0;row<7;++row) if (bits&(1<<row)) oled.drawPixel(x+col,y+row,SSD1306_WHITE); }
  if (base=='D'||base=='d') oled.drawFastHLine(x+1,y+3,4,SSD1306_WHITE);
  if (form==1||form==2) { oled.drawPixel(x+1,y-1,SSD1306_WHITE); oled.drawPixel(x+2,y-2,SSD1306_WHITE); oled.drawPixel(x+3,y-1,SSD1306_WHITE); }
  else if (form==3) { oled.drawPixel(x+3,y-1,SSD1306_WHITE); oled.drawPixel(x+4,y-2,SSD1306_WHITE); oled.drawPixel(x+4,y-1,SSD1306_WHITE); }
  const int cx=x+2;
  if (tone==1) { oled.drawPixel(cx,y-4,SSD1306_WHITE); oled.drawPixel(cx+1,y-3,SSD1306_WHITE); }
  else if (tone==2) { oled.drawPixel(cx,y-3,SSD1306_WHITE); oled.drawPixel(cx-1,y-4,SSD1306_WHITE); }
  else if (tone==3) { oled.drawPixel(cx-1,y-4,SSD1306_WHITE); oled.drawPixel(cx,y-3,SSD1306_WHITE); oled.drawPixel(cx+1,y-4,SSD1306_WHITE); }
  else if (tone==4) { oled.drawPixel(cx-1,y-4,SSD1306_WHITE); oled.drawPixel(cx,y-3,SSD1306_WHITE); oled.drawPixel(cx+1,y-4,SSD1306_WHITE); }
  else if (tone==5) { oled.drawPixel(cx,y+7,SSD1306_WHITE); oled.drawPixel(cx+1,y+7,SSD1306_WHITE); }
}

static int drawVietnamese(int x,int y,const String& text) {
  size_t i=0,len=text.length();
  while (i<len) {
    uint32_t cp=0; uint8_t c=(uint8_t)text[i];
    if (c<0x80) { cp=c; ++i; }
    else if ((c&0xE0)==0xC0&&i+1<len) { cp=((c&0x1F)<<6)|((uint8_t)text[i+1]&0x3F); i+=2; }
    else if ((c&0xF0)==0xE0&&i+2<len) { cp=((c&0x0F)<<12)|(((uint8_t)text[i+1]&0x3F)<<6)|((uint8_t)text[i+2]&0x3F); i+=3; }
    else { cp='?'; ++i; }
    char base=(char)cp; uint8_t form=0,tone=0; decodeVn(cp,base,form,tone); drawVnChar(x,y,base,form,tone); x+=6;
  }
  return x;
}

bool displayInit() {
  s_oledOk=oled.begin(SSD1306_SWITCHCAPVCC,OLED_ADDRESS);
  if (!s_oledOk) { Serial.println("[DISPLAY] Không tìm thấy màn hình OLED!"); return false; }
  oled.clearDisplay(); oled.setTextColor(SSD1306_WHITE); oled.display(); return true;
}

void displaySplashEffect() {
  if (!s_oledOk) return;
  oled.clearDisplay();
  drawVietnamese(3,8,"KHÔNG GIAN XANH");
  drawVietnamese(7,21,"Giám sát môi trường");
  oled.drawFastHLine(3,32,122,SSD1306_WHITE);
  oled.display(); delay(250);
}

static void drawAqiBar(int x,int y,int w,int h,uint8_t aqi) {
  oled.drawRect(x,y,w,h,SSD1306_WHITE);
  if (aqi==0) return;
  int fill=(w-2)*((aqi>5)?5:aqi)/5;
  if (fill>0) oled.fillRect(x+1,y+1,fill,h-2,SSD1306_WHITE);
}

void displayUpdate(const SensorData& data,const String& wifiStatus,const String& timeStr) {
  if (!s_oledOk) return;
  oled.clearDisplay();

  // Bố cục 128x64: chừa vùng trên cho dấu thanh và vùng dưới cho dấu nặng.
  drawVietnamese(2,6,"KHÔNG GIAN XANH");
  if ((millis()/500)%2==0) oled.fillCircle(124,9,2,SSD1306_WHITE);

  drawVietnamese(2,17,"Nhiệt độ");
  if (data.sht31Ok) { oled.setCursor(56,17); oled.printf("%.1fC",data.temperature); }
  else drawVietnamese(56,17,"Lỗi");

  drawVietnamese(2,28,"Độ ẩm");
  if (data.sht31Ok) { oled.setCursor(56,28); oled.printf("%.0f%%",data.humidity); }
  else drawVietnamese(56,28,"Lỗi");

  drawVietnamese(2,39,"AQI:");
  if (data.ens160Ok) drawVietnamese(26,39,data.aqiLabel); else drawVietnamese(26,39,"Lỗi");

  // CO2 được đẩy sang phải để không chồng lên "AQI: Rất tốt".
  oled.setCursor(84,39);
  if (data.ens160Ok) oled.printf("CO2:%u",data.eco2); else oled.print("CO2:--");

  // Thanh AQI nằm riêng giữa vùng dữ liệu và trạng thái kết nối.
  drawAqiBar(2,48,60,5,data.ens160Ok?data.aqi:0);

  // Dòng cuối đủ thấp để dấu nặng không chạm thanh AQI.
  drawVietnamese(2,56,wifiStatus);
  oled.setCursor(76,56); oled.print(timeStr);

  if (data.warning&&(millis()/400)%2==0) oled.drawRect(0,0,OLED_WIDTH-1,OLED_HEIGHT-1,SSD1306_WHITE);
  oled.display();
}

void displayShowError(const String& message) {
  if (!s_oledOk) return;
  oled.clearDisplay();
  drawVietnamese(2,15,"Lỗi cảm biến:");
  drawVietnamese(2,30,message);
  oled.display();
}
