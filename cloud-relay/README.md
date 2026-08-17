# Cloud WebSocket miễn phí — Không Gian Xanh

Kiến trúc này **không dùng MQTT**, không cần VPS và không cần PC/Raspberry Pi/server nằm cùng LAN với ESP32.

```text
ESP32 -- WSS outbound --> Cloudflare Worker + Durable Object <-- WSS -- Dashboard
```

Cloudflare Durable Objects có trên Workers Free; SQLite-backed Durable Objects là lựa chọn được hỗ trợ trên Free. Hibernation WebSocket được dùng để tránh giữ Durable Object chạy liên tục khi không có traffic. Giới hạn Free hiện tại gồm 100.000 requests/ngày và 13.000 GB-s/ngày cho Durable Objects; mỗi WebSocket message được tính vào request limit. Vì vậy firmware gửi dữ liệu mỗi 10 giây, phù hợp cho một thiết bị cá nhân. Xem tài liệu Cloudflare để biết giới hạn mới nhất.

## 1. Tạo Worker

Cài Wrangler và đăng nhập Cloudflare:

```bash
npm install -g wrangler
wrangler login
cd cloud-relay
wrangler deploy
```

## 2. Tạo secret

Tạo hai token riêng:

```bash
wrangler secret put DEVICE_TOKEN
wrangler secret put READ_TOKEN
```

- `DEVICE_TOKEN`: chỉ lưu trong ESP32, dùng để xác thực kết nối `/ws/device`.
- `READ_TOKEN`: dùng trên dashboard từ xa để mở `/ws/browser`.

Không commit token vào Git.

## 3. URL cấu hình ESP32

Sau khi deploy, Worker có dạng:

```text
https://TEN-WORKER.workers.dev
```

Đổi `https://` thành `wss://` khi nhập vào cấu hình Cloud trên ESP32:

```text
wss://TEN-WORKER.workers.dev/ws/device
```

Trong trang cấu hình Cloud của ESP32:

- URL: `wss://TEN-WORKER.workers.dev/ws/device`
- Token: giá trị `DEVICE_TOKEN`
- Bật đồng bộ Cloud.

ESP32 tự tạo Device ID từ eFuse MAC và nối outbound tới Cloudflare; không cần mở port router.

## 4. Dashboard từ xa

Có thể deploy thư mục `cloud-dashboard/` lên Cloudflare Pages hoặc Workers Static Assets.

Trong dashboard nhập:

- URL Worker: `https://TEN-WORKER.workers.dev`
- Device ID: xem Serial Monitor khi ESP32 khởi động (`[CLOUD] Device ID: ...`)
- READ_TOKEN: giá trị `READ_TOKEN`

Dashboard sẽ kết nối:

```text
wss://TEN-WORKER.workers.dev/ws/browser?device=DEVICE_ID&token=READ_TOKEN
```

Dữ liệu cảm biến được truyền thời gian thực qua Durable Object.

## 5. Kiểm tra

```text
https://TEN-WORKER.workers.dev/health
```

Phải trả JSON có `ok: true`.

## 6. Lưu ý OTA

Cloud relay chỉ chuyển dữ liệu WebSocket. OTA `/update` của ESP32 vẫn hoạt động khi truy cập trực tiếp trong LAN. Nếu muốn OTA từ Internet, cần bổ sung lớp proxy/command riêng; **không mở `/update` công khai chỉ bằng cách thêm Worker WebSocket**.
