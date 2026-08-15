# ☁️ Cloud Relay — Xem Không Gian Xanh từ bất kỳ đâu

Thư mục này chứa 1 **Cloudflare Worker** đóng vai trò trạm trung chuyển:
ESP32 (đang ở nhà, trong mạng WiFi riêng) định kỳ đẩy dữ liệu lên Worker này
qua internet, và bạn có thể mở dashboard từ xa (dùng 4G, ở công ty, quán cà
phê...) để xem dữ liệu **mà không cần kết nối vào cùng mạng WiFi với ESP32**.

```
ESP32 (mạng nhà) --HTTPS POST /ingest--> Worker + KV --HTTPS GET /api/*--> Dashboard (bất kỳ đâu)
```

Miễn phí với gói Cloudflare Workers Free (đủ dùng cho 1 thiết bị cá nhân).

## 1. Cài đặt công cụ

```bash
npm install -g wrangler
wrangler login
```

## 2. Tạo KV namespace (nơi lưu dữ liệu mới nhất + lịch sử)

```bash
cd cloud-relay
wrangler kv namespace create AIRMON_KV
```

Lệnh trên in ra 1 `id`, dán vào `wrangler.toml` ở phần `kv_namespaces`.

## 3. Đặt 2 mã bí mật (secrets)

- `DEVICE_TOKEN`: mã riêng để ESP32 xác thực khi đẩy dữ liệu lên (`/ingest`).
- `READ_TOKEN`: mã riêng để dashboard từ xa xác thực khi đọc dữ liệu (`/api/*`).
  Có thể bỏ trống bước này nếu chấp nhận cho phép đọc công khai (không khuyến nghị).

```bash
wrangler secret put DEVICE_TOKEN
# nhập 1 chuỗi ngẫu nhiên dài, ví dụ: openssl rand -hex 16

wrangler secret put READ_TOKEN
# nhập 1 chuỗi ngẫu nhiên khác
```

## 4. Triển khai (deploy)

```bash
wrangler deploy
```

Wrangler sẽ in ra URL dạng:
`https://khong-gian-xanh-relay.<tên-tài-khoản>.workers.dev`

Đây chính là **URL relay** cần nhập vào ESP32 và vào dashboard từ xa.

## 5. Cấu hình ESP32 dùng relay này

1. Mở dashboard cục bộ (`http://<ip-esp32>`) → **Chức năng → Đồng bộ Cloud**.
2. Nhập:
   - **URL relay:** `https://khong-gian-xanh-relay.<tên-tài-khoản>.workers.dev`
   - **Token thiết bị:** đúng giá trị đã đặt cho `DEVICE_TOKEN` ở bước 3.
   - Bật công tắc **Đồng bộ Cloud**.
3. Lưu. ESP32 sẽ đẩy dữ liệu lên relay mỗi 60 giây (khi đang kết nối WiFi nhà,
   không đẩy khi đang ở chế độ AP cấu hình).

## 6. Mở dashboard từ xa

Dùng `cloud-dashboard/index.html` (xem README ở thư mục đó) — deploy lên
Cloudflare Pages/GitHub Pages/Vercel như 1 trang tĩnh bình thường, hoặc mở
trực tiếp file đó trên máy/điện thoại. Trang sẽ hỏi URL relay + token đọc
(`READ_TOKEN`) ở lần mở đầu tiên rồi tự lưu lại trên trình duyệt.

## Ghi chú

- Dữ liệu chỉ gồm nhiệt độ/độ ẩm/AQI/TVOC/eCO2 phòng — không phải dữ liệu
  nhạy cảm, nhưng vẫn nên đặt `READ_TOKEN` để người lạ không dò được URL và
  xem trộm.
- Relay không lưu lịch sử vĩnh viễn — mặc định giữ tối đa `MAX_HISTORY_RECORDS`
  bản ghi gần nhất (chỉnh trong `worker.js`), đủ vài ngày với chu kỳ đẩy 60 giây.
- Dashboard cục bộ (`http://<ip-esp32>`) vẫn hoạt động bình thường, độc lập với
  relay này — relay chỉ là kênh bổ sung để xem từ xa.
