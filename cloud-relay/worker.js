/**
 * Không Gian Xanh — Cloud Relay (Cloudflare Worker)
 * ============================================================
 * Vai trò: làm cầu nối trung gian để dashboard có thể xem dữ liệu
 * ESP32 TỪ BẤT KỲ ĐÂU (dùng mạng di động, quán cà phê, ở công ty...)
 * mà KHÔNG cần kết nối vào cùng mạng WiFi/LAN với thiết bị.
 *
 * ESP32 (trong mạng nhà) --HTTPS POST--> Worker này --lưu vào KV-->
 *   --HTTPS GET--> Dashboard từ xa (mở ở bất kỳ đâu có internet)
 *
 * Endpoints:
 *   POST /ingest        Header: X-Device-Token: <DEVICE_TOKEN>
 *                        Body JSON: dữ liệu cảm biến hiện tại (do ESP32 gửi)
 *   GET  /api/data       ?token=<READ_TOKEN nếu có cấu hình>  -> bản ghi mới nhất
 *   GET  /api/history     ?hours=1|6|12|24|0&token=...          -> mảng lịch sử
 *
 * Triển khai: xem cloud-relay/README.md
 */

const MAX_HISTORY_RECORDS = 4320; // ví dụ: 3 ngày nếu ESP32 đẩy mỗi 60 giây

const CORS_HEADERS = {
  "Access-Control-Allow-Origin": "*",
  "Access-Control-Allow-Methods": "GET,POST,OPTIONS",
  "Access-Control-Allow-Headers": "Content-Type,X-Device-Token",
};

function jsonResponse(obj, status = 200) {
  return new Response(JSON.stringify(obj), {
    status,
    headers: { ...CORS_HEADERS, "Content-Type": "application/json" },
  });
}

function checkReadAccess(url, env) {
  // Nếu người dùng không cấu hình READ_TOKEN thì coi như cho đọc công khai
  // (chấp nhận được vì chỉ là nhiệt độ/độ ẩm phòng, không phải dữ liệu nhạy cảm -
  // nhưng khuyến nghị luôn đặt READ_TOKEN để giữ riêng tư).
  if (!env.READ_TOKEN) return true;
  return url.searchParams.get("token") === env.READ_TOKEN;
}

export default {
  async fetch(request, env) {
    const url = new URL(request.url);

    if (request.method === "OPTIONS") {
      return new Response(null, { headers: CORS_HEADERS });
    }

    // -------------------- ESP32 đẩy dữ liệu lên --------------------
    if (url.pathname === "/ingest" && request.method === "POST") {
      const token = request.headers.get("X-Device-Token") || "";
      if (!env.DEVICE_TOKEN || token !== env.DEVICE_TOKEN) {
        return jsonResponse({ ok: false, message: "Sai hoặc thiếu device token" }, 401);
      }

      let body;
      try {
        body = await request.json();
      } catch (e) {
        return jsonResponse({ ok: false, message: "JSON không hợp lệ" }, 400);
      }

      if (!body.time) body.time = Math.floor(Date.now() / 1000);
      await env.AIRMON_KV.put("latest", JSON.stringify(body));

      let history = [];
      const rawHistory = await env.AIRMON_KV.get("history");
      if (rawHistory) {
        try { history = JSON.parse(rawHistory); } catch (e) { history = []; }
      }
      history.push({
        time: body.time,
        temperature: body.temperature,
        humidity: body.humidity,
        tvoc: body.tvoc,
        eco2: body.eco2,
        aqi: body.aqi,
      });
      if (history.length > MAX_HISTORY_RECORDS) {
        history = history.slice(history.length - MAX_HISTORY_RECORDS);
      }
      await env.AIRMON_KV.put("history", JSON.stringify(history));

      return jsonResponse({ ok: true });
    }

    // -------------------- Dashboard từ xa đọc dữ liệu mới nhất --------------------
    if (url.pathname === "/api/data" && request.method === "GET") {
      if (!checkReadAccess(url, env)) return jsonResponse({ ok: false, message: "Sai token" }, 401);
      const raw = await env.AIRMON_KV.get("latest");
      return new Response(raw || "{}", { headers: { ...CORS_HEADERS, "Content-Type": "application/json" } });
    }

    // -------------------- Dashboard từ xa đọc lịch sử --------------------
    if (url.pathname === "/api/history" && request.method === "GET") {
      if (!checkReadAccess(url, env)) return jsonResponse({ ok: false, message: "Sai token" }, 401);

      const hours = parseInt(url.searchParams.get("hours") || "0", 10);
      const raw = await env.AIRMON_KV.get("history");
      let history = [];
      if (raw) {
        try { history = JSON.parse(raw); } catch (e) { history = []; }
      }
      if (hours > 0) {
        const cutoff = Math.floor(Date.now() / 1000) - hours * 3600;
        history = history.filter((r) => r.time >= cutoff);
      }
      return jsonResponse(history);
    }

    return new Response("Không Gian Xanh Cloud Relay đang chạy.", { headers: CORS_HEADERS });
  },
};
