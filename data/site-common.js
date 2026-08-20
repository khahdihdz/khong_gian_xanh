/* Không Gian Xanh - shared site chrome. Include this file on every HTML page. */
(function () {
  'use strict';
  const NAV = [
    ['dashboard', '⌂', 'Tổng quan', '/#dashboard'],
    ['history', '▥', 'Lịch sử', '/#history'],
    ['system', '⚙', 'Hệ thống', '/#system'],
    ['ota', '↻', 'Cập nhật', '/update.html']
  ];

  function injectCalibration() {
    if (location.pathname !== '/' && location.pathname !== '/index.html') return;
    if (document.getElementById('calibrationPanel')) return;
    const style = document.createElement('style');
    style.textContent = '.cal-panel{margin:14px 0}.cal-grid{display:grid;grid-template-columns:repeat(2,1fr);gap:10px}.cal-box{background:var(--panel2,#172a33);border:1px solid var(--line,#29404a);border-radius:14px;padding:12px}.cal-box label{display:block;color:var(--muted,#9db2ba);font-size:.75rem;margin-bottom:5px}.cal-box input{width:100%;padding:9px;border-radius:9px;border:1px solid var(--line,#29404a);background:#0b171c;color:var(--text,#eef7fa);font-size:1rem}.cal-actions{display:flex;gap:8px;flex-wrap:wrap;margin-top:10px}.cal-msg{color:var(--muted,#9db2ba);font-size:.78rem;margin-top:8px}.cal-note{color:var(--muted,#9db2ba);font-size:.74rem;line-height:1.45;margin-top:8px}@media(max-width:600px){.cal-grid{grid-template-columns:1fr}}';
    document.head.appendChild(style);
    const panel = document.createElement('section');
    panel.id = 'calibrationPanel'; panel.className = 'panel cal-panel';
    panel.innerHTML = '<div class="toolbar"><div><strong>🎯 Hiệu chỉnh cảm biến</strong><div class="sub">Hiệu chỉnh SHT31 ngay trên dashboard · lưu vào bộ nhớ ESP32</div></div></div>' +
      '<div class="cal-grid">' +
      '<div class="cal-box"><label>Nhiệt độ tham chiếu (°C)</label><input id="calRefTemp" type="number" step="0.1" placeholder="Ví dụ 28.5"></div>' +
      '<div class="cal-box"><label>Độ ẩm tham chiếu (%RH)</label><input id="calRefHum" type="number" step="0.1" placeholder="Ví dụ 62.0"></div>' +
      '</div>' +
      '<div class="cal-actions"><button class="btn active" id="calApply">Hiệu chỉnh theo giá trị tham chiếu</button><button class="btn" id="calReset">Đặt về 0</button></div>' +
      '<div class="cal-msg" id="calMsg">Đang đọc hiệu chỉnh hiện tại…</div>' +
      '<div class="cal-note">Cách dùng: đặt ESP32 cạnh nhiệt ẩm kế tham chiếu, chờ số đo ổn định rồi nhập giá trị chuẩn. Hệ thống tính offset = giá trị tham chiếu − giá trị SHT31 hiện tại. Không nên hiệu chỉnh nếu chưa có thiết bị tham chiếu đáng tin cậy.</div>';
    const main = document.querySelector('main.wrap');
    if (main) main.appendChild(panel); else document.body.appendChild(panel);

    const msg = document.getElementById('calMsg');
    async function loadCal(){ try { const r=await fetch('/api/calibration',{cache:'no-store'}); const d=await r.json(); msg.textContent='Offset hiện tại: nhiệt độ '+Number(d.temperature_offset).toFixed(2)+' °C · độ ẩm '+Number(d.humidity_offset).toFixed(2)+' %RH'; } catch(e){msg.textContent='Không đọc được trạng thái hiệu chỉnh.';} }
    document.getElementById('calApply').onclick = async function(){
      const rt=parseFloat(document.getElementById('calRefTemp').value), rh=parseFloat(document.getElementById('calRefHum').value);
      if(!Number.isFinite(rt)||!Number.isFinite(rh)){msg.textContent='Hãy nhập đủ nhiệt độ và độ ẩm tham chiếu.';return;}
      try{
        const cur=await (await fetch('/api/status',{cache:'no-store'})).json();
        const t=Number(cur.temperature), h=Number(cur.humidity);
        if(!Number.isFinite(t)||!Number.isFinite(h)){msg.textContent='SHT31 chưa có số đo hợp lệ.';return;}
        const to=rt-t, ho=rh-h;
        if(to < -20 || to > 20 || ho < -30 || ho > 30){msg.textContent='Sai lệch quá lớn. Kiểm tra lại cảm biến hoặc giá trị tham chiếu.';return;}
        const body=new URLSearchParams({temperature_offset:to.toFixed(2),humidity_offset:ho.toFixed(2)});
        const res=await fetch('/api/calibration',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body});
        const d=await res.json(); msg.textContent=d.message||'Đã lưu.'; if(res.ok) setTimeout(loadCal,300);
      }catch(e){msg.textContent='Không thể lưu hiệu chỉnh.';}
    };
    document.getElementById('calReset').onclick = async function(){
      try{const r=await fetch('/api/calibration/reset',{method:'POST'});const d=await r.json();msg.textContent=d.message||'Đã reset.';loadCal();}catch(e){msg.textContent='Không thể reset hiệu chỉnh.';}
    };
    loadCal();
  }

  function inject() {
    const root = document.body;
    if (!root || document.querySelector('[data-site-header]')) return;
    const header = document.createElement('header'); header.className='site-header'; header.dataset.siteHeader='1';
    header.innerHTML='<div class="site-header-inner"><a class="site-brand" href="/#dashboard" aria-label="Trang chủ Không Gian Xanh"><span class="site-brand-mark">🌿</span><span><strong>Không Gian Xanh</strong><small>Giám sát môi trường</small></span></a><button class="site-menu-toggle" type="button" aria-label="Mở menu" aria-expanded="false">☰</button><nav class="site-nav" aria-label="Điều hướng chính">'+NAV.map(([id,icon,label,href])=>`<a class="site-nav-link" data-nav="${id}" href="${href}"><span>${icon}</span>${label}</a>`).join('')+'</nav></div>';
    const footer=document.createElement('footer'); footer.className='site-footer'; footer.dataset.siteFooter='1'; footer.innerHTML='<div class="site-footer-inner"><span>Không Gian Xanh · ESP32</span><span>Powered by <a href="https://khahdihdz.github.io" target="_blank" rel="noopener noreferrer"><strong>khahdihdz.github.io</strong></a></span><span>Firmware <span id="siteFirmware">--</span></span></div>';
    root.insertBefore(header,root.firstChild); root.appendChild(footer);
    const toggle=header.querySelector('.site-menu-toggle'),nav=header.querySelector('.site-nav');
    toggle.addEventListener('click',()=>{const open=nav.classList.toggle('open');toggle.setAttribute('aria-expanded',open?'true':'false');});
    nav.addEventListener('click',e=>{if(e.target.closest('a')){nav.classList.remove('open');toggle.setAttribute('aria-expanded','false');}});
    const path=location.pathname,hash=location.hash.replace('#','');let active=hash||(path==='/'||path==='/index.html'?'dashboard':'');if(path==='/update.html'||path==='/update')active='ota';const activeLink=header.querySelector(`[data-nav="${active}"]`);if(activeLink)activeLink.classList.add('active');
    fetch('/api/info',{cache:'no-store'}).then(r=>r.ok?r.json():null).then(info=>{if(info&&info.firmware_version){const el=document.getElementById('siteFirmware');if(el)el.textContent=info.firmware_version;}}).catch(()=>{});
    setTimeout(injectCalibration,0);
  }
  if(document.readyState==='loading')document.addEventListener('DOMContentLoaded',inject);else inject();
})();
