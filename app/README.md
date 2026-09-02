# 이진견적서 - Electron 버전 (1단계: 기본 패키징 + 자동업데이트 뼈대)

## 폴더 구조

```
이진견적서-electron/
├─ main.js          # 메인 프로세스 (창 생성, 콘텐츠 로드, 업데이트 로직)
├─ preload.js        # 업데이트 알림 배너 (기존 index.html은 절대 수정하지 않음)
├─ package.json      # electron-builder 설정
├─ app/
│   └─ index.html    # 지금 쓰시는 이진견적서.html 원본 (오프라인 최초 실행용 "기본 버전")
└─ build/
    └─ icon.ico / icon.png   # 프로그램 아이콘 (직접 넣어주셔야 합니다, 현재 없음)
```

이미 이 상태로 `npm install` 후 `npm start` 하면 지금 쓰시는 HTML이 그대로 exe 창 안에서 동작합니다.
(로컬에서 AppImage로 빌드 테스트까지 완료했고, 정상적으로 패키징됩니다.)

---

## 왜 데이터가 안전한가

앱은 매번 `app/index.html`을 직접 여는 게 아니라, 최초 실행 시 그걸
`(사용자 데이터 폴더)/content/index.html` 로 복사해두고 **항상 이 고정된 경로**를 엽니다.

업데이트할 때는 이 파일의 **내용만 새 버전으로 교체**하고 창을 새로고침합니다.
파일 경로(origin)가 절대 안 바뀌기 때문에, 브라우저의 localStorage에 저장된
견적서/재고/거래처 데이터는 업데이트 전후로 그대로 유지됩니다.

---

## 자동업데이트 설정 방법 (GitHub Private 저장소)

**저장소**: https://github.com/fb01042731972/01042731972 (Private)

Private 저장소는 `raw.githubusercontent.com` 링크가 그냥 열리지 않습니다.
그래서 GitHub API를 "토큰"으로 인증해서 파일을 읽어오는 방식으로 만들어뒀습니다.
이미 `main.js`에 저장소 주소는 반영해뒀고, **토큰 발급 및 배치만 하시면 됩니다.**

### 1. Personal Access Token 발급 (한 번만)

1. github.com 로그인 상태에서 우측 상단 프로필 클릭 → **Settings**
2. 맨 아래 **Developer settings** 클릭
3. **Personal access tokens** → **Fine-grained tokens** → **Generate new token**
4. 설정:
   - **Repository access**: "Only select repositories" → `01042731972` 선택
   - **Permissions** → Repository permissions → **Contents**: **Read-only** 로 설정 (이것만 켜면 됩니다)
5. **Generate token** 클릭 → `github_pat_...` 로 시작하는 토큰이 딱 한 번 보여집니다. **그 자리에서 복사해두세요** (나중에 다시 못 봅니다)

### 2. 토큰을 프로그램이 읽을 수 있는 위치에 저장

exe 파일이 있는 폴더에 **`gh-token.txt`** 파일을 만들고, 그 안에 토큰 문자열만 붙여넣기 하세요.
(줄바꿈 없이 토큰 한 줄만)

```
exe가 설치된 폴더/
├─ 이진견적서.exe
└─ gh-token.txt   ← 여기에 토큰 붙여넣기
```

이 파일은 **절대 GitHub에 올리지 않습니다** (`.gitignore`에 이미 등록해뒀습니다).
직원들 PC 각각에도 이 파일을 하나씩 넣어줘야 합니다 (설치 시 같이 배포).

> 💡 만약 매번 각 PC에 토큰 파일 심는 게 번거로우시면, 나중에 저장소를 Public으로 전환하는 방법도 있습니다.
> Public으로 바꾸면 토큰 없이도 동작하도록 코드를 간단히 바꿔드릴 수 있어요 (프로그램 파일만 공개되는 것이라 데이터 유출 위험은 없습니다).

### 3. 이 프로젝트를 저장소에 처음 push 하기

```bash
git init
git add .
git commit -m "initial"
git branch -M main
git remote add origin https://github.com/fb01042731972/01042731972.git
git push -u origin main
```

### 4. 배포할 때마다 하실 일 (지금 워크플로와 거의 동일)

1. `app/index.html` 수정 (지금 저와 하시던 것)
2. 그 파일을 `releases/index-1.1.0.html` 같은 새 이름으로 복사
3. `releases/manifest.json`의 `version`, `path`, `notes` 갱신 (path는 `releases/index-1.1.0.html`처럼)
4. `git add . && git commit -m "v1.1.0" && git push`
5. 끝 — 몇 초~몇 분 내로 직원들 프로그램에 업데이트 배너가 뜹니다

### 5. Windows exe 자체 배포 (GitHub Actions 자동 빌드)

`.github/workflows/build.yml`을 추가해뒀습니다. GitHub에 push할 때마다
자동으로 Windows용 `.exe`(설치형 + portable)를 만들어서 Actions 탭의
"Artifacts"에 올려줍니다. 즉 **민희 PC에 Windows가 없어도** exe를 뽑을 수 있습니다.

- 저장소 → **Actions** 탭 → 최근 빌드 클릭 → 하단 Artifacts에서 다운로드
- 다운로드한 exe와 `gh-token.txt`를 같은 폴더에 두고 직원들에게 배포(설치)하면,
  이후 내용 업데이트는 위 4번 과정만으로 자동 반영됩니다.

---

## Windows exe 빌드 방법 (중요)

**Windows용 exe/installer는 Linux 환경(지금 이 작업 환경 포함)에서는 만들 수 없습니다.**
Windows 코드 서명 도구가 Wine을 필요로 하는데, 이 샌드박스에는 Wine이 없습니다.
(실제로 시도해봤고 `wine is required` 에러로 확인했습니다. Linux용 AppImage 빌드는 정상 성공했습니다.)

**Windows exe를 만드는 방법 3가지:**

### 방법 A. 민희 PC(Windows)에서 직접 빌드 — 가장 간단
```bash
npm install
npm run dist:win          # 인스톨러(.exe) + portable(.exe) 둘 다 생성
```
`dist/` 폴더에 결과물이 생깁니다.

### 방법 B. GitHub Actions로 자동 빌드 (추천 — 매번 수동 안 해도 됨)
GitHub 저장소에 올려두면 push할 때마다 Windows용 exe를 자동으로 만들어줍니다.
필요하시면 이 워크플로 파일(.github/workflows/build.yml)도 만들어드릴 수 있습니다.

### 방법 C. Mac이나 Linux에 Wine 설치 후 빌드
```bash
sudo apt install wine  # 또는 brew install wine
npm run dist:win
```

---

## 다음 단계 (아직 안 한 것)

- [ ] 아이콘 파일 (`build/icon.ico`) 준비
- [ ] 실제 배포 서버 주소 확정 후 `main.js`에 반영
- [ ] SQLite 전환 (localStorage 용량 한계 대비)
- [ ] 프린터/스캐너 하드웨어 직결
- [ ] 코드 서명 (Windows "알 수 없는 게시자" 경고 제거, 선택사항)
