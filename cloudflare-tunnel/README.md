# Public ESP32 qua Cloudflare Tunnel

Kiến trúc:

```text
Internet
   |
   v
Cloudflare HTTPS
   |
   v
cloudflared (PC/server luôn bật trong cùng LAN)
   |
   v
http://192.168.110.206:80
   |
   v
ESP32 - Không Gian Xanh
```

ESP32 không chạy `cloudflared`. Một PC/server/Raspberry Pi trong cùng mạng LAN phải chạy connector.

## 1. Tạo tunnel

Cài `cloudflared` trên máy luôn bật trong LAN, đăng nhập Cloudflare rồi tạo tunnel:

```bash
cloudflared tunnel login
cloudflared tunnel create khong-gian-xanh
```

Ghi lại Tunnel UUID và file credentials được tạo.

## 2. Tạo hostname công khai

Ví dụ muốn dùng:

```text
https://khonggianxanh.example.com
```

Route DNS:

```bash
cloudflared tunnel route dns khong-gian-xanh khonggianxanh.example.com
```

## 3. Cấu hình tunnel

Tạo `~/.cloudflared/config.yml`:

```yaml
tunnel: YOUR-TUNNEL-UUID
credentials-file: /home/YOUR_USER/.cloudflared/YOUR-TUNNEL-UUID.json

ingress:
  - hostname: khonggianxanh.example.com
    service: http://192.168.110.206:80
  - service: http_status:404
```

Thay `YOUR-TUNNEL-UUID`, đường dẫn credentials và hostname thật của bạn.

Kiểm tra:

```bash
cloudflared tunnel ingress validate
cloudflared tunnel ingress rule https://khonggianxanh.example.com
```

Chạy:

```bash
cloudflared tunnel run khong-gian-xanh
```

Sau đó truy cập:

```text
https://khonggianxanh.example.com/
```

Không cần mở port 80/443 trên router.

## 4. Bảo vệ OTA

`/update` là giao diện nạp firmware/filesystem. Không nên để endpoint này công khai không xác thực.

Khuyến nghị tạo Cloudflare Access Application cho hostname và yêu cầu đăng nhập trước khi cho phép truy cập. Nếu chỉ muốn bảo vệ OTA, có thể dùng policy/path riêng cho `/update` và các endpoint OTA.

## 5. Kiểm tra ESP32 trước khi chạy tunnel

Từ một thiết bị cùng LAN:

```text
http://192.168.110.206/
http://192.168.110.206/api/info
http://192.168.110.206/update
```

Cả ba phải hoạt động trước khi cấu hình Cloudflare.

## 6. Lưu ý WebSocket

Dashboard sử dụng WebSocket `/ws` để nhận dữ liệu thời gian thực. Cloudflare Tunnel có thể proxy WebSocket qua hostname HTTPS; không cần MQTT.

Firmware hiện tại giao tiếp trực tiếp bằng HTTP/WebSocket. MQTT đã được loại bỏ khỏi firmware, PlatformIO và giao diện.
