// Không Gian Xanh — PWA mobile + cấu hình MQTT dùng chung với Web
(function () {
  'use strict';

  const CFG = 'kgx_mqtt_personal_v2';
  const PASS = 'kgx_mqtt_password_local';
  const READY = 'kgx_mqtt_configured';
  const SESSION = 'kgx_mqtt_password_session';
  const RESTORED = 'kgx_pwa_session_restored';

  if (!document.querySelector('link[rel="manifest"]')) {
    const link = document.createElement('link');
    link.rel = 'manifest';
    link.href = '/manifest.json';
    document.head.appendChild(link);
  }

  const theme = document.createElement('meta');
  theme.name = 'theme-color';
  theme.content = '#0c130c';
  document.head.appendChild(theme);

  for (const [name, content] of [
    ['mobile-web-app-capable', 'yes'],
    ['apple-mobile-web-app-capable', 'yes'],
    ['apple-mobile-web-app-status-bar-style', 'black-translucent']
  ]) {
    const m = document.createElement('meta');
    m.name = name;
    m.content = content;
    document.head.appendChild(m);
  }

  function standalone() {
    return window.matchMedia('(display-mode: standalone)').matches || window.navigator.standalone === true;
  }

  function readConfig() {
    try {
      const cfg = JSON.parse(localStorage.getItem(CFG) || '{}');
      const pass = localStorage.getItem(PASS) || sessionStorage.getItem(SESSION) || '';
      return { cfg, pass };
    } catch (_) {
      return { cfg: {}, pass: '' };
    }
  }

  function ready() {
    const { cfg, pass } = readConfig();
    return !!(cfg.host && cfg.port && cfg.user && cfg.device && pass && localStorage.getItem(READY) === '1');
  }

  function syncWebConfig() {
    try {
      const passInput = document.getElementById('pass');
      const savePass = () => {
        if (passInput && passInput.value) localStorage.setItem(PASS, passInput.value);
      };
      if (passInput) {
        savePass();
        passInput.addEventListener('input', savePass);
        passInput.addEventListener('change', savePass);
      }

      const status = document.getElementById('status');
      if (status) {
        const mark = () => {
          const text = (status.textContent || '').toLowerCase();
          if (text.includes('mqtt websocket đã kết nối')) {
            const { cfg, pass } = readConfig();
            if (cfg.host && cfg.port && cfg.user && cfg.device && pass) {
              localStorage.setItem(READY, '1');
              localStorage.setItem(PASS, pass);
              createInstallButton();
            }
          }
        };
        new MutationObserver(mark).observe(status, { childList: true, characterData: true, subtree: true });
        mark();
      }
    } catch (_) {}
  }

  function restoreForApp() {
    if (!standalone()) return;
    try {
      const { cfg, pass } = readConfig();
      if (!cfg.host || !cfg.port || !cfg.user || !cfg.device || !pass || localStorage.getItem(READY) !== '1') {
        if (!location.pathname.endsWith('/mqtt')) location.replace('/mqtt?setup=1');
        return;
      }

      const hadSession = !!sessionStorage.getItem(SESSION);
      sessionStorage.setItem(SESSION, pass);

      if (!hadSession && !sessionStorage.getItem(RESTORED) && !location.pathname.endsWith('/mqtt')) {
        sessionStorage.setItem(RESTORED, '1');
        location.reload();
      }
    } catch (_) {}
  }

  if ('serviceWorker' in navigator && location.protocol !== 'file:') {
    window.addEventListener('load', () => {
      navigator.serviceWorker.register('/sw.js', { scope: '/' }).then(reg => reg.update().catch(() => {})).catch(err => console.warn('PWA Service Worker:', err));
    });
  }

  let deferredPrompt = null;

  function createInstallButton() {
    if (standalone() || document.getElementById('pwaInstall') || !ready()) return;
    const bar = document.querySelector('.actions') || document.querySelector('.bar .actions') || document.querySelector('.top .actions') || document.querySelector('.bar');
    if (!bar) return;

    const btn = document.createElement('button');
    btn.id = 'pwaInstall';
    btn.type = 'button';
    btn.textContent = '📲 Cài ứng dụng';
    btn.title = 'Cấu hình MQTT đã hoàn tất — cài ứng dụng Không Gian Xanh';
    btn.style.cssText = 'border:1px solid #31432c;background:#182016;color:#f5f8ee;border-radius:99px;padding:8px 12px;font-size:.8rem;cursor:pointer;font:inherit';
    btn.addEventListener('click', async () => {
      if (!ready()) {
        alert('Hãy cấu hình MQTT trên Web và kết nối thành công trước khi cài ứng dụng.');
        return;
      }
      if (deferredPrompt) {
        deferredPrompt.prompt();
        const result = await deferredPrompt.userChoice.catch(() => null);
        deferredPrompt = null;
        if (result && result.outcome === 'accepted') btn.remove();
        return;
      }
      alert('Cấu hình Web đã sẵn sàng. Mở menu ⋮ của trình duyệt → chọn “Thêm vào màn hình chính” hoặc “Cài đặt ứng dụng”.');
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

  document.addEventListener('DOMContentLoaded', () => {
    syncWebConfig();
    restoreForApp();
    setTimeout(createInstallButton, 300);
  });
})();
