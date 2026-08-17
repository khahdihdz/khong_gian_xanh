import { DurableObject } from "cloudflare:workers";

/**
 * Không Gian Xanh - Cloudflare Worker + Durable Object WebSocket relay.
 *
 * ESP32 mở WSS outbound tới /ws/device?device=<MAC>.
 * Dashboard mở WSS tới /ws/browser?device=<MAC>&token=<READ_TOKEN>.
 * Durable Object giữ hai đầu kết nối và chuyển tiếp dữ liệu hai chiều.
 * Không MQTT, không KV, không cần PC/Raspberry Pi/VPS trong LAN.
 */

function json(obj, status = 200) {
  return new Response(JSON.stringify(obj), {
    status,
    headers: { "content-type": "application/json; charset=utf-8" },
  });
}

export default {
  async fetch(request, env) {
    const url = new URL(request.url);

    if (url.pathname === "/health") {
      return json({ ok: true, service: "khong-gian-xanh-ws-relay" });
    }

    if (url.pathname !== "/ws/device" && url.pathname !== "/ws/browser") {
      return new Response("Không Gian Xanh WebSocket Relay", { status: 200 });
    }

    if (request.headers.get("Upgrade")?.toLowerCase() !== "websocket") {
      return new Response("WebSocket required", { status: 426 });
    }

    const device = (url.searchParams.get("device") || "").trim().toUpperCase();
    if (!/^[0-9A-F]{8,32}$/.test(device)) {
      return json({ ok: false, message: "Device ID không hợp lệ" }, 400);
    }

    if (url.pathname === "/ws/device") {
      const token = request.headers.get("X-Device-Token") || "";
      if (!env.DEVICE_TOKEN || token !== env.DEVICE_TOKEN) {
        return json({ ok: false, message: "Sai device token" }, 401);
      }
    } else {
      const token = url.searchParams.get("token") || "";
      if (!env.READ_TOKEN || token !== env.READ_TOKEN) {
        return json({ ok: false, message: "Sai read token" }, 401);
      }
    }

    const id = env.KGX_ROOMS.idFromName(device);
    return env.KGX_ROOMS.get(id).fetch(request);
  },
};

export class KgxRoom extends DurableObject {
  constructor(ctx, env) {
    super(ctx, env);
    this.ctx = ctx;
    // Ping/pong được xử lý ở runtime, không cần đánh thức DO.
    this.ctx.setWebSocketAutoResponse(
      new WebSocketRequestResponsePair("ping", "pong")
    );
  }

  async fetch(request) {
    const url = new URL(request.url);
    const role = url.pathname.endsWith("/device") ? "device" : "browser";

    const pair = new WebSocketPair();
    const [client, server] = Object.values(pair);

    this.ctx.acceptWebSocket(server, [role]);
    server.serializeAttachment({ role, connectedAt: Date.now() });

    // Browser mới kết nối được biết trạng thái thiết bị ngay.
    if (role === "browser") {
      const devices = this.ctx.getWebSockets("device");
      server.send(JSON.stringify({
        type: "relay",
        device_online: devices.length > 0,
      }));
    }

    return new Response(null, { status: 101, webSocket: client });
  }

  webSocketMessage(ws, message) {
    const state = ws.deserializeAttachment() || {};
    const targetRole = state.role === "device" ? "browser" : "device";
    const targets = this.ctx.getWebSockets(targetRole);

    for (const target of targets) {
      if (target.readyState === WebSocket.OPEN) {
        try { target.send(message); } catch (_) {}
      }
    }
  }

  webSocketClose(ws, code, reason, wasClean) {
    const state = ws.deserializeAttachment() || {};
    ws.close(code, reason);

    // Báo cho phía còn lại để dashboard biết ESP32 vừa offline.
    if (state.role === "device") {
      for (const target of this.ctx.getWebSockets("browser")) {
        if (target.readyState === WebSocket.OPEN) {
          try { target.send(JSON.stringify({ type: "relay", device_online: false })); } catch (_) {}
        }
      }
    }
    void wasClean;
  }

  webSocketError(ws, error) {
    console.error("WebSocket error:", error);
  }
}
