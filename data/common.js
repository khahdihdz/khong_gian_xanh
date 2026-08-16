(() => {
  'use strict';

  /* =========================================================
   * KHÔNG GIAN XANH - GIAO DIỆN DÙNG CHUNG
   * Header/footer phải giống nhau trên Cloudflare Pages và ESP32.
   * ========================================================= */
  const CSS = `
    html,body{min-height:100%;}
    body{margin:0 !important;background:var(--bg,#0c130c);color:var(--t,#f5f8ee);}
    .kgx-header{width:100%;box-sizing:border-box;padding:14px 20px;border-bottom:1px solid var(--l,#31432c);display:flex;align-items:center;justify-content:space-between;gap:16px;background:var(--bg,#0c130c);color:var(--t,#f5f8ee);position:relative;z-index:1000;}
    .kgx-brand{flex:1 1 auto;min-width:0;font:600 1.35rem Fraunces,'Be Vietnam Pro',system-ui,sans-serif;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
    .kgx-brand small{display:block;color:var(--m,#a7b69c);font:500 .65rem 'Be Vietnam Pro',system-ui,sans-serif;letter-spacing:1.4px;text-transform:uppercase;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
    .kgx-actions{flex:0 0 auto;display:grid;grid-template-columns:210px 112px 104px;align-items:center;gap:8px;white-space:nowrap}
    .kgx-pill,.kgx-badge{height:40px;box-sizing:border-box;display:flex;align-items:center;justify-content:center;border:1px solid var(--l,#31432c);background:var(--p,#182016);border-radius:99px;padding:0 12px;color:var(--m,#a7b69c);text-decoration:none;font-size:.8rem;overflow:hidden;text-overflow:ellipsis}
    .kgx-dot{display:inline-block;flex:0 0 8px;width:8px;height:8px;border-radius:50%;background:#f87171;margin-right:7px}.kgx-dot.on{background:#4ade80}
    .kgx-nav{position:relative;width:104px;height:40px}.kgx-nav-toggle{width:104px;height:40px;border:1px solid var(--l,#31432c);background:var(--p,#182016);border-radius:99px;padding:0 13px;color:var(--t,#f5f8ee);font:inherit;font-size:.8rem;cursor:pointer;white-space:nowrap}
    .kgx-nav-menu{display:none;position:absolute;right:0;top:48px;width:220px;min-width:220px;padding:7px;background:var(--p,#182016);border:1px solid var(--l,#31432c);border-radius:14px;box-shadow:0 14px 35px rgba(0,0,0,.35);z-index:10000}.kgx-nav.open .kgx-nav-menu{display:grid;gap:3px}
    .kgx-nav-menu a{display:block;padding:10px 12px;border-radius:9px;color:var(--t,#f5f8ee);text-decoration:none;font-size:.82rem;white-space:nowrap}.kgx-nav-menu a:hover,.kgx-nav-menu a:focus{background:var(--p2,#1e2a1a);color:#4ade80;outline:0}.kgx-nav-menu a.active{background:rgba(74,222,128,.14);color:#4ade80}
    .kgx-footer{width:100%;box-sizing:border-box;max-width:1100px;margin:24px auto 0;padding:18px 20px;border-top:1px solid var(--l,#31432c);text-align:center;color:var(--m,#a7b69c);font-size:.78rem;clear:both}.kgx-footer a{color:#4ade80;text-decoration:none;font-weight:600}.kgx-footer a:hover{text-decoration:underline}
    body>.bar,body>header.top,body>header.bar,body>.top,body>.heading{display:none !important}body>.back-link{display:none !important}
    body.kgx-common-layout{display:block !important;padding:0 !important;min-height:100vh}body.kgx-common-layout>.kgx-page-content{display:block}
    @media(max-width:760px){.kgx-header{padding:12px 14px;display:block}.kgx-brand{display:block;width:100%;font-size:1.22rem;margin-bottom:10px}.kgx-actions{width:100%;grid-template-columns:minmax(0,1fr) 96px 96px;gap:7px}.kgx-pill,.kgx-badge,.kgx-nav,.kgx-nav-toggle{height:38px}.kgx-nav{width:96px}.kgx-nav-toggle{width:96px}.kgx-nav-menu{position:fixed;right:12px;top:112px;width:min(230px,calc(100vw - 24px));min-width:0}.kgx-footer{margin-top:18px;padding:16px 12px}}
    @media(max-width:390px){.kgx-actions{grid-template-columns:minmax(0,1fr) 88px 88px}.kgx-nav,.kgx-nav-toggle{width:88px}.kgx-pill,.kgx-badge,.kgx-nav-toggle{font-size:.74rem;padding-left:8px;padding-right:8px}}
  `;

  const FALLBACK_HEADER = `<header class="kgx-header"><div class="kgx-brand">🌿 Không Gian Xanh<small>Giám sát môi trường phòng</small></div><div class="kgx-actions"><span class="kgx-pill"><span id="dot" class="kgx-dot"></span><span id="conn">Đang kiểm tra MQTT</span></span><span id="clock" class="kgx-pill">--:--:--</span><div class="kgx-nav" id="nav"><button class="kgx-nav-toggle" id="navToggle" type="button" aria-expanded="false" aria-controls="navMenu">☰ Menu</button><nav class="kgx-nav-menu" id="navMenu" aria-label="Điều hướng"><a href="/">🏠 Tổng quan</a><a href="/mqtt">📡 MQTT</a><a href="/tools">⚙️ Chức năng</a><a href="/wifi">📶 Wi-Fi</a><a href="/update">⬆️ Nâng cấp OTA</a></nav></div></div></header>`;
  const FALLBACK_FOOTER = `<footer class="kgx-footer">Powered by <a href="https://khahdihdz.github.io" target="_blank" rel="noopener noreferrer">khahdihdz.github.io</a></footer>`;

  function installCss(){if(document.getElementById('kgx-common-style'))return;const style=document.createElement('style');style.id='kgx-common-style';style.textContent=CSS;document.head.appendChild(style)}
  async function loadFragment(path,fallback){try{const r=await fetch(path,{cache:'no-store'});if(r.ok){const t=await r.text();if(t.trim())return t}}catch(e){}return fallback}
  function setHeaderState(on,text){const dot=document.getElementById('dot'),conn=document.getElementById('conn');if(dot)dot.className='kgx-dot'+(on?' on':'');if(conn)conn.textContent=text}

  function normalizeRoutes(){
    const esp32=!location.hostname.endsWith('.pages.dev');
    const map=esp32?{'/mqtt':'/mqtt_dashboard.html','/tools':'/tools.html','/wifi':'/wifi_config.html','/update':'/update'}:{'/mqtt':'/mqtt','/tools':'/tools','/wifi':'/wifi','/update':'/update'};
    document.querySelectorAll('#navMenu a').forEach(link=>{
      const raw=link.getAttribute('href')||'';
      const path=new URL(raw,location.href).pathname;
      // OTA luôn là route /update. Tuyệt đối không dùng /update.html làm link điều hướng.
      if(path==='/update.html'||path==='/update')link.setAttribute('href','/update');
      else if(map[path])link.setAttribute('href',map[path]);
    });
  }

  function bindNav(){
    const nav=document.getElementById('nav'),toggle=document.getElementById('navToggle');if(!nav||!toggle)return;
    normalizeRoutes();if(nav.dataset.bound==='1')return;nav.dataset.bound='1';
    toggle.addEventListener('click',e=>{e.stopPropagation();const open=nav.classList.toggle('open');toggle.setAttribute('aria-expanded',open?'true':'false')});
    document.addEventListener('click',e=>{if(!nav.contains(e.target)){nav.classList.remove('open');toggle.setAttribute('aria-expanded','false')}});
    document.addEventListener('keydown',e=>{if(e.key==='Escape'){nav.classList.remove('open');toggle.setAttribute('aria-expanded','false')}});
    const current=location.pathname.replace(/\/$/,'')||'/';nav.querySelectorAll('a').forEach(link=>{const href=new URL(link.href,location.href).pathname.replace(/\/$/,'')||'/';if(href===current)link.classList.add('active')});
  }

  function loadMqttLibrary(){if(window.mqtt)return Promise.resolve(window.mqtt);if(window.__kgxMqttLoader)return window.__kgxMqttLoader;window.__kgxMqttLoader=new Promise((resolve,reject)=>{const s=document.createElement('script');s.src='https://unpkg.com/mqtt/dist/mqtt.min.js';s.async=true;s.onload=()=>resolve(window.mqtt);s.onerror=reject;document.head.appendChild(s)});return window.__kgxMqttLoader}
  async function connectHeaderMqtt(){try{let cfg={};try{cfg=JSON.parse(localStorage.getItem('kgx_mqtt_personal_v2')||'{}')}catch(e){}const pass=sessionStorage.getItem('kgx_mqtt_password_session')||'';if(!cfg.host||!cfg.device||!cfg.user||!pass){setHeaderState(false,'Chưa cấu hình MQTT');return}const mqtt=await loadMqttLibrary();if(!mqtt){setHeaderState(false,'MQTT chưa sẵn sàng');return}if(window.__kgxHeaderMqtt){try{window.__kgxHeaderMqtt.end(true)}catch(e){}}const port=Number(cfg.port)||8884,topic=`khonggianxanh/${cfg.device}/telemetry`;const client=mqtt.connect(`wss://${cfg.host}:${port}/mqtt`,{username:cfg.user,password:pass,clientId:'kgx-header-'+Math.random().toString(16).slice(2),clean:false,keepalive:30,reconnectPeriod:3000,connectTimeout:10000,resubscribe:true});window.__kgxHeaderMqtt=client;client.on('connect',()=>setHeaderState(true,'MQTT đã kết nối'));client.on('reconnect',()=>setHeaderState(false,'Đang kết nối lại MQTT...'));client.on('offline',()=>setHeaderState(false,'MQTT offline'));client.on('error',()=>setHeaderState(false,'MQTT lỗi'));client.on('message',(t,p)=>{if(t!==topic)return;try{const d=JSON.parse(p.toString()),ts=Number(d.timestamp);if(Number.isFinite(ts)&&ts>0){const clock=document.getElementById('clock');if(clock)clock.textContent=new Date(ts*1000).toLocaleTimeString('vi-VN')}}catch(e){}});client.subscribe(topic,{qos:1})}catch(e){setHeaderState(false,'MQTT không khả dụng')}}

  async function init(){
    installCss();document.body.classList.add('kgx-common-layout');
    document.querySelectorAll('body>.bar,body>header.top,body>header.bar,body>.top,body>.heading,body>.back-link').forEach(n=>n.remove());
    document.querySelectorAll('body>footer').forEach(n=>{if(!n.classList.contains('kgx-footer'))n.remove()});
    const header=await loadFragment('/header.html',FALLBACK_HEADER),footer=await loadFragment('/footer.html',FALLBACK_FOOTER);
    const hw=document.createElement('div');hw.innerHTML=header.trim();const h=hw.firstElementChild;if(h)document.body.insertBefore(h,document.body.firstChild);
    const fw=document.createElement('div');fw.innerHTML=footer.trim();const f=fw.firstElementChild;if(f)document.body.appendChild(f);
    bindNav();connectHeaderMqtt();
  }
  if(document.readyState==='loading')document.addEventListener('DOMContentLoaded',init,{once:true});else init();
})();
