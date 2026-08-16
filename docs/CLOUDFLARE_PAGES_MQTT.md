# Không Gian Xanh — Cloudflare Pages + MQTT cá nhân

## Kiến trúc

ESP32 → MQTT/TLS → MQTT Broker → MQTT over WebSocket → Cloudflare Pages.

Cloudflare Pages chỉ phục vụ giao diện; MQTT broker không chạy trên Pages.

## Dashboard

Sau khi Pages được cấu hình với thư mục publish `data`, mở:

`/mqtt_dashboard.html`

Trang này kết nối trực tiếp tới broker bằng `wss://` và subscribe:

- `khonggianxanh/<DEVICE_ID>/telemetry`
- `khonggianxanh/<DEVICE_ID>/status`

## HiveMQ Cloud

Với broker HiveMQ Cloud hiện tại của dự án:

- MQTT TLS: port `8883`
- MQTT WebSocket TLS: port `8884`
- WebSocket path: `/mqtt`

Broker hostname được nhập trong giao diện, không hard-code vào source.

## Cloudflare Pages

Tạo Pages project từ repository này và chọn thư mục `data` làm thư mục static assets. Không cần Worker cho dashboard MQTT trực tiếp.

## Bảo mật

Username/password MQTT chỉ nhập trên giao diện và không commit vào GitHub. Dashboard lưu một phần cấu hình kết nối trong localStorage của trình duyệt; mật khẩu không được lưu.

## Tương thích firmware

Firmware ESP32 hiện tại đã dùng topic dạng:

`khonggianxanh/<DEVICE_ID>/telemetry`

`khonggianxanh/<DEVICE_ID>/status`

và publish telemetry QoS 1, nên dashboard mới có thể sử dụng trực tiếp mà không cần HTTP Cloud Relay.
