// Không Gian Xanh — menu PWA dùng chung cho Web Server ESP32
(function () {
  'use strict';
  const isPages = location.hostname.endsWith('.pages.dev');
  const standalone = () => window.matchMedia('(display-mode: standalone)').matches || window.navigator.standalone === true;

  function installMeta() {
    if (!document.querySelector('link[rel="manifest"]')) {
      const l = document.createElement('link'); l.rel = 'manifest'; l.href = '/manifest.json'; document.head.appendChild(l);
    }
    if (!document.querySelector('meta[name="theme-color"]')) {
      const m = document.createElement('meta'); m.name = 'theme-color'; m.content = '#0c130c'; document.head.appendChild(m);
    }
    for (const [name, content] of [['mobile-web-app-capable','yes'],['apple-mobile-web-app-capable','yes'],['apple-mobile-web-app-status-bar-style','black-translucent']]) {
      if (!document.querySelector(`meta[name="${name}"]`)) { const m = document.createElement('meta'); m.name = name; m.content = content; document.head.appendChild(m); }
    }
  }

  function injectNav() {
    if (document.getElementById('kgxNav')) return;
    const current = location.pathname.replace(/\/$/, '') || '/';
    const routes = isPages
      ? [['/','🏠 Tổng quan'],['/tools','⚙️ Chức năng'],['/wifi','📶 Wi-Fi'],['/update','⬆️ OTA']]
      : [['/','🏠 Tổng quan'],['/tools.html','⚙️ Chức năng'],['/wifi_config.html','📶 Wi-Fi'],['/update','⬆️ OTA']];
    const nav = document.createElement('nav'); nav.id = 'kgxNav';
    nav.innerHTML = routes.map(([u,t]) => `<a href="${u}" data-route="${u}">${t}</a>`).join('') + '<span class="kgxNavSep"></span><button type="button" id="kgxInstall" hidden>📲 Cài app</button>';
    const style = document.createElement('style'); style.id = 'kgxNavStyle'; style.textContent = '#kgxNav{display:flex;align-items:center;gap:6px;flex-wrap:wrap;margin-left:auto}#kgxNav a,#kgxNav button{border:1px solid #31432c;background:#182016;color:#a7b69c;border-radius:999px;padding:8px 11px;text-decoration:none;font:inherit;font-size:.78rem;cursor:pointer;white-space:nowrap}#kgxNav a:hover,#kgxNav button:hover{color:#f5f8ee;border-color:#4ade80}#kgxNav a.active{background:#4ade80;color:#071007;border-color:#4ade80;font-weight:700}#kgxNav .kgxNavSep{width:1px;height:22px;background:#31432c;margin:0 2px}@media(max-width:700px){#kgxNav{width:100%;overflow-x:auto;flex-wrap:nowrap;margin-left:0;padding-bottom:2px;scrollbar-width:none}#kgxNav::-webkit-scrollbar{display:none}#kgxNav a,#kgxNav button{flex:0 0 auto}}'; document.head.appendChild(style);
    nav.querySelectorAll('[data-route]').forEach(a => { const href = a.dataset.route; if (href === current) a.classList.add('active'); });
    const duplicatePaths = isPages ? ['/tools','/wifi','/update','/tools.html','/wifi_config.html','/update.html'] : ['/','/tools.html','/wifi_config.html','/update','/update.html'];
    document.querySelectorAll('a').forEach(a => { if (a.closest('#kgxNav')) return; const h = a.getAttribute('href'); if (h && duplicatePaths.includes(h)) a.remove(); });
    const host = document.querySelector('.actions') || document.querySelector('.top .actions') || document.querySelector('.bar .actions');
    if (host) host.appendChild(nav); else { const bar = document.querySelector('.top,.bar,header'); if (bar) bar.appendChild(nav); }
  }

  if ('serviceWorker' in navigator && location.protocol !== 'file:') {
    window.addEventListener('load', () => navigator.serviceWorker.register('/sw.js', {scope:'/'}).then(r => r.update().catch(() => {})).catch(() => {}));
  }

  let deferredPrompt = null;
  function createInstallButton() {
    if (standalone() || !isPages) return;
    const b = document.getElementById('kgxInstall'); if (!b) return;
    b.hidden = false;
    b.onclick = async () => {
      if (deferredPrompt) { deferredPrompt.prompt(); const r = await deferredPrompt.userChoice.catch(() => null); deferredPrompt = null; if (r && r.outcome === 'accepted') b.hidden = true; return; }
      alert('Mở menu ⋮ của trình duyệt → chọn “Thêm vào màn hình chính” hoặc “Cài đặt ứng dụng”.');
    };
  }
  window.addEventListener('beforeinstallprompt', e => { e.preventDefault(); deferredPrompt = e; createInstallButton(); });
  window.addEventListener('appinstalled', () => { deferredPrompt = null; const b = document.getElementById('kgxInstall'); if (b) b.hidden = true; });
  document.addEventListener('DOMContentLoaded', () => { installMeta(); injectNav(); setTimeout(createInstallButton, 300); });
})();
