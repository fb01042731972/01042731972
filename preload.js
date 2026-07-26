// ============================================================
// Preload: 기존 index.html(26000+줄) 코드는 절대 건드리지 않고,
// 업데이트 알림 배너만 별도 레이어로 얹어서 표시합니다.
// ============================================================
const { ipcRenderer, contextBridge } = require('electron');

let pendingInfo = null;

function buildBanner(info) {
  if (document.getElementById('ej-update-banner')) return; // 중복 방지

  const banner = document.createElement('div');
  banner.id = 'ej-update-banner';
  banner.style.cssText = `
    position:fixed; top:0; left:0; right:0; z-index:2147483647;
    background:#1f2937; color:#fff; font-family:'Malgun Gothic',sans-serif;
    padding:10px 16px; display:flex; align-items:center; justify-content:center;
    gap:14px; font-size:14px; box-shadow:0 2px 8px rgba(0,0,0,.3);
  `;

  const label = document.createElement('span');
  label.textContent = `🔔 새 업데이트가 있습니다 (v${info.version})` + (info.notes ? ` — ${info.notes}` : '');
  label.style.cssText = 'flex:0 1 auto; overflow:hidden; text-overflow:ellipsis; white-space:nowrap; max-width:70vw;';

  const applyBtn = document.createElement('button');
  applyBtn.textContent = '지금 업데이트';
  applyBtn.style.cssText = `
    background:#2563eb; color:#fff; border:none; border-radius:6px;
    padding:6px 14px; font-size:13px; cursor:pointer; flex:0 0 auto;
  `;
  applyBtn.onclick = async () => {
    applyBtn.disabled = true;
    applyBtn.textContent = '적용 중...';
    label.textContent = '업데이트를 다운로드하고 있습니다. 잠시만 기다려 주세요...';
    const result = await ipcRenderer.invoke('ej:apply-update');
    if (!result.ok) {
      applyBtn.disabled = false;
      applyBtn.textContent = '지금 업데이트';
      label.textContent = `⚠ 업데이트 실패: ${result.error} (나중에 다시 시도해 주세요)`;
    }
    // 성공 시 main.js가 창을 reload하므로 이 배너 자체는 자연히 사라짐
  };

  banner.appendChild(label);
  banner.appendChild(applyBtn);

  if (!info.forced) {
    const laterBtn = document.createElement('button');
    laterBtn.textContent = '나중에';
    laterBtn.style.cssText = `
      background:transparent; color:#d1d5db; border:1px solid #4b5563; border-radius:6px;
      padding:6px 14px; font-size:13px; cursor:pointer; flex:0 0 auto;
    `;
    laterBtn.onclick = () => banner.remove();
    banner.appendChild(laterBtn);
  }

  // body 최상단에 삽입, 기존 레이아웃을 덮어쓰지 않도록 body에 상단 패딩 확보
  document.documentElement.style.setProperty('--ej-banner-offset', '0px');
  document.body.prepend(banner);
  requestAnimationFrame(() => {
    document.body.style.marginTop = banner.offsetHeight + 'px';
  });
}

ipcRenderer.on('ej:update-available', (_event, info) => {
  pendingInfo = info;
  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', () => buildBanner(info));
  } else {
    buildBanner(info);
  }
});

// 수동 "업데이트 확인" 버튼을 나중에 앱 안에 추가하고 싶을 때 쓸 수 있도록 노출
contextBridge.exposeInMainWorld('ejUpdater', {
  checkNow: () => ipcRenderer.invoke('ej:check-now'),
  getVersion: () => ipcRenderer.invoke('ej:get-version'),
});

ipcRenderer.on('ej:update-none', () => {
  console.log('[이진견적서] 이미 최신 버전입니다.');
});
ipcRenderer.on('ej:update-error', (_e, msg) => {
  console.warn('[이진견적서] 업데이트 확인 실패:', msg);
});
