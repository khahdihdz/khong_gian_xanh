(() => {
  const CSS = `
    .kgx-header{padding:16px 20px;border-bottom:1px solid var(--l,#31432c);display:flex;justify-content:space-between;align-items:center;gap:10px;flex-wrap:wrap;background:var(--bg,#0c130c);color:var(--t,#f5f8ee);position:relative;z-index:1000}
    .kgx-brand{font:600 1.35rem Fraunces,'Be Vietnam Pro',system-ui,sans-serif}.kgx-brand small{display:block;color:var(--m,#a7b69c);font:500 .65rem 'Be Vietnam Pro',system-ui,sans-serif;letter-spacing:1.4px;text-transform:uppercase}
    .kgx-actions{display:flex;gap:8px;align-items:center;flex-wrap:wrap}.kgx-pill,.kgx-badge{border:1px solid var(--l,#31432c);background:var(--p,#182016);border-radius:99px;padding:8px 12px;color:var(--m,#a7b69c);text-decoration:none;font-size:.8rem}.kgx-dot{display:inline-block;width:8px;height:8px;border-radius:50%;background:#f87171;margin-right:6px}.kgx-dot.on{background:#4ade80}.kgx-badge.online{color:#4ade80;border-color:#4ade80}
    .kgx-nav{position:relative}.kgx-nav-toggle{border:1px solid var(--l,#31432c);background:var(--p,#182016);border-radius:99px;padding:8px 13px;color:var(--t,#f5f8ee);font:inherit;font-size:.8rem;cursor:pointer}.kgx-nav-menu{display:none;position:absolute;right:0;top:calc(100% + 8px);min-width:210px;padding:7px;background:var(--p,#182016);border:1px solid var(--l,#31432c);border-radius:14px;box-shadow:0 14px 35px rgba(0,0,0,.35);z-index:10000}.kgx-nav.open .kgx-nav-menu{display:grid;gap:3px}.kgx-nav-menu a{display:block;padding:10px 12px;border-radius:9px;color:var(--t,#f5f8ee);text-decoration:none;font-size:.82rem}.kgx-nav-menu a:hover,.kgx-nav-menu a:focus{background:var(--p2,#1e2a1a);color:#4ade80;outline:0}.kgx-nav-menu a.active{background:rgba(74,222,128,.14);color:#4ade80}
    .kgx-footer{max-width:1100px;margin:24px auto 0;padding:18px 20px;border-top:1px solid var(--l,#31432c);text-align:center;color:var(--m,#a7b69c);font-size:.78rem}.kgx-footer a{color:#4ade80;text-decoration:none;font-weight:600}.kgx-footer a:hover{text-decoration:underline}
    @media(max-width:600px){.kgx-header{padding:14px}.kgx-actions{width:100%}.kgx-nav-menu{position:fixed;right:12px;top:72px;width:min(230px,calc(100vw - 24px))}.kgx-footer{margin-top:18px;padding:16px 12px}}
  `;

  function installCss() {
    if (document.getElementById('kgx-common-style')) return;
    const style = document.createElement('style');
    style.id = 'kgx-common-style';
    style.textContent = CSS;
    document.head.appendChild(style);
  }

  async function loadFragment(path) {
    const response = await fetch(path, { cache: 'no-store' });
    if (!response.ok) throw new Error(`${path}: HTTP ${response.status}`);
    return response.text();
  }

  function bindNav() {
    const nav = document.getElementById('nav');
    const toggle = document.getElementById('navToggle');
    if (!nav || !toggle || nav.dataset.bound === '1') return;
    nav.dataset.bound = '1';
    toggle.addEventListener('click', event => {
      event.stopPropagation();
      const open = nav.classList.toggle('open');
      toggle.setAttribute('aria-expanded', open ? 'true' : 'false');
    });
    document.addEventListener('click', event => {
      if (!nav.contains(event.target)) {
        nav.classList.remove('open');
        toggle.setAttribute('aria-expanded', 'false');
      }
    });
    document.addEventListener('keydown', event => {
      if (event.key === 'Escape') {
        nav.classList.remove('open');
        toggle.setAttribute('aria-expanded', 'false');
      }
    });

    const current = location.pathname.replace(/\/$/, '') || '/';
    nav.querySelectorAll('a').forEach(link => {
      const href = new URL(link.href, location.href).pathname.replace(/\/$/, '') || '/';
      if (href === current) link.classList.add('active');
    });
  }

  async function init() {
    installCss();
    try {
      const [header, footer] = await Promise.all([
        loadFragment('/header.html'),
        loadFragment('/footer.html')
      ]);

      document.querySelectorAll('header.top, .top, .bar').forEach(node => node.remove());
      const wrapper = document.createElement('div');
      wrapper.innerHTML = header.trim();
      document.body.insertBefore(wrapper.firstElementChild, document.body.firstChild);

      const footerWrapper = document.createElement('div');
      footerWrapper.innerHTML = footer.trim();
      document.body.appendChild(footerWrapper.firstElementChild);
      bindNav();
    } catch (error) {
      console.warn('[KGX] Không tải được header/footer dùng chung:', error);
      // Fallback vẫn tạo footer nếu ESP32 đang phục vụ fragment không ổn định.
      if (!document.querySelector('.kgx-footer')) {
        const footer = document.createElement('footer');
        footer.className = 'kgx-footer';
        footer.innerHTML = 'Powered by <a href="https://khahdihdz.github.io" target="_blank" rel="noopener noreferrer">khahdihdz.github.io</a>';
        document.body.appendChild(footer);
      }
    }
  }

  if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', init, { once: true });
  else init();
})();
