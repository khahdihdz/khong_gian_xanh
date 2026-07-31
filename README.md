# 🌿 Không Gian Xanh

### Hệ thống ESP32 giám sát môi trường phòng thời gian thực

Hệ thống theo dõi nhiệt độ, độ ẩm và chất lượng không khí trong phòng,
hiển thị trên màn hình OLED và Web Dashboard realtime (WebSocket),
kèm cảnh báo LED/Buzzer, lưu lịch sử, xuất CSV, và cập nhật firmware OTA.

💛 Dashboard có nút **"Ủng hộ"** liên kết tới [khahdihdz.github.io](https://khahdihdz.github.io) nếu bạn muốn ủng hộ tác giả.

---

## 1. Sơ đồ nối dây

```
                         ESP32 DevKit V1 (30 chân)
                        ┌───────────────────────┐
                        │                       │
      DHT22             │                       │
    ┌──────┐             │                       │
    │  VCC │───────────► │ 3V3                   │
    │  DATA│───────────► │ GPIO 4                │
    │  GND │───────────► │ GND                   │
    └──────┘             │                       │
   (thêm điện trở kéo lên│                       │
    10kΩ giữa VCC-DATA)  │                       │
                        │                       │
      OLED SSD1306      │                       │
      (I2C 128x64)      │                       │
    ┌──────┐             │                       │
    │  VCC │───────────► │ 3V3                   │
    │  GND │───────────► │ GND                   │
    │  SDA │───────────► │ GPIO 21 (SDA)         │
    │  SCL │───────────► │ GPIO 22 (SCL)         │
    └──────┘             │                       │
                        │        (I2C dùng chung)│
      ENS160             │                       │
    ┌──────┐             │                       │
    │  VCC │───────────► │ 3V3                   │
    │  GND │───────────► │ GND                   │
    │  SDA │───────────► │ GPIO 21 (SDA)         │
    │  SCL │───────────► │ GPIO 22 (SCL)         │
    └──────┘             │                       │
                        │                       │
      LED trạng thái     │                       │
    ┌──────┐             │                       │
    │  A(+)│──[220Ω]───► │ GPIO 25               │
    │  K(-)│───────────► │ GND                   │
    └──────┘             │                       │
                        │                       │
      Buzzer (còi)       │                       │
    ┌──────┐             │                       │
    │  (+) │───────────► │ GPIO 26               │
    │  (-) │───────────► │ GND                   │
    └──────┘             │                       │
                        │                       │
      LED WiFi = LED onboard GPIO 2 (có sẵn trên board, không cần nối)
                        └───────────────────────┘
```

**Ghi chú quan trọng:**
- OLED và ENS160 dùng chung bus I2C (SDA=GPIO21, SCL=GPIO22) — đấu song song hai cảm biến vào cùng 2 dây SDA/SCL.
- Nếu ENS160 và OLED trùng địa chỉ I2C, kiểm tra lại địa chỉ bằng I2C scanner (ENS160 thường là `0x53`, một số module là `0x52`; code đã tự dò cả hai).
- Với DHT22 nên thêm điện trở kéo lên (pull-up) 10kΩ giữa chân VCC và DATA nếu module không có sẵn.
- Tất cả cảm biến dùng nguồn **3.3V** (không dùng 5V) vì ESP32 GPIO không chịu được 5V.

---

## 2. Danh sách thư viện cần cài

| Thư viện | Tác giả | Ghi chú |
|---|---|---|
| DHT sensor library | Adafruit | Đọc DHT22 |
| Adafruit Unified Sensor | Adafruit | Phụ thuộc của DHT library |
| Adafruit GFX Library | Adafruit | Vẽ đồ hoạ OLED |
| Adafruit SSD1306 | Adafruit | Điều khiển màn hình OLED |
| SparkFun Indoor Air Quality Sensor - ENS160 Arduino Library | SparkFun | Đọc TVOC/eCO2/AQI (cài qua link GitHub, xem lưu ý bên dưới) |
| ESPAsyncWebServer | ESP32Async (fork mới) | Web server + WebSocket bất đồng bộ |
| AsyncTCP | ESP32Async (fork mới) | Phụ thuộc bắt buộc của ESPAsyncWebServer |
| ElegantOTA | ayushsharma82 | Cập nhật firmware qua web tại `/update` |

> **Lưu ý:** dùng bản fork **ESP32Async/ESPAsyncWebServer** và **ESP32Async/AsyncTCP**
> (bản mới nhất, tương thích ESP32 core 3.x). Bản cũ của `me-no-dev` đã ngừng cập nhật
> và có thể lỗi biên dịch với Arduino core mới.

> **Lưu ý về SparkFun ENS160:** thư viện này chưa được đăng ký lên PlatformIO Registry
> (chỉ tồn tại dưới dạng Arduino Library trên GitHub), nên `platformio.ini` khai báo
> trực tiếp bằng link Git thay vì tên registry:
> ```ini
> https://github.com/sparkfun/SparkFun_Indoor_Air_Quality_Sensor-ENS160_Arduino_Library.git
> ```
> Nếu dùng Arduino IDE, tìm "ENS160" trong Library Manager (chọn bản của **SparkFun Electronics**)
> hoặc tải .zip trực tiếp từ GitHub rồi **Add .ZIP Library**.

### Cài bằng PlatformIO (khuyến nghị)
Đã khai báo sẵn trong `platformio.ini`, chỉ cần build là PlatformIO tự tải thư viện.

### Cài bằng Arduino IDE
Mở **Sketch → Include Library → Manage Libraries**, tìm và cài từng thư viện theo tên ở bảng trên.
Với ESPAsyncWebServer/AsyncTCP, nếu không tìm thấy bản đúng trên Library Manager,
tải trực tiếp từ GitHub (`ESP32Async/ESPAsyncWebServer`, `ESP32Async/AsyncTCP`) rồi
**Sketch → Include Library → Add .ZIP Library**.

Cần thêm plugin **ESP32 LittleFS Data Upload** để tải file trong thư mục `data/` lên ESP32
(xem bước 4 bên dưới).

---

## 3. Cấu trúc dự án

```
khong_gian_xanh/
├── platformio.ini          # Cấu hình PlatformIO (board, thư viện, partition)
├── README.md                # File này
├── src/                      # Toàn bộ mã nguồn C++ (PlatformIO build từ đây)
│   ├── main.cpp              # Vòng lặp chính, điều phối các module
│   ├── config.h               # Cấu hình chân, ngưỡng cảnh báo, hằng số
│   ├── sensor.h / sensor.cpp   # Đọc DHT22 + ENS160, phân loại AQI, tự hồi phục lỗi
│   ├── display.h / display.cpp # Vẽ giao diện OLED SSD1306
│   ├── wifi_manager.h/.cpp     # Quản lý WiFi STA/AP, lưu Preferences, tự reconnect
│   ├── web_server.h/.cpp       # Web server, WebSocket, REST API, OTA
│   └── storage.h / storage.cpp # Ring-buffer lịch sử 1000 bản ghi, xuất JSON/CSV
└── data/                     # Nội dung upload lên LittleFS (giao diện web)
    ├── index.html             # Dashboard chính - CHỈ hiển thị theo dõi (Bootstrap 5 + Chart.js, dark mode, có nút "❤️ Ủng hộ")
    ├── tools.html              # Trang chức năng: xuất CSV, cấu hình WiFi, OTA, thông tin thiết bị
    └── wifi_config.html        # Trang cấu hình WiFi (mở từ tools.html)
```

> Nếu dùng **Arduino IDE** thay vì PlatformIO: tạo 1 thư mục sketch tên `khong_gian_xanh`,
> copy toàn bộ các file `.cpp/.h` trong `src/` vào thẳng thư mục sketch đó (Arduino IDE không
> cần thư mục `src/` con), đổi `main.cpp` thành `khong_gian_xanh.ino`. Thư mục `data/` giữ
> nguyên tên và vị trí (ngang hàng với file `.ino`) để công cụ upload LittleFS nhận diện.

---

## 4. Hướng dẫn nạp code

### Cách A — PlatformIO (khuyến nghị)

```bash
# 1. Build và nạp firmware
pio run --target upload

# 2. Nạp giao diện web (LittleFS) — BẮT BUỘC, nếu không dashboard sẽ không hiện
pio run --target uploadfs

# 3. Mở Serial Monitor để xem log / lấy địa chỉ IP
pio device monitor
```

⚠️ **Thứ tự quan trọng:** luôn chạy `uploadfs` sau khi `upload`, nếu không trang web sẽ trả về lỗi 404.

### Cách B — Arduino IDE

1. Cài ESP32 board package (`esp32` by Espressif Systems) qua Boards Manager.
2. Chọn board: **ESP32 Dev Module**.
3. Chọn **Partition Scheme: Minimal SPIFFS (1.9MB APP / 190KB SPIFFS)** hoặc bất kỳ scheme nào có chừa dung lượng cho OTA — bắt buộc vì firmware dùng ElegantOTA cần vùng nhớ dự phòng để nhận file .bin mới.
4. Cài plugin **ESP32 LittleFS Data Upload** (Tools → ESP32 Sketch Data Upload) để tải thư mục `data/`.
5. Nạp code (Upload), sau đó chạy **Tools → ESP32 Sketch Data Upload** để tải giao diện web.

---

## 5. Sử dụng lần đầu

1. Cấp nguồn cho ESP32. Vì chưa có WiFi lưu sẵn, thiết bị tự phát WiFi:
   - **SSID:** `KhongGianXanh-Setup`
   - **Mật khẩu:** `12345678`
2. Dùng điện thoại/máy tính kết nối vào mạng trên.
3. Mở trình duyệt, truy cập `http://192.168.4.1/wifi_config.html`, nhập SSID/mật khẩu WiFi nhà bạn.
4. ESP32 sẽ thử kết nối; nếu thành công, xem địa chỉ IP mới qua Serial Monitor và truy cập dashboard tại địa chỉ đó.
5. Nếu không thành công sau ~15 giây, thiết bị tự quay lại chế độ AP để cấu hình lại.

## 6. API tham khảo

| Endpoint | Method | Mô tả |
|---|---|---|
| `/api/data` | GET | Dữ liệu cảm biến hiện tại (JSON) |
| `/api/history?hours=1\|6\|12\|24\|0` | GET | Lịch sử theo khung giờ (0 = toàn bộ) |
| `/api/history/csv` | GET | Tải lịch sử dạng CSV |
| `/api/info` | GET | Thông tin thiết bị (RAM trống, uptime, IP, RSSI...) |
| `/api/wifi-config` | POST (form: `ssid`, `password`) | Lưu WiFi mới |
| `/api/wifi-reset` | POST | Xoá WiFi đã lưu, quay về chế độ AP |
| `/update` | GET | Trang cập nhật firmware OTA (ElegantOTA) |
| `/ws` | WebSocket | Đẩy dữ liệu realtime mỗi 2 giây |

## 7. Tự động Build & Release (GitHub Actions)

Mỗi khi push code lên nhánh `main` (trừ khi chỉ sửa file `.md`), GitHub Actions sẽ tự động:

1. Build firmware bằng PlatformIO (`pio run`).
2. Build ảnh hệ thống file LittleFS chứa giao diện web (`pio run -t buildfs`).
3. Tạo một **Release** mới trên GitHub, gắn tag dạng `v<phiên_bản>-<mã_commit>`.
4. Đính kèm 2 file `.bin`:
   - `khong-gian-xanh-firmware-v<version>.bin` — firmware chính.
   - `khong-gian-xanh-littlefs-v<version>.bin` — ảnh giao diện web (LittleFS).

File workflow: `.github/workflows/release.yml`.

### Cách tăng số phiên bản
Sửa dòng `FIRMWARE_VERSION` trong `src/config.h` trước khi push:
```cpp
#define FIRMWARE_VERSION "1.0.1"
```

### Cách nạp file `.bin` tải từ trang Releases

**Firmware (`khong-gian-xanh-firmware-*.bin`) — có 2 cách:**
- **OTA qua web (khuyến nghị, không cần dây):** vào trang **Chức năng → Cập nhật Firmware**
  (`/update`) trên dashboard, chọn file `.bin` vừa tải, bấm Upload.
- **Qua USB:** dùng `esptool.py`:
  ```bash
  esptool.py --chip esp32 --port <COM_PORT> --baud 921600 write_flash 0x10000 khong-gian-xanh-firmware-v1.0.0.bin
  ```

**Filesystem (`khong-gian-xanh-littlefs-*.bin`) — chỉ cần nạp lại khi giao diện web thay đổi:**
- Cách đơn giản nhất: build từ mã nguồn rồi chạy `pio run --target uploadfs`.
- Hoặc nạp trực tiếp qua USB bằng `esptool.py` (offset phụ thuộc bảng phân vùng
  `min_spiffs.csv` đang dùng, thường là `0x3D0000` — nên kiểm tra lại bằng lệnh
  `pio run -t uploadfs --verbose` trên máy có mã nguồn để lấy offset chính xác trước khi flash):
  ```bash
  esptool.py --chip esp32 --port <COM_PORT> --baud 921600 write_flash 0x3D0000 khong-gian-xanh-littlefs-v1.0.0.bin
  ```
- Trang OTA (`/update`) hiện chỉ hỗ trợ cập nhật firmware, chưa hỗ trợ cập nhật filesystem qua mạng.

## 8. Ngưỡng cảnh báo mặc định (chỉnh trong `config.h`)

- Nhiệt độ > 35°C
- Độ ẩm > 80%
- AQI ≥ 4 (Kém trở lên)
- TVOC ≥ 500 ppb
- eCO2 ≥ 1200 ppm
