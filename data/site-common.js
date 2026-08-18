/* Không Gian Xanh - shared site chrome. Include this file on every HTML page. */
(function () {
  'use strict';
  const NAV = [
    ['dashboard', '⌂', 'Tổng quan', '/#dashboard'],
    ['history', '▥', 'Lịch sử', '/#history'],
    ['system', '⚙', 'Hệ thống', '/#system'],
    ['ota', '↻', 'Cập nhật', '/update']
  ];

  function inject() {
    const root = document.body;
    if (!root || document.querySelector('[data-site-header]')) return;

    const header = document.createElement('header');
    header.className = 'site-header';
    header.dataset.siteHeader = '1';
    header.innerHTML = `
      <div class="site-header-inner">
        <a class="site-brand" href="/#dashboard" aria-label="Trang chủ Không Gian Xanh">
          <span class="site-brand-mark">🌿</span>
          <span><strong>Không Gian Xanh</strong><small>Giám sát môi trường</small></span>
        </a>
        <button class="site-menu-toggle" type="button" aria-label="Mở menu" aria-expanded="false">☰</button>
        <nav class="site-nav" aria-label="Điều hướng chính">
          ${NAV.map(([id, icon, label, href]) => `<a class="site-nav-link" data-nav="${id}" href="${href}"><span>${icon}</span>${label}</a>`).join('')}
        </nav>
      </div>`;

    const footer = document.createElement('footer');
    footer.className = 'site-footer';
    footer.dataset.siteFooter = '1';
    footer.innerHTML = `
      <div class="site-footer-inner">
        <span>Không Gian Xanh · ESP32</span>
        <span>Powered by <strong>Không Gian Xanh</strong></span>
        <span>Firmware <span id="siteFirmware">--</span></span>
      </div>`;

    root.insertBefore(header, root.firstChild);
    root.appendChild(footer);

    const toggle = header.querySelector('.site-menu-toggle');
    const nav = header.querySelector('.site-nav');
    toggle.addEventListener('click', function () {
      const open = nav.classList.toggle('open');
      toggle.setAttribute('aria-expanded', open ? 'true' : 'false');
    });
    nav.addEventListener('click', function (e) {
      if (e.target.closest('a')) {
        nav.classList.remove('open');
        toggle.setAttribute('aria-expanded', 'false');
      }
    });

    const path = location.pathname;
    const hash = location.hash.replace('#', '');
    let active = hash || (path === '/' || path === '/index.html' ? 'dashboard' : '');
    if (path === '/update') active = 'ota';
    const activeLink = header.querySelector(`[data-nav="${active}"]`);
    if (activeLink) activeLink.classList.add('active');

    fetch('/api/info', { cache: 'no-store' })
      .then(r => r.ok ? r.json() : null)
      .then(info => {
        if (info && info.firmware_version) {
          const el = document.getElementById('siteFirmware');
          if (el) el.textContent = info.firmware_version;
        }
      }).catch(() => {});
  }

  if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', inject);
  else inject();
})();
