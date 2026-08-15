// Không Gian Xanh — service worker
// Chỉ cache "khung" giao diện (HTML/manifest/icon) để mở nhanh trên mọi thiết bị,
// KHÔNG bao giờ cache dữ liệu cảm biến (/api/*), WebSocket (/ws) hay trang OTA (/update).
const CACHE_NAME = 'kgx-shell-v1';
const SHELL_FILES = [
  '/',
  '/index.html',
  '/tools.html',
  '/wifi_config.html',
  '/manifest.json',
  '/icons/icon-192.png',
  '/icons/icon-512.png'
];

self.addEventListener('install', (event) => {
  event.waitUntil(
    caches.open(CACHE_NAME)
      .then((cache) => cache.addAll(SHELL_FILES))
      .then(() => self.skipWaiting())
  );
});

self.addEventListener('activate', (event) => {
  event.waitUntil(
    caches.keys()
      .then((keys) => Promise.all(keys.filter((k) => k !== CACHE_NAME).map((k) => caches.delete(k))))
      .then(() => self.clients.claim())
  );
});

self.addEventListener('fetch', (event) => {
  const url = new URL(event.request.url);

  // Chỉ can thiệp request cùng gốc (thiết bị ESP32); để nguyên CDN font/Chart.js.
  if (url.origin !== self.location.origin) return;
  // Không bao giờ cache dữ liệu sống hoặc trang OTA.
  if (url.pathname.startsWith('/api/') || url.pathname.startsWith('/ws') || url.pathname.startsWith('/update')) return;
  if (event.request.method !== 'GET') return;

  // Stale-while-revalidate: trả cache ngay (mở nhanh), song song tải bản mới để cập nhật cache.
  event.respondWith(
    caches.match(event.request).then((cached) => {
      const network = fetch(event.request)
        .then((response) => {
          if (response && response.status === 200) {
            const copy = response.clone();
            caches.open(CACHE_NAME).then((cache) => cache.put(event.request, copy));
          }
          return response;
        })
        .catch(() => cached);
      return cached || network;
    })
  );
});
