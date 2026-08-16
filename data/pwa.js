// Không Gian Xanh — PWA mobile
(function () {
  'use strict';

  // Khai báo manifest động để trang vẫn hoạt động khi được phục vụ trực tiếp từ ESP32.
  if (!document.querySelector('link[rel="manifest"]')) {
    const link = document.createElement('link');
    link.rel = 'manifest';
    link.href = '/manifest.json';
    document.head.appendChild(link);
  }

  // Thiết lập màu thanh trạng thái và giao diện mobile.
  const theme = document.createElement('meta');
  theme.name = 'theme-color';
  theme.content = '#0c130c';
  document.head.appendChild(theme);

  const mobile = document.createElement('meta');
  mobile.name = 'mobile-web-app-capable';
  mobile.content = 'yes';
  document.head.appendChild(mobile);

  const apple = document.createElement('meta');
  apple.name = 'apple-mobile-web-app-capable';
  apple.content = 'yes';
  document.head.appendChild(apple);

  const appleStatus = document.createElement('meta');
  appleStatus.name = 'apple-mobile-web-app-status-bar-style';
  appleStatus.content = 'black-translucent';
  document.head.appendChild(appleStatus);

  // Đăng ký Service Worker để PWA có app shell và cập nhật phiên bản tự động.
  if ('serviceWorker' in navigator && location.protocol !== 'file:') {
    window.addEventListener('load', () => {
      navigator.serviceWorker.register('/sw.js', { scope: '/' }).then(reg => {
        reg.update().catch(() => {});
      }).catch(err => console.warn('PWA Service Worker:', err));
    });
  }

  let deferredPrompt = null;

  function isStandalone() {
    return window.matchMedia('(display-mode: standalone)').matches || window.navigator.standalone === true;
  }

  function createInstallButton() {
    if (isStandalone() || document.getElementById('pwaInstall')) return;
    const bar = document.querySelector('.actions') || document.querySelector('.bar .actions') || document.querySelector('.top .actions') || document.querySelector('.bar');
    if (!bar) return;
    const btn = document.createElement('button');
    btn.id = 'pwaInstall';
    btn.type = 'button';
    btn.textContent = '📲 Cài ứng dụng';
    btn.style.cssText = 'border:1px solid #31432c;background:#182016;color:#f5f8ee;border-radius:99px;padding:8px 12px;font-size:.8rem;cursor:pointer;font:inherit';
    btn.addEventListener('click', async () => {
      if (deferredPrompt) {
        deferredPrompt.prompt();
        const result = await deferredPrompt.userChoice.catch(() => null);
        deferredPrompt = null;
        if (result && result.outcome === 'accepted') btn.remove();
        return;
      }
      // iOS/Safari không có beforeinstallprompt; hướng dẫn thao tác hệ thống.
      alert('Để cài Không Gian Xanh trên điện thoại: mở menu Chia sẻ hoặc ⋮ của trình duyệt → chọn “Thêm vào màn hình chính” hoặc “Cài đặt ứng dụng”.');
    });
    bar.appendChild(btn);
  }

  window.addEventListener('beforeinstallprompt', event => {
    event.preventDefault();
    deferredPrompt = event;
    createInstallButton();
  });

  window.addEventListener('appinstalled', () => {
    deferredPrompt = null;
    const btn = document.getElementById('pwaInstall');
    if (btn) btn.remove();
  });

  // Một số trình duyệt không phát beforeinstallprompt ngay khi tải; vẫn hiển thị nút hướng dẫn.
  document.addEventListener('DOMContentLoaded', () => setTimeout(createInstallButton, 300));
})();
