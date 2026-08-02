#!/data/data/com.termux/files/usr/bin/bash
# ============================================================
#  flash.sh — Tự động biên dịch & nạp firmware ESP32 qua Termux
#  Dự án: Không Gian Xanh
#
#  Cách dùng:
#     ./flash.sh
#
#  Script sẽ:
#    1. Tự cài PlatformIO nếu chưa có
#    2. Tự dò tìm ESP32 cắm qua cáp OTG
#    3. Biên dịch firmware
#    4. Nạp firmware
#    5. Nạp filesystem (thư mục data/) nếu có, để dashboard web hoạt động
# ============================================================
set -uo pipefail

GREEN='\033[1;32m'; YELLOW='\033[1;33m'; RED='\033[1;31m'; CYAN='\033[1;36m'; NC='\033[0m'
info()  { echo -e "${CYAN}➜${NC} $1"; }
ok()    { echo -e "${GREEN}✔${NC} $1"; }
warn()  { echo -e "${YELLOW}⚠${NC} $1"; }
err()   { echo -e "${RED}✘${NC} $1"; }

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$PROJECT_DIR" || { err "Không tìm thấy thư mục dự án."; exit 1; }

if [ ! -f "platformio.ini" ]; then
    err "Không thấy platformio.ini trong $PROJECT_DIR — hãy chạy script này ngay trong thư mục dự án."
    exit 1
fi

# ---------- 1. Kiểm tra môi trường ----------
if [ ! -d "/data/data/com.termux" ]; then
    warn "Không phát hiện đang chạy trong Termux — phần tự xin quyền USB (termux-usb) có thể không hoạt động."
fi

# ---------- 2. Cài PlatformIO nếu chưa có ----------
if ! command -v pio >/dev/null 2>&1; then
    warn "Chưa có PlatformIO (pio). Đang cài đặt (cần internet, có thể mất vài phút)..."
    if ! command -v python >/dev/null 2>&1; then
        info "Cài Python..."
        pkg install -y python || { err "Cài Python thất bại."; exit 1; }
    fi
    command -v git >/dev/null 2>&1 || pkg install -y git
    pip install -U platformio || {
        err "Cài PlatformIO qua pip thất bại."
        echo "  Thử thủ công: pkg update && pkg install -y python git && pip install -U platformio"
        exit 1
    }
fi
ok "PlatformIO sẵn sàng: $(pio --version 2>/dev/null)"

# ---------- 3. Hàm dò cổng USB-Serial (ESP32 qua OTG) ----------
find_serial_ports() {
    local ports=()
    for p in /dev/ttyUSB* /dev/ttyACM*; do
        [ -e "$p" ] && ports+=("$p")
    done
    printf '%s\n' "${ports[@]}"
}

# ---------- 4. Xin quyền truy cập USB qua termux-usb (nếu cần) ----------
grant_usb_permission() {
    command -v termux-usb >/dev/null 2>&1 || return 0
    local dev list
    list="$(termux-usb -l 2>/dev/null | tr -d '[]," ')"
    for dev in $list; do
        [ -n "$dev" ] && termux-usb -r "$dev" >/dev/null 2>&1
    done
    sleep 1
}

info "Đang tìm ESP32 qua OTG (cắm cáp nếu chưa cắm)..."
PORT=""
ATTEMPTS=0
while [ $ATTEMPTS -lt 20 ]; do
    mapfile -t PORTS < <(find_serial_ports)
    if [ ${#PORTS[@]} -gt 0 ]; then
        break
    fi
    ATTEMPTS=$((ATTEMPTS+1))
    sleep 1
done

if [ ${#PORTS[@]:-0} -eq 0 ]; then
    err "Không tìm thấy ESP32 nào qua OTG sau ${ATTEMPTS}s."
    echo "  Kiểm tra:"
    echo "   - Cáp OTG + cáp USB-Serial (CP2102/CH340) đã cắm chắc chưa."
    echo "   - Nếu điện thoại hiện popup xin quyền USB, hãy bấm Cho phép."
    echo "   - Cài thêm 'Termux:API' + gói termux-api nếu cổng không tự hiện:"
    echo "       pkg install termux-api"
    exit 1
fi

if [ ${#PORTS[@]} -eq 1 ]; then
    PORT="${PORTS[0]}"
else
    warn "Phát hiện nhiều cổng USB-Serial:"
    select p in "${PORTS[@]}"; do
        [ -n "$p" ] && { PORT="$p"; break; }
    done
fi
ok "Tìm thấy ESP32 tại: $PORT"

grant_usb_permission

# Kiểm tra thử quyền đọc/ghi cổng, nếu chưa có thì thử xin quyền lại 1 lần
if [ ! -r "$PORT" ] || [ ! -w "$PORT" ]; then
    warn "Chưa có quyền truy cập $PORT, đang thử xin lại quyền USB..."
    grant_usb_permission
    sleep 1
fi

# ---------- 5. Biên dịch firmware ----------
info "Đang biên dịch firmware..."
pio run -e esp32dev || { err "Biên dịch thất bại. Xem log phía trên."; exit 1; }
ok "Biên dịch firmware thành công."

# ---------- 6. Nạp firmware ----------
info "Đang nạp firmware vào $PORT ..."
if ! pio run -e esp32dev --target upload --upload-port "$PORT"; then
    err "Nạp firmware thất bại."
    echo "  - Thử giữ nút BOOT trên ESP32 khi bắt đầu nạp nếu board không tự vào chế độ nạp."
    echo "  - Kiểm tra lại quyền USB (popup xin quyền trên điện thoại)."
    exit 1
fi
ok "Nạp firmware thành công."

# ---------- 7. Nạp filesystem (data/) nếu có ----------
if [ -d "data" ] && [ -n "$(ls -A data 2>/dev/null)" ]; then
    info "Phát hiện thư mục data/ — đang nạp giao diện web (LittleFS)..."
    sleep 2   # chờ ESP32 khởi động lại sau khi nạp firmware xong
    if ! pio run -e esp32dev --target uploadfs --upload-port "$PORT"; then
        err "Nạp filesystem thất bại."
        echo "  Thử lại thủ công: pio run -e esp32dev -t uploadfs --upload-port $PORT"
        exit 1
    fi
    ok "Nạp filesystem thành công."
else
    warn "Không có thư mục data/ (hoặc rỗng) — bỏ qua bước nạp filesystem."
fi

echo
ok "🎉 Hoàn tất! Xem log thiết bị bằng:  pio device monitor --port $PORT"
