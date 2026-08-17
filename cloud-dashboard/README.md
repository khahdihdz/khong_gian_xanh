# 📱 Dashboard Xem Từ Xa

`index.html` là 1 trang tĩnh độc lập (không chạy trên ESP32) — mở được từ bất
kỳ đâu có internet, không cần cùng mạng WiFi với thiết bị. Nó đọc dữ liệu qua
Cloud Relay (xem `../cloud-relay/README.md`) thay vì WebSocket cục bộ.

## Cách dùng nhanh nhất
Mở thẳng file `index.html` trên điện thoại/máy tính (double-click hoặc
"Open with browser"). Lần đầu mở, trang sẽ hỏi **URL relay** và **token đọc
dữ liệu** — nhập vào rồi bấm **Lưu & Kết nối**. Thông tin này chỉ lưu trên
trình duyệt của bạn (`localStorage`), không gửi đi đâu khác.

## Triển khai thành 1 trang web thật (tuỳ chọn)
Deploy như bất kỳ site tĩnh nào — kéo thả cả thư mục vào:
- **Cloudflare Pages:** `wrangler pages deploy cloud-dashboard`
- **GitHub Pages:** đẩy thư mục này lên 1 nhánh/repo rồi bật Pages
- **Vercel:** `vercel deploy cloud-dashboard`

Sau khi deploy, bạn có 1 URL riêng (vd: `giam-sat.vercel.app`) để bookmark
trên điện thoại, mở là xem được ngay dù đang ở đâu.
