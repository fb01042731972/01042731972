// ============================================================
// 이진견적서 - Electron 메인 프로세스
// ============================================================
// 구조 설명:
//   - app/index.html            : 앱에 내장된 "기본" 버전 (오프라인 최초 실행용)
//   - userData/content/index.html : 실제로 매번 로드되는 "활성" 버전
//                                    (최초 실행 시 app/index.html을 복사해서 만듦)
//   - userData/content/version.json : 현재 적용된 버전 정보
//
// 왜 이렇게 나누나?
//   BrowserWindow가 항상 같은 파일 경로(userData/content/index.html)를
//   로드하도록 고정해야 localStorage/IndexedDB의 origin이 유지됩니다.
//   업데이트할 때는 "이 파일의 내용"만 새 버전으로 덮어쓰고 reload 하기
//   때문에, 사용자가 쌓아둔 견적서/재고/거래처 데이터는 그대로 보존됩니다.
// ============================================================

const { app, BrowserWindow, ipcMain, Menu, shell } = require('electron');
const path = require('path');
const fs = require('fs');

// ---- 배포 서버 설정 (GitHub Private 저장소) ----------------------
// 저장소: https://github.com/fb01042731972/01042731972 (Private)
//
// Private 저장소는 raw.githubusercontent.com 링크가 그냥은 안 열립니다.
// 그래서 GitHub API(api.github.com)로 "인증 토큰을 들고" 파일을 읽어옵니다.
//
// 토큰(Personal Access Token) 발급 방법은 README.md의
// "Private 저장소 토큰 발급" 항목을 참고하세요.
// 토큰은 절대 이 코드에 직접 적지 않고, 사용자 PC의 별도 파일에서 읽습니다:
//   (실행파일 폴더)/gh-token.txt   또는
//   (userData 폴더)/gh-token.txt
const GITHUB_OWNER = 'fb01042731972';
const GITHUB_REPO = '01042731972';
const GITHUB_BRANCH = 'main';
const MANIFEST_PATH = 'releases/manifest.json';

function readGithubToken() {
  const candidates = [
    path.join(process.cwd(), 'gh-token.txt'),
    path.join(app.getPath('userData'), 'gh-token.txt'),
    path.join(__dirname, 'gh-token.txt'),
  ];
  for (const p of candidates) {
    try {
      const t = fs.readFileSync(p, 'utf-8').trim();
      if (t) return t;
    } catch (e) { /* 다음 후보 확인 */ }
  }
  return null;
}

function githubApiUrl(repoPath) {
  return `https://api.github.com/repos/${GITHUB_OWNER}/${GITHUB_REPO}/contents/${repoPath}?ref=${GITHUB_BRANCH}`;
}

function githubHeaders() {
  const token = readGithubToken();
  const headers = {
    'Accept': 'application/vnd.github.raw',
    'User-Agent': 'ejin-quotation-app',
  };
  if (token) headers['Authorization'] = `token ${token}`;
  return { headers, hasToken: !!token };
}
// ---------------------------------------------------------------

const CONTENT_DIR = path.join(app.getPath('userData'), 'content');
const CONTENT_HTML = path.join(CONTENT_DIR, 'index.html');
const VERSION_FILE = path.join(CONTENT_DIR, 'version.json');
const BUNDLED_HTML = path.join(__dirname, 'app', 'index.html');

let mainWindow = null;

function readLocalVersion() {
  try {
    const raw = fs.readFileSync(VERSION_FILE, 'utf-8');
    return JSON.parse(raw);
  } catch (e) {
    return { version: '0.0.0' };
  }
}

function writeLocalVersion(info) {
  fs.mkdirSync(CONTENT_DIR, { recursive: true });
  fs.writeFileSync(VERSION_FILE, JSON.stringify(info, null, 2), 'utf-8');
}

function ensureContentExists() {
  fs.mkdirSync(CONTENT_DIR, { recursive: true });
  if (!fs.existsSync(CONTENT_HTML)) {
    fs.copyFileSync(BUNDLED_HTML, CONTENT_HTML);
    writeLocalVersion({ version: app.getVersion(), source: 'bundled' });
  }
}

// 간단한 semver 비교 (major.minor.patch 형태만 지원)
function isNewer(remote, local) {
  const a = String(remote).split('.').map(Number);
  const b = String(local).split('.').map(Number);
  for (let i = 0; i < Math.max(a.length, b.length); i++) {
    const x = a[i] || 0, y = b[i] || 0;
    if (x > y) return true;
    if (x < y) return false;
  }
  return false;
}

function createWindow() {
  ensureContentExists();

  mainWindow = new BrowserWindow({
    width: 1400,
    height: 900,
    icon: path.join(__dirname, 'build', 'icon.png'),
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
      contextIsolation: true,
      nodeIntegration: false,
      sandbox: false, // localStorage/카메라 접근 등 기존 HTML 기능 유지를 위해 false
    },
  });

  Menu.setApplicationMenu(null); // 업무용 앱이므로 기본 메뉴바 숨김

  mainWindow.loadFile(CONTENT_HTML);

  mainWindow.webContents.on('did-finish-load', () => {
    checkForUpdate(false);
  });

  // 새 창 열기 요청(발주서, QR 라벨, 캡처 등)은 시스템 브라우저 대신
  // Electron 자식 창으로 그대로 띄워줌 (기존 window.open 동작 유지)
  mainWindow.webContents.setWindowOpenHandler(({ url }) => {
    if (url.startsWith('http://') || url.startsWith('https://')) {
      shell.openExternal(url);
      return { action: 'deny' };
    }
    return { action: 'allow' };
  });
}

// ---- 업데이트 체크 ---------------------------------------------
async function checkForUpdate(manual) {
  try {
    const { headers, hasToken } = githubHeaders();
    if (!hasToken && manual) {
      mainWindow.webContents.send('ej:update-error', 'gh-token.txt 파일이 없습니다 (Private 저장소는 토큰이 필요합니다). README를 참고해 토큰을 발급해주세요.');
    }
    const res = await fetch(githubApiUrl(MANIFEST_PATH), { headers, cache: 'no-store' });
    if (!res.ok) throw new Error('manifest fetch failed: ' + res.status + (res.status === 404 ? ' (경로 확인 필요)' : res.status === 401 || res.status === 403 ? ' (토큰 확인 필요)' : ''));
    const manifest = await res.json();
    const local = readLocalVersion();

    if (manifest.version && isNewer(manifest.version, local.version)) {
      mainWindow.webContents.send('ej:update-available', {
        version: manifest.version,
        notes: manifest.notes || '',
        forced: !!manifest.forced,
      });
      mainWindow._pendingManifest = manifest;
    } else if (manual) {
      mainWindow.webContents.send('ej:update-none');
    }
  } catch (e) {
    if (manual) {
      mainWindow.webContents.send('ej:update-error', String(e.message || e));
    }
    // 조용히 무시 (오프라인 상태일 수 있음)
  }
}

// 렌더러(배너의 "지금 업데이트" 버튼)에서 호출
ipcMain.handle('ej:apply-update', async () => {
  const manifest = mainWindow._pendingManifest;
  if (!manifest || !manifest.path) return { ok: false, error: '적용할 업데이트 정보가 없습니다.' };

  try {
    const { headers } = githubHeaders();
    const res = await fetch(githubApiUrl(manifest.path), { headers, cache: 'no-store' });
    if (!res.ok) throw new Error('다운로드 실패: ' + res.status);
    const newHtml = await res.text();

    // 손상된 파일을 받는 것을 방지하는 최소한의 안전장치
    if (!newHtml || newHtml.length < 1000 || !/<html/i.test(newHtml)) {
      throw new Error('받은 파일이 올바른 HTML이 아닙니다.');
    }

    // 롤백을 위해 이전 버전 백업
    const backupPath = path.join(CONTENT_DIR, `backup-${Date.now()}.html`);
    fs.copyFileSync(CONTENT_HTML, backupPath);
    pruneOldBackups();

    fs.writeFileSync(CONTENT_HTML, newHtml, 'utf-8');
    writeLocalVersion({ version: manifest.version, source: 'remote', updatedAt: new Date().toISOString() });

    mainWindow.loadFile(CONTENT_HTML);
    return { ok: true };
  } catch (e) {
    return { ok: false, error: String(e.message || e) };
  }
});

// 백업 파일은 최근 5개만 유지
function pruneOldBackups() {
  const files = fs.readdirSync(CONTENT_DIR)
    .filter(f => f.startsWith('backup-'))
    .sort();
  while (files.length > 5) {
    fs.unlinkSync(path.join(CONTENT_DIR, files.shift()));
  }
}

ipcMain.handle('ej:check-now', () => checkForUpdate(true));
ipcMain.handle('ej:get-version', () => readLocalVersion());

// ---- 앱 라이프사이클 -------------------------------------------
app.whenReady().then(createWindow);

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') app.quit();
});

app.on('activate', () => {
  if (BrowserWindow.getAllWindows().length === 0) createWindow();
});
