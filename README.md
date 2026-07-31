# 🌿 Không Gian Xanh

### Hệ thống ESP32 giám sát môi trường phòng thời gian thực

Hệ thống theo dõi nhiệt độ, độ ẩm và chất lượng không khí trong phòng,
hiển thị trên màn hình OLED và Web Dashboard realtime (WebSocket),
kèm cảnh báo LED/Buzzer, lưu lịch sử, xuất CSV, và cập nhật firmware OTA.

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
| SparkFun ENS160 | SparkFun | Đọc TVOC/eCO2/AQI |
| ESPAsyncWebServer | ESP32Async (fork mới) | Web server + WebSocket bất đồng bộ |
| AsyncTCP | ESP32Async (fork mới) | Phụ thuộc bắt buộc của ESPAsyncWebServer |
| ElegantOTA | ayushsharma82 | Cập nhật firmware qua web tại `/update` |

> **Lưu ý:** dùng bản fork **ESP32Async/ESPAsyncWebServer** và **ESP32Async/AsyncTCP**
> (bản mới nhất, tương thích ESP32 core 3.x). Bản cũ của `me-no-dev` đã ngừng cập nhật
> và có thể lỗi biên dịch với Arduino core mới.

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
    ├── index.html             # Dashboard chính - CHỈ hiển thị theo dõi (Bootstrap 5 + Chart.js, dark mode)
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

## 7. Ngưỡng cảnh báo mặc định (chỉnh trong `config.h`)

- Nhiệt độ > 35°C
- Độ ẩm > 80%
- AQI ≥ 4 (Kém trở lên)
- TVOC ≥ 500 ppb
- eCO2 ≥ 1200 ppm
