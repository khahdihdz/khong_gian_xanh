# 🌐 Truy cập Không Gian Xanh từ Internet

## 1. Mô hình

ESP32 mặc định chạy web server HTTP trên TCP port `80` và nhận IP LAN từ router.

```text
Internet
   ↓
IP công khai của router
   ↓
Port Forward TCP
   ↓
ESP32 LAN:80
```

ESP32 **không tự cấu hình Port Forward/NAT**. Việc này phải cấu hình trên router.

## 2. Kiểm tra CGNAT

Trên router, xem `WAN/Internet IP` rồi so sánh với `IP công khai` hiển thị trong Dashboard.

Nếu WAN IP nằm trong các dải private/CGNAT như `10.0.0.0/8`, `172.16.0.0/12`, `192.168.0.0/16` hoặc `100.64.0.0/10`, port forwarding IPv4 thông thường có thể không hoạt động từ Internet.

## 3. Port Forward

Ví dụ ESP32 có IP `192.168.10.206`:

```text
Protocol: TCP
External port: 8080
Internal IP: 192.168.10.206
Internal port: 80
```

Sau đó URL sẽ có dạng:

```text
http://IP_CONG_KHAI:8080/
```

Không nên dùng port `80` public nếu router cho phép chọn port ngoài khác.

## 4. HTTPS và bảo vệ OTA

Không khuyến nghị mở trực tiếp ESP32 HTTP/OTA ra Internet trong môi trường production.

Đặc biệt `/update` cho phép nâng cấp firmware, vì vậy không nên expose endpoint này công khai nếu chưa có lớp xác thực và HTTPS phía trước.

Kiến trúc production nên là:

```text
Internet
   ↓
HTTPS + Authentication
   ↓
Reverse Proxy / VPN
   ↓
Router
   ↓
ESP32
```

## 5. IP động

Nếu ISP cấp IP động, dùng DDNS:

```text
esp32.example.com
       ↓
IP công khai hiện tại
       ↓
Router
       ↓
ESP32
```

Không lưu username/password WiFi hoặc MQTT vào URL public.

## 6. Kiểm tra từ mạng ngoài

Không kiểm tra bằng WiFi nội bộ. Hãy dùng 4G/5G của điện thoại hoặc một mạng Internet khác rồi mở URL public.

Nếu truy cập được trong LAN nhưng không truy cập được từ 4G/5G, kiểm tra lần lượt:

1. WAN IP / CGNAT.
2. Port Forward.
3. Firewall router.
4. Firewall/ISP chặn port.
5. IP LAN của ESP32 có bị DHCP đổi hay không.

## 7. OTA

OTA hiện được cung cấp tại `/update`. Chỉ sử dụng OTA qua mạng tin cậy hoặc qua lớp HTTPS/VPN có xác thực.
