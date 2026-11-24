/* ============================================================================
   Mega2560 Pro + DNMPB28(Eletechsup) PLC IO보드 전용 통합 펌웨어  (v2.0)
   ----------------------------------------------------------------------------
   PC 쪽 2560.html(웹 설정 도구)과 명령·응답 형식을 맞춘 버전입니다.

   ★ 올리기 전에 꼭 읽어주세요 ★

   [필요 라이브러리 — 아두이노 IDE 라이브러리 매니저에서 설치]
     - U8g2 (olikraus)      ← OLED 한글 표시용 (이전 버전의 Adafruit GFX/SSD1306은 이제 불필요)
     (Wire, EEPROM 은 기본 내장)
     ※ 한글 글꼴은 아래 OLED_FONT 로 고릅니다. 기본(korean1)은 흔한 한글 2350자이고, 표시가 안 되는
       글자가 있으면 u8g2_font_unifont_t_korean2 로 바꾸세요(전체 음절, 프로그램 용량이 훨씬 큼).
       OLED를 안 쓰면 아무것도 안 달아도 나머지 기능은 정상 동작합니다.

   [보드 고정 배선 — DNMPB28 공식 핀표 기준]
     릴레이 DO  CH1~CH8  (기계식 릴레이)  = 핀 40~47   (R1~R8)
     MOS   DO  D1~D8    (2803 오픈컬렉터) = 핀 32~39   (R9~R16)
     DI    IN1~IN8 (NPN)                  = 핀 16~23
     DI    IN9~IN16(PNP)                  = 핀 24~31   ← ★미검증(제품 페이지 표에 21~24로
                                             오타가 있음). 웹의 "RAWSCAN"(핀 자동 감지)으로
                                             실제 단자를 눌러 확인하고, 다르면 IN_PIN[] 값만 고치세요.
     AI 16채널 = A0~A15, 온보드 디지털튜브/버튼/LED = 3~14 (이 펌웨어에서는 사용 안 함)

   [입력 채널 번호 — 웹에서 보이는 CH번호와 보드 단자 대응]
     I2C(SDA=20, SCL=21)가 IN7·IN8 자리와 겹쳐서 이 두 단자는 쓰지 않습니다.
     웹의 입력 CH1~CH14 = 보드 IN1~IN6, IN9~IN16 순서입니다.
       CH1~CH6  = IN1~IN6 (핀 16,17,18,19,22,23)
       CH7~CH14 = IN9~IN16 (핀 24~31)

   [입력 극성]
     기본은 "핀이 LOW가 되면 눌림(ON)"입니다. 보드의 PNP 입력(IN9~16)이 반대로 나오면
     IN_ACTIVE_LOW[] 배열의 해당 칸을 false 로 바꾸세요.

   [추가 배선]
     OLED(SSD1306, I2C, 128x64, 주소 0x3C)  : SDA=20, SCL=21
     MCP23017(주소 0x20, I2C, 키패드·스텝 EN용): SDA=20, SCL=21 (OLED와 버스 공유)
       GPA0~GPA3 = 키패드 행1~4(출력), GPA4~GPA7 = 키패드 열1~4(입력,풀업)
       GPB0      = 스텝모터1 EN (active-LOW)
     초음파(HC-SR04) 1채널 : TRIG=50, ECHO=51
     로드셀(HX711)   1채널 : DOUT=52, SCK=53
     스텝모터 1채널        : STEP=2, DIR=15, EN=MCP23017 GPB0
     → 웹의 핀 설정 화면에서 MCP23017 쪽 핀은 "가상 핀 번호"로 보입니다:
       100~103 = 키패드 행, 104~107 = 키패드 열, 108 = 스텝1 EN

   [시리얼 사용 범위]
     USB 시리얼(Serial)만 사용합니다. 블루투스(Serial1=18/19)·ESP32 브리지(Serial2=16/17)는
     이 보드에서 해당 핀이 입력 단자(IN1~IN4)라서 쓸 수 없습니다.

   [이전 버전에서 고친 것]
     · PIN 명령은 응답을 보내지 않음(웹이 응답을 기다리지 않아 다음 명령 응답과 섞이던 문제)
     · SEQRUN 같은 "자발적" 출력 제거(웹의 응답 대기와 섞이던 문제). 자발 출력은 KEY 뿐입니다.
     · GETCONFIG? / LOAD? / ULTRA? 등 응답 형식을 웹이 읽는 형식으로 수정
     · 웹이 쓰는데 없던 명령 추가: SETRMODE/GETRMODE?, GETBOOT?, RAWPIN, RAWSCAN?,
       STEPSTATUS?, STEPSTOP, STEPEN, ULTRAMM, GETSTEPCONFIG?/GETULTRACONFIG?/GETLOADCONFIG?,
       GETLED?, GETLEDTRIG?, SEQGET?
     · 스텝모터를 타이머 인터럽트로 구동(이동 중에도 다른 명령 응답, 속도 단위 = step/s)
     · 릴레이 부팅 시 깜박임 제거, 부팅 3초 대기 제거(입력 상태는 부팅 때 읽어 기준으로 삼음)
     · 시퀀스·로드셀 보정·릴레이 방식·키패드 사용·OLED 문구를 EEPROM에 저장(체크섬 검사)
     · 초음파/로드셀 트리거 시퀀스를 보드 단독으로도 동작
     · 줄 길이 초과·잘못된 인자에 안전(예전엔 인자가 모자라면 오동작)

   [웹과 달라서 보드 단독에서는 안 되는 것 — 웹도 같은 안내를 띄웁니다]
     키패드 코드/신호 코드 트리거, 반복 실행·시작/정지 신호 N번, 계수계,
     OLED/로드셀 0점/초음파 켜기끄기/키패드 켜기끄기 동작 줄(릴레이·스텝모터만 저장됨)
   ============================================================================ */

#include <Wire.h>
#include <EEPROM.h>
#include <U8g2lib.h>
#if defined(__AVR__)
#include <avr/eeprom.h>
#endif

#define FW_VERSION "PLC-2560 v2.0"

/* ----------------------------- 하드웨어 상수 ----------------------------- */
#define RELAY_COUNT   16
#define INPUT_COUNT   14      // CH1~6 = IN1~6, CH7~14 = IN9~16 (IN7,IN8은 I2C 때문에 미사용)
#define ULTRA_COUNT   4       // 배열은 4채널분, 실제 배선은 1채널
#define LOAD_COUNT    4
#define STEP_COUNT    6
#define N_SEQ         14      // 웹의 BOARD_MAX_SEQ 와 같아야 함
#define MAX_STEPS     12      // 웹의 BOARD_MAX_STEPS 와 같아야 함
#define TEXT_MAX      119     // OLED 문구 최대 바이트(한글은 글자당 3바이트)
#define SEQ_NAME_MAX  11

const uint8_t RELAY_PIN[RELAY_COUNT] = {
  40,41,42,43,44,45,46,47,     // R1~R8  = CH1~CH8 (기계식 릴레이)
  32,33,34,35,36,37,38,39      // R9~R16 = D1~D8   (MOS)
};

// 웹 CH1~CH14 의 실제 핀. IN7·IN8(핀 20,21)은 I2C라서 빠져 있습니다.
const uint8_t IN_PIN[INPUT_COUNT] = {
  16,17,18,19,22,23,           // CH1~6  = IN1~IN6
  24,25,26,27,28,29,30,31      // CH7~14 = IN9~IN16 (★핀 번호 미검증 — 실측 후 필요시 수정)
};
// true = 핀이 LOW일 때 "눌림(ON)", false = HIGH일 때 눌림
const bool IN_ACTIVE_LOW[INPUT_COUNT] = {
  true,true,true,true,true,true,
  true,true,true,true,true,true,true,true
};

// MCP23017 (I2C 0x20)
#define MCP_ADDR   0x20
#define MCP_IODIRA 0x00
#define MCP_IODIRB 0x01
#define MCP_GPPUA  0x0C
#define MCP_GPIOA  0x12
#define MCP_OLATA  0x14
#define MCP_OLATB  0x15
// 웹에 보여 줄 MCP 가상 핀 번호
#define VPIN_KP_ROW0 100      // 100~103 = GPA0~3
#define VPIN_KP_COL0 104      // 104~107 = GPA4~7
#define VPIN_STEP1_EN 108     // GPB0

// 초음파/로드셀/스텝모터 — 1채널만 실배선(나머지 0 = 미사용)
const uint8_t ULTRA_TRIG[ULTRA_COUNT] = {50, 0, 0, 0};
const uint8_t ULTRA_ECHO[ULTRA_COUNT] = {51, 0, 0, 0};
const uint8_t LOAD_DOUT[LOAD_COUNT]   = {52, 0, 0, 0};
const uint8_t LOAD_SCK[LOAD_COUNT]    = {53, 0, 0, 0};
const uint8_t STEP_STEP[STEP_COUNT]   = {2, 0,0,0,0,0};
const uint8_t STEP_DIR[STEP_COUNT]    = {15,0,0,0,0,0};
const uint8_t STEP_EN_VPIN[STEP_COUNT]= {VPIN_STEP1_EN,0,0,0,0,0};   // 0 = EN 핀 없음

const char KEY_LABELS[16] = {'1','2','3','A','4','5','6','B','7','8','9','C','*','0','#','D'};

/* OLED (U8g2 — 한글 지원) */
#define OLED_FONT u8g2_font_unifont_t_korean1   // 흔한 한글 2350자. 빠지는 글자가 있으면 korean2(전체 음절, 용량 큼)
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

/* ----------------------------- 시퀀스 구조체 ------------------------------ */
struct Step {
  uint8_t  type;       // 0=릴레이 1=스텝모터
  uint8_t  target;     // type0: 릴레이 1~16 / type1: 모터 1~STEP_COUNT
  uint8_t  on;         // type0: 1=ON 0=OFF
  uint32_t delayMs;    // 동작(이동 완료) 뒤 대기 시간
  int32_t  steps;      // type1: 이동 스텝 수(부호 = 방향)
  uint16_t speed;      // type1: step/s
};
struct Seq {
  char     name[SEQ_NAME_MAX+1];
  uint8_t  trigSrc;    // 0=디지털 입력 1=초음파 2=로드셀
  uint8_t  trigCh;     // 입력 CH(1~14) / 초음파 채널 / 로드셀 채널
  uint8_t  trigCond;   // 0=OFF(놓을 때) 1=ON(누를 때) 2=양쪽
  uint8_t  distMode;   // 초음파/로드셀: 0=이하(<=) 1=이상(>=)
  int32_t  distVal;    // 초음파: cm / 로드셀: g
  uint32_t onMask;     // 추가 조건(인터록): 눌려 있어야 하는 채널 비트(비트 n = CH n+1)
  uint32_t offMask;    // 추가 조건(인터록): 떨어져 있어야 하는 채널 비트
  uint8_t  nSteps;     // 0 = 빈 슬롯
  Step     steps[MAX_STEPS];
};
Seq seqs[N_SEQ];

// 슬롯별 실행 상태(RAM 전용)
#define PH_IDLE 0
#define PH_RUN  1      // 다음 단계 실행 대기
#define PH_MOTOR 2     // 모터 이동 완료 대기
#define PH_DELAY 3     // 지연 대기
struct SeqRun { uint8_t phase; uint8_t idx; uint32_t until; int8_t prev; };   // prev: 값 트리거의 이전 판정(-1=아직 모름)
SeqRun seqRun[N_SEQ];

/* ----------------------------- 런타임 상태 -------------------------------- */
bool relayState[RELAY_COUNT+1];
bool relayActiveLow = false;
uint8_t relayBoot[RELAY_COUNT+1];

bool inStateRaw[INPUT_COUNT+1];
bool inStateDeb[INPUT_COUNT+1];
unsigned long inLastChange[INPUT_COUNT+1];
#define DEBOUNCE_MS 30UL

long  loadOffset[LOAD_COUNT+1];
float loadScale[LOAD_COUNT+1];     // 0 = 아직 보정 안 함

bool kpEnabled = false;
bool mcpPresent = false;
bool oledPresent = false;
uint8_t mcpOutB = 0x01;            // GPB 출력 그림자(bit0 = 스텝1 EN, 1=비활성)
uint8_t kpCount[16];
bool    kpDown[16];

char ledText[TEXT_MAX+1];
char ledTrigText[TEXT_MAX+1];
uint8_t  ledTrigCh = 0, ledTrigCond = 1;
uint16_t ledTrigSec = 3;
bool ledShowing = false;
unsigned long ledUntil = 0;

bool bootWarn = false;
bool lastWriteOk = true;

/* ------------------------------- EEPROM --------------------------------- */
#define EE_MAGIC_VAL   0xB7
#define EE_VERSION_VAL 2
#define EE_MAGIC     0
#define EE_VER       1
#define EE_RLOW      2
#define EE_KPEN      3
#define EE_BOOT      4        // 16바이트
#define EE_LEDTXT    20       // 120바이트
#define EE_LEDTRIG   140      // ch(1) cond(1) sec(2) text(120) = 124바이트
#define EE_LOADCAL   264      // offset int32 x4 + scale float x4 = 32바이트
#define EE_SEQCHK    296      // 시퀀스 체크섬 2바이트
#define EE_SEQ       298      // seqs 전체

bool     seqPersistPending = false;
uint16_t seqPersistPos = 0;
uint16_t seqPersistChk = 0;

uint16_t seqChecksum(){
  const uint8_t *p = (const uint8_t*)seqs;
  uint8_t a = 1, b = 0;                         // Fletcher-16 (mod 256) — 가볍고 충분함
  for (uint16_t i = 0; i < sizeof(seqs); i++){ a += p[i]; b += a; }
  return (uint16_t)(((uint16_t)b << 8) | a);
}
void markSeqDirty(){ seqPersistPending = true; seqPersistPos = 0; }   // 체크섬은 기록이 데이터 끝에 닿았을 때 계산

// 시퀀스 영역을 EEPROM에 조금씩(배경으로) 기록. 바뀐 바이트만 쓰고 체크섬은 마지막에 씀.
// blocking=true 이면 쓰기가 끝날 때까지 기다림(EXTMEM? 확인용).
void serviceEepromWriter(bool blocking){
  if (!seqPersistPending) return;
  uint16_t total = sizeof(seqs) + 2;
  uint8_t budget = 64;
  while (seqPersistPos < total && budget--){
    uint16_t addr, i = seqPersistPos;
    uint8_t want;
    if (i < sizeof(seqs)){ addr = EE_SEQ + i; want = ((const uint8_t*)seqs)[i]; }
    else {
      if (i == sizeof(seqs)) seqPersistChk = seqChecksum();    // 데이터를 다 쓴 뒤 최신 내용으로 계산
      addr = EE_SEQCHK + (i - sizeof(seqs));
      want = (i == sizeof(seqs)) ? (uint8_t)(seqPersistChk & 0xFF) : (uint8_t)(seqPersistChk >> 8);
    }
    if (EEPROM.read(addr) != want){
#if defined(__AVR__)
      if (!blocking && !eeprom_is_ready()) return;       // 아직 쓰는 중이면 다음 루프에
#endif
      EEPROM.update(addr, want);
      seqPersistPos++;
      if (!blocking) return;                             // 한 루프에 한 바이트만(블로킹 최소화)
    } else seqPersistPos++;
  }
  if (seqPersistPos >= total) seqPersistPending = false;
}
bool flushSeqPersist(unsigned long maxMs){
  unsigned long t0 = millis();
  while (seqPersistPending && millis() - t0 < maxMs) serviceEepromWriter(true);
  return !seqPersistPending;
}

void eeSaveRelayMode(){ EEPROM.update(EE_RLOW, relayActiveLow ? 1 : 0); }
void eeSaveKpEn(){ EEPROM.update(EE_KPEN, kpEnabled ? 1 : 0); }
void eeSaveBoot(int ch){ EEPROM.update(EE_BOOT + ch - 1, relayBoot[ch] ? 1 : 0); }
void eeSaveText(int addr, const char *s){
  for (int i = 0; i <= TEXT_MAX; i++){
    uint8_t c = (uint8_t)s[i];
    EEPROM.update(addr + i, c);
    if (c == 0) break;
  }
}
void eeLoadText(int addr, char *dst){
  for (int i = 0; i <= TEXT_MAX; i++) dst[i] = (char)EEPROM.read(addr + i);
  dst[TEXT_MAX] = 0;
  for (int i = 0; i < TEXT_MAX; i++) if ((uint8_t)dst[i] == 0xFF) { dst[i] = 0; break; }   // 비어 있던(0xFF) 칸 보호
}
void eeSaveLedTrig(){
  EEPROM.update(EE_LEDTRIG, ledTrigCh);
  EEPROM.update(EE_LEDTRIG + 1, ledTrigCond);
  EEPROM.update(EE_LEDTRIG + 2, (uint8_t)(ledTrigSec & 0xFF));
  EEPROM.update(EE_LEDTRIG + 3, (uint8_t)(ledTrigSec >> 8));
  eeSaveText(EE_LEDTRIG + 4, ledTrigText);
}
void eeSaveLoadCal(int ch){
  EEPROM.put(EE_LOADCAL + (ch - 1) * 4, loadOffset[ch]);
  EEPROM.put(EE_LOADCAL + 16 + (ch - 1) * 4, loadScale[ch]);
}
void clearAllSeqs(){
  memset(seqs, 0, sizeof(seqs));
  for (int s = 0; s < N_SEQ; s++){ seqRun[s].phase = PH_IDLE; seqRun[s].prev = -1; }
}

void eepromLoadAll(){
  clearAllSeqs();
  if (EEPROM.read(EE_MAGIC) != EE_MAGIC_VAL || EEPROM.read(EE_VER) != EE_VERSION_VAL){
    // 처음 부팅(또는 예전 버전의 EEPROM): 기본값으로 초기화
    relayActiveLow = false; kpEnabled = false;
    for (int i = 1; i <= RELAY_COUNT; i++){ relayBoot[i] = 0; eeSaveBoot(i); }
    ledText[0] = 0; eeSaveText(EE_LEDTXT, ledText);
    ledTrigCh = 0; ledTrigCond = 1; ledTrigSec = 3; ledTrigText[0] = 0; eeSaveLedTrig();
    for (int c = 1; c <= LOAD_COUNT; c++){ loadOffset[c] = 0; loadScale[c] = 0; eeSaveLoadCal(c); }
    eeSaveRelayMode(); eeSaveKpEn();
    markSeqDirty();                                   // 빈 시퀀스 + 체크섬을 배경으로 기록
    EEPROM.update(EE_VER, EE_VERSION_VAL);
    EEPROM.update(EE_MAGIC, EE_MAGIC_VAL);
    return;
  }
  relayActiveLow = EEPROM.read(EE_RLOW) != 0;
  kpEnabled = EEPROM.read(EE_KPEN) != 0;
  for (int i = 1; i <= RELAY_COUNT; i++) relayBoot[i] = EEPROM.read(EE_BOOT + i - 1) ? 1 : 0;
  eeLoadText(EE_LEDTXT, ledText);
  ledTrigCh = EEPROM.read(EE_LEDTRIG);
  ledTrigCond = EEPROM.read(EE_LEDTRIG + 1) ? 1 : 0;
  ledTrigSec = (uint16_t)EEPROM.read(EE_LEDTRIG + 2) | ((uint16_t)EEPROM.read(EE_LEDTRIG + 3) << 8);
  if (ledTrigSec < 1 || ledTrigSec > 3600) ledTrigSec = 3;
  if (ledTrigCh > INPUT_COUNT) ledTrigCh = 0;
  eeLoadText(EE_LEDTRIG + 4, ledTrigText);
  for (int c = 1; c <= LOAD_COUNT; c++){
    EEPROM.get(EE_LOADCAL + (c - 1) * 4, loadOffset[c]);
    EEPROM.get(EE_LOADCAL + 16 + (c - 1) * 4, loadScale[c]);
    if (!(loadScale[c] == loadScale[c])) loadScale[c] = 0;        // NaN 보호
  }
  // 시퀀스: 체크섬이 맞을 때만 사용
  uint8_t *p = (uint8_t*)seqs;
  for (uint16_t i = 0; i < sizeof(seqs); i++) p[i] = EEPROM.read(EE_SEQ + i);
  uint16_t stored = (uint16_t)EEPROM.read(EE_SEQCHK) | ((uint16_t)EEPROM.read(EE_SEQCHK + 1) << 8);
  bool bad = (stored != seqChecksum());
  if (!bad){
    for (int s = 0; s < N_SEQ; s++){
      if (seqs[s].nSteps > MAX_STEPS) { bad = true; break; }
      seqs[s].name[SEQ_NAME_MAX] = 0;
    }
  }
  if (bad){ clearAllSeqs(); bootWarn = true; markSeqDirty(); }    // 손상 → 비우고 깨끗한 상태를 다시 기록
  for (int s = 0; s < N_SEQ; s++){ seqRun[s].phase = PH_IDLE; seqRun[s].prev = -1; }
}

/* ---------------------------- MCP23017 최소 구현 -------------------------- */
bool mcpWrite(uint8_t reg, uint8_t val){
  Wire.beginTransmission(MCP_ADDR);
  Wire.write(reg); Wire.write(val);
  return Wire.endTransmission() == 0;
}
bool mcpReadA(uint8_t &val){
  Wire.beginTransmission(MCP_ADDR);
  Wire.write(MCP_GPIOA);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((int)MCP_ADDR, 1) != 1) return false;
  val = Wire.read();
  return true;
}
void mcpInit(){
  Wire.beginTransmission(MCP_ADDR);
  mcpPresent = (Wire.endTransmission() == 0);
  if (!mcpPresent) return;
  mcpWrite(MCP_IODIRA, 0xF0);     // GPA0~3 출력(행), GPA4~7 입력(열)
  mcpWrite(MCP_GPPUA, 0xF0);      // 열 풀업
  mcpWrite(MCP_OLATA, 0x0F);      // 행 전부 HIGH(대기)
  mcpWrite(MCP_IODIRB, 0xFE);     // GPB0 출력(스텝1 EN)
  mcpOutB = 0x01;                 // EN 비활성(active-LOW)
  mcpWrite(MCP_OLATB, mcpOutB);
}
void mcpSetStepEN(bool enable){
  if (!mcpPresent) return;
  if (enable) mcpOutB &= ~0x01; else mcpOutB |= 0x01;
  mcpWrite(MCP_OLATB, mcpOutB);
}
bool stepEnabledFlag[STEP_COUNT+1];

/* --------------------------------- 릴레이 -------------------------------- */
void writeRelayLevel(uint8_t ch){
  bool level = relayActiveLow ? !relayState[ch] : relayState[ch];
  digitalWrite(RELAY_PIN[ch-1], level ? HIGH : LOW);
}
void setRelay(uint8_t ch, bool on){
  if (ch < 1 || ch > RELAY_COUNT) return;
  relayState[ch] = on;
  writeRelayLevel(ch);
}

/* --------------------------------- OLED ----------------------------------- */
static uint8_t utf8Len(uint8_t c){
  if (c < 0x80) return 1;
  if ((c & 0xE0) == 0xC0) return 2;
  if ((c & 0xF0) == 0xE0) return 3;
  if ((c & 0xF8) == 0xF0) return 4;
  return 1;
}
void oledDraw(const char *text){
  if (!oledPresent) return;
  u8g2.clearBuffer();
  int x = 0, y = 0;
  const char *p = text;
  while (*p && y <= 48){
    uint8_t n = utf8Len((uint8_t)*p);
    char buf[5] = {0,0,0,0,0};
    uint8_t k = 0;
    while (k < n && p[k]) { buf[k] = p[k]; k++; }
    if (buf[0] == '\n'){ x = 0; y += 16; p += 1; continue; }
    int w = u8g2.getUTF8Width(buf);
    if (w <= 0) w = 8;
    if (x + w > 128){ x = 0; y += 16; if (y > 48) break; }
    u8g2.drawUTF8(x, y, buf);
    x += w;
    p += k;
  }
  u8g2.sendBuffer();
}
// 문자 경계를 지켜서 최대 maxBytes 바이트만 복사
void copyUtf8(char *dst, const char *src, int maxBytes){
  int n = 0;
  while (src[n] && n < maxBytes) n++;
  while (n > 0 && src[n] && ((uint8_t)src[n] & 0xC0) == 0x80) n--;   // 잘린 문자의 중간이면 뒤로
  memcpy(dst, src, n);
  dst[n] = 0;
}
void ledShowTemp(){
  if (ledTrigText[0] == 0) return;
  ledShowing = true;
  ledUntil = millis() + (unsigned long)ledTrigSec * 1000UL;
  oledDraw(ledTrigText);
}
void serviceLed(){
  if (ledShowing && (long)(millis() - ledUntil) >= 0){
    ledShowing = false;
    oledDraw(ledText);
  }
}

/* --------------------------------- 스텝모터 ------------------------------- */
// 타이머 인터럽트(20kHz)로 STEP 펄스를 만든다. 이동 중에도 loop()가 계속 돌아 명령에 응답한다.
#define STEP_TICK_HZ 20000UL
#define STEP_MAX_PPS 5000
volatile bool     stActive[STEP_COUNT+1];
volatile bool     stHigh[STEP_COUNT+1];
volatile int32_t  stRemain[STEP_COUNT+1];
volatile int32_t  stPos[STEP_COUNT+1];
volatile uint16_t stPeriod[STEP_COUNT+1];
volatile uint16_t stCnt[STEP_COUNT+1];
volatile int8_t   stDir[STEP_COUNT+1];

void stepTimerOn(){
#if defined(__AVR_ATmega2560__)
  noInterrupts();
  TCCR5A = 0; TCCR5B = _BV(WGM52) | _BV(CS51);          // CTC, 분주 8 → 2MHz
  OCR5A = (uint16_t)(2000000UL / STEP_TICK_HZ - 1);     // 20kHz
  TCNT5 = 0;
  TIMSK5 |= _BV(OCIE5A);
  interrupts();
#endif
}
void stepTimerOff(){
#if defined(__AVR_ATmega2560__)
  TIMSK5 &= ~_BV(OCIE5A);
#endif
}
void stepTick(){
  bool any = false;
  for (uint8_t c = 1; c <= STEP_COUNT; c++){
    if (!stActive[c]) continue;
    any = true;
    if (--stCnt[c]) continue;
    uint8_t sp = STEP_STEP[c-1];
    if (stHigh[c]){
      digitalWrite(sp, LOW); stHigh[c] = false;
      if (stRemain[c] <= 0){ stActive[c] = false; }
      else stCnt[c] = (stPeriod[c] > 1) ? (stPeriod[c] - 1) : 1;
    } else {
      digitalWrite(sp, HIGH); stHigh[c] = true;
      stRemain[c]--; stPos[c] += stDir[c];
      stCnt[c] = 1;
    }
  }
  if (!any) stepTimerOff();
}
#if defined(__AVR_ATmega2560__)
ISR(TIMER5_COMPA_vect){ stepTick(); }
#endif

bool stepWired(uint8_t ch){ return ch >= 1 && ch <= STEP_COUNT && STEP_STEP[ch-1] && STEP_DIR[ch-1]; }
bool stepMoving(uint8_t ch){ bool m; noInterrupts(); m = stActive[ch]; interrupts(); return m; }
void stepSetEnable(uint8_t ch, bool en){
  if (ch == 1) mcpSetStepEN(en);
  stepEnabledFlag[ch] = en;
}
void stepStop(uint8_t ch){
  if (ch < 1 || ch > STEP_COUNT) return;
  noInterrupts();
  stActive[ch] = false; stRemain[ch] = 0;
  if (stHigh[ch] && STEP_STEP[ch-1]) digitalWrite(STEP_STEP[ch-1], LOW);
  stHigh[ch] = false;
  interrupts();
}
// steps 부호 = 방향, pps = step/s. 이동 중이면 이전 이동을 멈추고 새 이동으로 바꾼다.
bool stepStart(uint8_t ch, long steps, long pps){
  if (!stepWired(ch)) return false;
  if (steps == 0) return true;
  if (pps < 1) pps = 1;
  if (pps > STEP_MAX_PPS) pps = STEP_MAX_PPS;
  stepStop(ch);
  stepSetEnable(ch, true);                                   // STEPMOVE 는 EN 을 자동으로 켠다
  digitalWrite(STEP_DIR[ch-1], steps > 0 ? HIGH : LOW);
  long n = steps > 0 ? steps : -steps;
  uint16_t period = (uint16_t)(STEP_TICK_HZ / (unsigned long)pps);
  if (period < 4) period = 4;
  noInterrupts();
  stDir[ch] = steps > 0 ? 1 : -1;
  stPeriod[ch] = period;
  stRemain[ch] = n;
  stHigh[ch] = false;
  stCnt[ch] = 120;                                           // 약 6ms: EN/DIR 안정 대기
  stActive[ch] = true;
  interrupts();
  stepTimerOn();
  return true;
}
void stepInfo(uint8_t ch, bool &moving, long &remain, long &pos){
  noInterrupts();
  moving = stActive[ch]; remain = stRemain[ch]; pos = stPos[ch];
  interrupts();
}
void stepZero(uint8_t ch){ if (ch >= 1 && ch <= STEP_COUNT){ noInterrupts(); stPos[ch] = 0; interrupts(); } }

/* --------------------------------- 입력 ---------------------------------- */
bool readInputActive(uint8_t ch){        // ch: 1~INPUT_COUNT
  bool low = (digitalRead(IN_PIN[ch-1]) == LOW);
  return IN_ACTIVE_LOW[ch-1] ? low : !low;
}
bool condsMatch(uint8_t s){
  const Seq &S = seqs[s];
  for (uint8_t ch = 1; ch <= 32; ch++){
    uint32_t b = 1UL << (ch - 1);
    bool state = (ch <= INPUT_COUNT) ? inStateDeb[ch] : false;   // 없는 채널은 OFF 로 취급
    if ((S.onMask & b) && !state) return false;
    if ((S.offMask & b) && state) return false;
  }
  return true;
}
void startSequence(uint8_t s){
  if (s >= N_SEQ) return;
  if (seqRun[s].phase != PH_IDLE) return;       // 이미 실행 중이면 무시
  if (seqs[s].nSteps == 0) return;
  seqRun[s].phase = PH_RUN; seqRun[s].idx = 0;
}
void abortSequence(uint8_t s){ if (s < N_SEQ) seqRun[s].phase = PH_IDLE; }

void onInputEdge(uint8_t ch, bool nowOn){
  if (ledTrigCh == ch && ledTrigCond == (nowOn ? 1 : 0)) ledShowTemp();
  for (uint8_t s = 0; s < N_SEQ; s++){
    const Seq &S = seqs[s];
    if (S.nSteps == 0 || S.trigSrc != 0 || S.trigCh != ch) continue;
    bool fire = (S.trigCond == 2) || (S.trigCond == 1 && nowOn) || (S.trigCond == 0 && !nowOn);
    if (!fire) continue;
    if (!condsMatch(s)) continue;
    startSequence(s);
  }
}
void scanInputs(){
  unsigned long now = millis();
  for (uint8_t ch = 1; ch <= INPUT_COUNT; ch++){
    bool raw = readInputActive(ch);
    if (raw != inStateRaw[ch]){ inStateRaw[ch] = raw; inLastChange[ch] = now; }
    if (inStateDeb[ch] != inStateRaw[ch] && now - inLastChange[ch] >= DEBOUNCE_MS){
      inStateDeb[ch] = inStateRaw[ch];
      onInputEdge(ch, inStateDeb[ch]);
    }
  }
}

/* --------------------------------- 초음파 --------------------------------- */
long ultraReadMm(uint8_t ch){            // ch: 1~4, 실패/미배선 = -1
  if (ch < 1 || ch > ULTRA_COUNT) return -1;
  uint8_t trig = ULTRA_TRIG[ch-1], echo = ULTRA_ECHO[ch-1];
  if (!trig || !echo) return -1;
  digitalWrite(trig, LOW); delayMicroseconds(3);
  digitalWrite(trig, HIGH); delayMicroseconds(10);
  digitalWrite(trig, LOW);
  unsigned long dur = pulseIn(echo, HIGH, 25000UL);
  if (dur == 0) return -1;
  return (long)((dur * 10UL + 29UL) / 58UL);
}
long ultraReadCm(uint8_t ch){ long mm = ultraReadMm(ch); return mm < 0 ? -1 : (mm + 5) / 10; }

/* --------------------------------- 로드셀 --------------------------------- */
// waitMs 동안 HX711 준비를 기다렸다가 24비트를 읽는다. 실패하면 false.
bool hxReadRaw(uint8_t ch, long &out, unsigned long waitMs){
  if (ch < 1 || ch > LOAD_COUNT) return false;
  uint8_t dout = LOAD_DOUT[ch-1], sck = LOAD_SCK[ch-1];
  if (!dout || !sck) return false;
  unsigned long t0 = millis();
  while (digitalRead(dout) != LOW){ if (millis() - t0 >= waitMs) return false; }
  long v = 0;
  for (uint8_t i = 0; i < 24; i++){
    digitalWrite(sck, HIGH); delayMicroseconds(1);
    v = (v << 1) | (digitalRead(dout) ? 1 : 0);
    digitalWrite(sck, LOW); delayMicroseconds(1);
  }
  digitalWrite(sck, HIGH); delayMicroseconds(1); digitalWrite(sck, LOW);   // 25번째 펄스: 채널A, 게인128
  v = (v ^ 0x800000L) - 0x800000L;                                          // 24비트 부호 확장
  out = v;
  return true;
}
bool hxReadAvg(uint8_t ch, long &out, uint8_t n){
  long sum = 0, v;
  for (uint8_t i = 0; i < n; i++){ if (!hxReadRaw(ch, v, 200)) return false; sum += v; }
  out = sum / n;
  return true;
}
// 그램 단위 정수. 핀 미설정/보정 전/읽기 실패 = false
bool loadReadGram(uint8_t ch, long &g, unsigned long waitMs){
  if (ch < 1 || ch > LOAD_COUNT || loadScale[ch] == 0) return false;
  long raw;
  if (!hxReadRaw(ch, raw, waitMs)) return false;
  float f = (float)(raw - loadOffset[ch]) / loadScale[ch];
  g = (long)(f >= 0 ? f + 0.5f : f - 0.5f);
  return true;
}

/* --------------------------------- 키패드 --------------------------------- */
void scanKeypad(){
  if (!kpEnabled || !mcpPresent) return;
  static unsigned long lastScan = 0;
  unsigned long now = millis();
  if (now - lastScan < 20) return;
  lastScan = now;
  for (uint8_t r = 0; r < 4; r++){
    mcpWrite(MCP_OLATA, (uint8_t)(~(1 << r)) & 0x0F);     // 해당 행만 LOW
    delayMicroseconds(40);
    uint8_t val;
    if (!mcpReadA(val)) continue;
    for (uint8_t c = 0; c < 4; c++){
      bool pressed = !((val >> (4 + c)) & 1);              // 열은 풀업, 눌리면 LOW
      uint8_t idx = r * 4 + c;
      if (pressed){
        if (kpCount[idx] < 255) kpCount[idx]++;
        if (kpCount[idx] == 2 && !kpDown[idx]){            // 연속 2번 감지(약 40ms)해야 인정
          kpDown[idx] = true;
          Serial.print(F("KEY ")); Serial.println(KEY_LABELS[idx]);
        }
      } else { kpCount[idx] = 0; kpDown[idx] = false; }
    }
  }
  mcpWrite(MCP_OLATA, 0x0F);                               // 대기: 행 전부 HIGH
}

/* ------------------------------ 시퀀스 엔진 -------------------------------- */
bool motorBusyByOther(uint8_t motor, uint8_t self){
  if (!stepMoving(motor)) return false;
  (void)self;
  return true;
}
void serviceSequences(){
  unsigned long now = millis();
  for (uint8_t s = 0; s < N_SEQ; s++){
    SeqRun &R = seqRun[s];
    if (R.phase == PH_IDLE) continue;
    Seq &S = seqs[s];
    // 한 슬롯이 한 번에 여러 단계(지연 0인 릴레이 등)를 처리하되, 무한 루프는 막음
    for (uint8_t guard = 0; guard < MAX_STEPS + 2; guard++){
      if (R.phase == PH_DELAY){
        if ((long)(now - R.until) < 0) break;
        R.phase = PH_RUN;
      }
      if (R.phase == PH_MOTOR){
        const Step &cur = S.steps[R.idx];
        if (stepMoving(cur.target)) break;
        R.idx++;
        if (cur.delayMs > 0){ R.until = now + cur.delayMs; R.phase = PH_DELAY; continue; }
        R.phase = PH_RUN;
      }
      if (R.phase == PH_RUN){
        if (R.idx >= S.nSteps){ R.phase = PH_IDLE; break; }
        const Step &st = S.steps[R.idx];
        if (st.type == 0){
          setRelay(st.target, st.on != 0);
          R.idx++;
          if (st.delayMs > 0){ R.until = now + st.delayMs; R.phase = PH_DELAY; }
        } else {
          if (!stepWired(st.target) || st.steps == 0){ R.idx++; continue; }   // 못 하는 동작은 건너뜀
          if (motorBusyByOther(st.target, s)) break;                          // 다른 시퀀스가 쓰는 중이면 기다림
          stepStart(st.target, st.steps, st.speed ? st.speed : 400);
          R.phase = PH_MOTOR;
          break;
        }
      } else break;
    }
  }
}
// 초음파·로드셀 트리거(보드 단독 감시)
void serviceValueTriggers(){
  static unsigned long lastUltra = 0, lastLoad = 0;
  unsigned long now = millis();
  bool needUltra = false, needLoad = false;
  for (uint8_t s = 0; s < N_SEQ; s++){
    if (seqs[s].nSteps == 0) continue;
    if (seqs[s].trigSrc == 1) needUltra = true;
    else if (seqs[s].trigSrc == 2) needLoad = true;
  }
  for (uint8_t src = 1; src <= 2; src++){
    if (src == 1 && (!needUltra || now - lastUltra < 120)) continue;
    if (src == 2 && (!needLoad || now - lastLoad < 150)) continue;
    if (src == 1) lastUltra = now; else lastLoad = now;
    long val[5]; bool ok[5];
    for (uint8_t c = 1; c <= 4; c++) ok[c] = false;
    for (uint8_t s = 0; s < N_SEQ; s++){
      Seq &S = seqs[s];
      if (S.nSteps == 0 || S.trigSrc != src) continue;
      uint8_t ch = S.trigCh;
      if (ch < 1 || ch > 4) continue;
      if (!ok[ch]){
        long v;
        bool got = (src == 1) ? ((v = ultraReadCm(ch)) >= 0) : loadReadGram(ch, v, 0);
        if (!got) continue;
        val[ch] = v; ok[ch] = true;
      }
      long v = val[ch];
      int8_t cur = (S.distMode == 1) ? (v >= S.distVal) : (v <= S.distVal);
      int8_t prev = seqRun[s].prev;
      seqRun[s].prev = cur;
      if (prev < 0) continue;                                   // 첫 측정은 기준만 잡음
      bool fire = (S.trigCond == 2) ? (cur != prev) : (cur == (S.trigCond == 1 ? 1 : 0) && prev != cur);
      if (fire && condsMatch(s)) startSequence(s);
    }
  }
}

/* ------------------------------ 시리얼 프로토콜 ---------------------------- */
// 규칙: PIN 을 뺀 모든 명령은 정확히 한 줄로 응답한다(웹이 응답 한 줄을 기다림).
//       응답 말고 보드가 먼저 보내는 줄은 "KEY x" 뿐이다.
#define RX_MAX 200
char rxBuf[RX_MAX];
uint8_t rxLen = 0;
bool rxOver = false;

static void sendOK(){ Serial.println(F("OK")); }
static void sendERR(const __FlashStringHelper *why){ Serial.print(F("ERR ")); Serial.println(why); }

static char *tok(char *&p){
  while (*p == ' ') p++;
  if (!*p) return NULL;
  char *s = p;
  while (*p && *p != ' ') p++;
  if (*p){ *p = 0; p++; }
  return s;
}
static bool toLong(const char *s, long &v){
  if (!s) return false;
  char *e; v = strtol(s, &e, 10);
  return e != s && *e == 0;
}
static bool toULong(const char *s, unsigned long &v){
  if (!s) return false;
  char *e; v = strtoul(s, &e, 10);
  return e != s && *e == 0;
}
static bool isProtectedPin(long pin){
  if (pin < 2 || pin > 69) return true;                                   // 0,1 = USB 시리얼
  for (uint8_t i = 0; i < RELAY_COUNT; i++) if (RELAY_PIN[i] == pin) return true;   // 릴레이 출력
  for (uint8_t i = 0; i < STEP_COUNT; i++) if (pin && (STEP_STEP[i] == pin || STEP_DIR[i] == pin)) return true;
  for (uint8_t i = 0; i < ULTRA_COUNT; i++) if (pin && ULTRA_TRIG[i] == pin) return true;
  for (uint8_t i = 0; i < LOAD_COUNT; i++) if (pin && LOAD_SCK[i] == pin) return true;
  if (pin == 20 || pin == 21) return true;                                // I2C
  return false;
}

void cmdStatus(){
  Serial.print(F("STATUS "));
  for (uint8_t ch = 1; ch <= RELAY_COUNT; ch++){ Serial.print(relayState[ch] ? '1' : '0'); if (ch < RELAY_COUNT) Serial.print(','); }
  Serial.println();
}
void cmdInput(){
  Serial.print(F("INPUT "));
  for (uint8_t ch = 1; ch <= INPUT_COUNT; ch++){ Serial.print(inStateDeb[ch] ? '1' : '0'); if (ch < INPUT_COUNT) Serial.print(','); }
  Serial.println();
}
void cmdGetConfig(){
  Serial.print(F("CONFIG OUT "));
  for (uint8_t ch = 1; ch <= RELAY_COUNT; ch++){ Serial.print(ch); Serial.print(':'); Serial.print(RELAY_PIN[ch-1]); if (ch < RELAY_COUNT) Serial.print(','); }
  Serial.print(F(" IN "));
  for (uint8_t ch = 1; ch <= INPUT_COUNT; ch++){ Serial.print(ch); Serial.print(':'); Serial.print(IN_PIN[ch-1]); if (ch < INPUT_COUNT) Serial.print(','); }
  Serial.println();
}
void cmdGetKpConfig(){
  Serial.print(F("KPCONFIG ROW "));
  for (uint8_t i = 0; i < 4; i++){ Serial.print(VPIN_KP_ROW0 + i); if (i < 3) Serial.print(','); }
  Serial.print(F(" COL "));
  for (uint8_t i = 0; i < 4; i++){ Serial.print(VPIN_KP_COL0 + i); if (i < 3) Serial.print(','); }
  Serial.print(F(" EN ")); Serial.println(kpEnabled ? 1 : 0);
}
void cmdGetStepConfig(){
  Serial.print(F("STEPCONFIG "));
  for (uint8_t c = 1; c <= STEP_COUNT; c++){
    Serial.print(c); Serial.print(':'); Serial.print(STEP_STEP[c-1]); Serial.print(':');
    Serial.print(STEP_DIR[c-1]); Serial.print(':'); Serial.print(STEP_EN_VPIN[c-1]);
    if (c < STEP_COUNT) Serial.print(',');
  }
  Serial.println();
}
void cmdGetUltraConfig(){
  Serial.print(F("ULTRACONFIG "));
  for (uint8_t c = 1; c <= ULTRA_COUNT; c++){
    Serial.print(c); Serial.print(':'); Serial.print(ULTRA_TRIG[c-1]); Serial.print(':'); Serial.print(ULTRA_ECHO[c-1]);
    if (c < ULTRA_COUNT) Serial.print(',');
  }
  Serial.println();
}
void cmdGetLoadConfig(){
  Serial.print(F("LOADCONFIG "));
  for (uint8_t c = 1; c <= LOAD_COUNT; c++){
    Serial.print(c); Serial.print(':'); Serial.print(LOAD_DOUT[c-1]); Serial.print(':'); Serial.print(LOAD_SCK[c-1]);
    if (c < LOAD_COUNT) Serial.print(',');
  }
  Serial.println();
}
void cmdStepStatus(){
  Serial.print(F("STEPSTATUS "));
  for (uint8_t c = 1; c <= STEP_COUNT; c++){
    bool mv; long rem, pos; stepInfo(c, mv, rem, pos);
    Serial.print(c); Serial.print(':'); Serial.print(mv ? 1 : 0); Serial.print(':');
    Serial.print(rem); Serial.print(':'); Serial.print(pos);
    if (c < STEP_COUNT) Serial.print(',');
  }
  Serial.println();
}
void cmdSeqGet(){
  for (uint8_t s = 0; s < N_SEQ; s++){
    const Seq &S = seqs[s];
    if (S.nSteps == 0) continue;
    Serial.print(F("SEQ ")); Serial.print(s); Serial.print(' ');
    Serial.print(S.trigSrc); Serial.print(' '); Serial.print(S.trigCh); Serial.print(' ');
    Serial.print(S.trigCond); Serial.print(' '); Serial.print(S.distMode); Serial.print(' ');
    Serial.print(S.distVal); Serial.print(' '); Serial.print(S.nSteps); Serial.print(' ');
    if (S.name[0]) Serial.print(S.name); else { Serial.print(F("seq")); Serial.print(s + 1); }
    Serial.print(' ');
    for (uint8_t i = 0; i < S.nSteps; i++){
      const Step &st = S.steps[i];
      Serial.print(st.type); Serial.print(','); Serial.print(st.target); Serial.print(',');
      Serial.print(st.on); Serial.print(','); Serial.print((unsigned long)st.delayMs); Serial.print(',');
      Serial.print(st.steps); Serial.print(','); Serial.print(st.speed); Serial.print(';');
    }
    Serial.println();
  }
  for (uint8_t s = 0; s < N_SEQ; s++){
    if (seqs[s].nSteps == 0 || (seqs[s].onMask == 0 && seqs[s].offMask == 0)) continue;
    Serial.print(F("SEQCOND ")); Serial.print(s); Serial.print(' ');
    Serial.print((unsigned long)seqs[s].onMask); Serial.print(' '); Serial.println((unsigned long)seqs[s].offMask);
  }
  Serial.println(F("SEQGETDONE"));
}

void handleLine(char *line){
  // 오른쪽 공백/CR 제거
  int n = strlen(line);
  while (n > 0 && (line[n-1] == ' ' || line[n-1] == '\r' || line[n-1] == '\t')) line[--n] = 0;
  if (n == 0) return;
  char *rest = line;
  char *sp = strchr(line, ' ');
  if (sp){ *sp = 0; rest = sp + 1; } else rest = line + n;
  for (char *c = line; *c; c++) if (*c >= 'a' && *c <= 'z') *c -= 32;
  const char *cmd = line;
  char *p = rest;
  long a, b, c3, d;
  #define IS(x) (strcmp(cmd, x) == 0)

  /* ---- PIN: 응답 없음(웹이 기다리지 않음) ---- */
  if (IS("PIN")){
    char *t1 = tok(p), *t2 = tok(p);
    if (toLong(t1, a) && t2){
      bool on = (t2[0] == 'O' || t2[0] == 'o') && (t2[1] == 'N' || t2[1] == 'n');
      setRelay((uint8_t)a, on);
    }
    return;
  }
  if (IS("STATUS?")){ cmdStatus(); return; }
  if (IS("INPUT?")){ cmdInput(); return; }
  if (IS("GETCONFIG?")){ cmdGetConfig(); return; }
  if (IS("GETKPCONFIG?")){ cmdGetKpConfig(); return; }
  if (IS("GETSTEPCONFIG?")){ cmdGetStepConfig(); return; }
  if (IS("GETULTRACONFIG?")){ cmdGetUltraConfig(); return; }
  if (IS("GETLOADCONFIG?")){ cmdGetLoadConfig(); return; }
  if (IS("STEPSTATUS?")){ cmdStepStatus(); return; }
  if (IS("VER?")){ Serial.print(F("VER ")); Serial.println(F(FW_VERSION)); return; }

  /* ---- 핀 매핑: 이 보드는 핀이 고정배선이라 같은 값만 받아들이고 다르면 ERR ---- */
  if (IS("SETOUT")){
    if (toLong(tok(p), a) && toLong(tok(p), b) && a >= 1 && a <= RELAY_COUNT && RELAY_PIN[a-1] == b) sendOK(); else sendERR(F("FIXED"));
    return;
  }
  if (IS("SETIN")){
    if (toLong(tok(p), a) && toLong(tok(p), b) && a >= 1 && a <= INPUT_COUNT && IN_PIN[a-1] == b) sendOK(); else sendERR(F("FIXED"));
    return;
  }
  if (IS("SETSTEPPIN")){
    if (toLong(tok(p), a) && toLong(tok(p), b) && toLong(tok(p), c3) && toLong(tok(p), d) && a >= 1 && a <= STEP_COUNT
        && STEP_STEP[a-1] == b && STEP_DIR[a-1] == c3 && STEP_EN_VPIN[a-1] == d) sendOK(); else sendERR(F("FIXED"));
    return;
  }
  if (IS("SETULTRAPIN")){
    if (toLong(tok(p), a) && toLong(tok(p), b) && toLong(tok(p), c3) && a >= 1 && a <= ULTRA_COUNT
        && ULTRA_TRIG[a-1] == b && ULTRA_ECHO[a-1] == c3) sendOK(); else sendERR(F("FIXED"));
    return;
  }
  if (IS("SETLOADPIN")){
    if (toLong(tok(p), a) && toLong(tok(p), b) && toLong(tok(p), c3) && a >= 1 && a <= LOAD_COUNT
        && LOAD_DOUT[a-1] == b && LOAD_SCK[a-1] == c3) sendOK(); else sendERR(F("FIXED"));
    return;
  }
  if (IS("SETKPROW") || IS("SETKPCOL")){
    bool isRow = IS("SETKPROW");
    if (toLong(tok(p), a) && toLong(tok(p), b) && a >= 0 && a <= 3 && b == (isRow ? VPIN_KP_ROW0 : VPIN_KP_COL0) + a) sendOK(); else sendERR(F("FIXED"));
    return;
  }

  /* ---- 릴레이 방식 / 시작 상태 ---- */
  if (IS("GETRMODE?")){ Serial.print(F("RMODE ")); Serial.println(relayActiveLow ? 1 : 0); return; }
  if (IS("SETRMODE")){
    if (!toLong(tok(p), a)) { sendERR(F("ARG")); return; }
    relayActiveLow = (a != 0);
    eeSaveRelayMode();
    for (uint8_t ch = 1; ch <= RELAY_COUNT; ch++) writeRelayLevel(ch);   // 논리 상태 유지, 물리 레벨만 맞춤
    sendOK(); return;
  }
  if (IS("GETBOOT?")){
    Serial.print(F("BOOT "));
    for (uint8_t ch = 1; ch <= RELAY_COUNT; ch++){ Serial.print(relayBoot[ch] ? '1' : '0'); if (ch < RELAY_COUNT) Serial.print(','); }
    Serial.println(); return;
  }
  if (IS("SETBOOT")){
    if (!toLong(tok(p), a) || !toLong(tok(p), b) || a < 1 || a > RELAY_COUNT) { sendERR(F("ARG")); return; }
    relayBoot[a] = b ? 1 : 0; eeSaveBoot((int)a);
    sendOK(); return;
  }

  /* ---- 직접 핀 읽기 ---- */
  if (IS("RAWPIN")){
    if (!toLong(tok(p), a) || isProtectedPin(a)) { sendERR(F("PIN")); return; }
    pinMode((uint8_t)a, INPUT_PULLUP);
    delayMicroseconds(50);
    Serial.print(F("RAWPIN ")); Serial.print(a); Serial.print(' '); Serial.println(digitalRead((uint8_t)a) == LOW ? 1 : 0);
    return;
  }
  if (IS("RAWSCAN?")){
    Serial.print(F("RAWSCAN "));
    bool first = true;
    for (uint8_t pin = 16; pin <= 31; pin++){
      if (pin == 20 || pin == 21) continue;                 // I2C
      if (!first) Serial.print(',');
      first = false;
      Serial.print(pin); Serial.print(':'); Serial.print(digitalRead(pin) == LOW ? 1 : 0);   // 1 = LOW(눌림)
    }
    Serial.println(); return;
  }

  /* ---- 키패드 ---- */
  if (IS("SETKPEN")){
    if (!toLong(tok(p), a)) { sendERR(F("ARG")); return; }
    kpEnabled = (a != 0); eeSaveKpEn();
    for (uint8_t i = 0; i < 16; i++){ kpCount[i] = 0; kpDown[i] = false; }
    sendOK(); return;
  }

  /* ---- OLED ---- */
  if (IS("SETLED")){
    copyUtf8(ledText, rest, TEXT_MAX);
    eeSaveText(EE_LEDTXT, ledText);
    ledShowing = false;
    oledDraw(ledText);
    sendOK(); return;
  }
  if (IS("GETLED?")){ Serial.print(F("LEDTEXT ")); Serial.println(ledText); return; }
  if (IS("SETLEDTRIG")){
    if (!toLong(tok(p), a) || !toLong(tok(p), b) || !toLong(tok(p), c3) || a < 0 || a > INPUT_COUNT) { sendERR(F("ARG")); return; }
    ledTrigCh = (uint8_t)a; ledTrigCond = b ? 1 : 0;
    ledTrigSec = (uint16_t)(c3 < 1 ? 1 : (c3 > 3600 ? 3600 : c3));
    while (*p == ' ') p++;
    copyUtf8(ledTrigText, p, TEXT_MAX);
    eeSaveLedTrig();
    sendOK(); return;
  }
  if (IS("GETLEDTRIG?")){
    Serial.print(F("LEDTRIG ")); Serial.print(ledTrigCh); Serial.print(' '); Serial.print(ledTrigCond); Serial.print(' ');
    Serial.print(ledTrigSec); Serial.print(' '); Serial.println(ledTrigText);
    return;
  }

  /* ---- 스텝모터 ---- */
  if (IS("STEPMOVE")){
    if (!toLong(tok(p), a) || !toLong(tok(p), b) || !toLong(tok(p), c3)) { sendERR(F("ARG")); return; }
    if (!stepWired((uint8_t)a)) { sendERR(F("NOPIN")); return; }
    stepStart((uint8_t)a, b, c3);
    sendOK(); return;
  }
  if (IS("STEPSTOP")){ if (toLong(tok(p), a)) stepStop((uint8_t)a); sendOK(); return; }
  if (IS("STEPSTOPALL")){ for (uint8_t ch = 1; ch <= STEP_COUNT; ch++) stepStop(ch); sendOK(); return; }
  if (IS("STEPZERO")){ if (toLong(tok(p), a)) stepZero((uint8_t)a); sendOK(); return; }
  if (IS("STEPEN")){
    if (!toLong(tok(p), a) || !toLong(tok(p), b) || a < 1 || a > STEP_COUNT) { sendERR(F("ARG")); return; }
    if (!STEP_EN_VPIN[a-1]) { sendERR(F("NOEN")); return; }
    stepSetEnable((uint8_t)a, b != 0);
    sendOK(); return;
  }

  /* ---- 초음파 ---- */
  if (IS("ULTRA?")){
    Serial.print(F("ULTRA "));
    for (uint8_t ch = 1; ch <= ULTRA_COUNT; ch++){ Serial.print(ch); Serial.print(':'); Serial.print(ultraReadCm(ch)); if (ch < ULTRA_COUNT) Serial.print(','); }
    Serial.println(); return;
  }
  if (IS("ULTRAMM")){
    if (!toLong(tok(p), a)) { sendERR(F("ARG")); return; }
    Serial.print(F("ULTRAMM ")); Serial.print(a); Serial.print(':'); Serial.println(ultraReadMm((uint8_t)a));
    return;
  }

  /* ---- 로드셀 ---- */
  if (IS("LOAD?")){
    Serial.print(F("LOAD "));
    for (uint8_t ch = 1; ch <= LOAD_COUNT; ch++){
      long g = 0;
      Serial.print(ch); Serial.print(':');
      Serial.print(loadReadGram(ch, g, 150) ? g : -9999L);       // -9999 = 응답 없음(핀 미설정/보정 전)
      if (ch < LOAD_COUNT) Serial.print(',');
    }
    Serial.println(); return;
  }
  if (IS("LOADTARE")){
    long raw;
    if (toLong(tok(p), a) && a >= 1 && a <= LOAD_COUNT && hxReadAvg((uint8_t)a, raw, 3)){
      loadOffset[a] = raw; eeSaveLoadCal((int)a);
      Serial.println(F("LOADTAREOK"));
    } else sendERR(F("LOAD"));
    return;
  }
  if (IS("LOADCAL")){
    long raw;
    char *t1 = tok(p), *t2 = tok(p);
    float known = t2 ? (float)atof(t2) : 0;
    if (toLong(t1, a) && a >= 1 && a <= LOAD_COUNT && known != 0 && hxReadAvg((uint8_t)a, raw, 3)){
      float sc = (float)(raw - loadOffset[a]) / known;
      if (sc == 0) { sendERR(F("LOAD")); return; }
      loadScale[a] = sc; eeSaveLoadCal((int)a);
      Serial.println(F("LOADCALOK"));
    } else sendERR(F("LOAD"));
    return;
  }

  /* ---- 시퀀스 저장(독립 작동) ---- */
  if (IS("SEQCLR")){
    clearAllSeqs();
    markSeqDirty();
    sendOK(); return;
  }
  if (IS("SEQTRIG")){
    // SEQTRIG idx trigSrc trigCh trigCond distMode distCm
    long v[6]; bool okp = true;
    for (uint8_t i = 0; i < 6; i++) okp = okp && toLong(tok(p), v[i]);
    if (!okp || v[0] < 0 || v[0] >= N_SEQ || v[1] < 0 || v[1] > 2 || v[3] < 0 || v[3] > 2) { sendERR(F("ARG")); return; }
    Seq &S = seqs[v[0]];
    S.trigSrc = (uint8_t)v[1]; S.trigCh = (uint8_t)(v[2] < 0 ? 0 : v[2]); S.trigCond = (uint8_t)v[3];
    S.distMode = v[4] ? 1 : 0; S.distVal = v[5];
    S.onMask = 0; S.offMask = 0;                       // 이어서 SEQCOND 가 오면 다시 채워짐
    markSeqDirty();
    sendOK(); return;
  }
  if (IS("SEQCOND")){
    // SEQCOND idx onMask offMask
    unsigned long onM, offM;
    if (!toLong(tok(p), a) || !toULong(tok(p), onM) || !toULong(tok(p), offM) || a < 0 || a >= N_SEQ) { sendERR(F("ARG")); return; }
    seqs[a].onMask = onM; seqs[a].offMask = offM;
    markSeqDirty();
    sendOK(); return;
  }
  if (IS("SEQCOND?")){ Serial.println(F("SEQCOND OK")); return; }
  if (IS("SEQSTEP")){
    // SEQSTEP idx si isMotor target on delay steps speed
    long v[8]; bool okp = true;
    for (uint8_t i = 0; i < 8; i++) okp = okp && toLong(tok(p), v[i]);
    if (!okp || v[0] < 0 || v[0] >= N_SEQ || v[1] < 0 || v[1] >= MAX_STEPS) { sendERR(F("ARG")); return; }
    bool motor = v[2] != 0;
    if (motor ? (v[3] < 1 || v[3] > STEP_COUNT) : (v[3] < 1 || v[3] > RELAY_COUNT)) { sendERR(F("RANGE")); return; }
    Step &st = seqs[v[0]].steps[v[1]];
    st.type = motor ? 1 : 0; st.target = (uint8_t)v[3]; st.on = v[4] ? 1 : 0;
    st.delayMs = (uint32_t)(v[5] < 0 ? 0 : v[5]);
    st.steps = motor ? (int32_t)v[6] : 0;
    st.speed = motor ? (uint16_t)(v[7] < 0 ? 0 : (v[7] > 65535 ? 65535 : v[7])) : 0;
    markSeqDirty();                                    // nSteps 는 SEQCOUNT 에서 확정(업로드 도중 반쯤 만든 시퀀스가 실행되지 않게)
    sendOK(); return;
  }
  if (IS("SEQNAME")){
    char *t1 = tok(p), *t2 = tok(p);
    if (!toLong(t1, a) || a < 0 || a >= N_SEQ) { sendERR(F("ARG")); return; }
    memset(seqs[a].name, 0, sizeof(seqs[a].name));
    if (t2) copyUtf8(seqs[a].name, t2, SEQ_NAME_MAX);
    markSeqDirty();
    sendOK(); return;
  }
  if (IS("SEQCOUNT")){
    if (!toLong(tok(p), a) || !toLong(tok(p), b) || a < 0 || a >= N_SEQ) { sendERR(F("ARG")); return; }
    seqs[a].nSteps = (uint8_t)(b < 0 ? 0 : (b > MAX_STEPS ? MAX_STEPS : b));
    abortSequence((uint8_t)a);
    seqRun[a].prev = -1;
    markSeqDirty();
    sendOK(); return;
  }
  if (IS("SEQGET?")){ cmdSeqGet(); return; }
  if (IS("EXTMEM?")){
    // 내장 EEPROM 사용. 기록이 끝날 때까지 잠깐 기다린 뒤 결과를 알려준다.
    bool done = flushSeqPersist(1500);
    Serial.print(F("EXTMEM CONNECTED "));
    Serial.print(bootWarn ? F("BOOTWARN ") : F("NOWARN "));
    Serial.println(done ? F("WRITEOK") : F("WRITEFAIL"));
    return;
  }

  sendERR(F("UNKNOWN"));
  #undef IS
}

void pollSerial(){
  while (Serial.available()){
    char c = (char)Serial.read();
    if (c == '\n'){
      if (rxOver) sendERR(F("LONG"));
      else { rxBuf[rxLen] = 0; handleLine(rxBuf); }
      rxLen = 0; rxOver = false;
    } else if (c != '\r'){
      if (rxLen < RX_MAX - 1) rxBuf[rxLen++] = c; else rxOver = true;
    }
  }
}

/* ---------------------------------- setup/loop ---------------------------- */
void setup(){
  Serial.begin(115200);
  Wire.begin();
  Wire.setClock(400000);
#if defined(WIRE_HAS_TIMEOUT)
  Wire.setWireTimeout(5000, true);       // I2C가 멈춰도 프로그램이 멈추지 않게
#endif

  eepromLoadAll();

  // 릴레이: 레벨을 먼저 정한 뒤 출력으로 바꿔서, 부팅 때 깜박이지 않게 한다
  for (uint8_t ch = 1; ch <= RELAY_COUNT; ch++){
    relayState[ch] = relayBoot[ch] != 0;
    writeRelayLevel(ch);
    pinMode(RELAY_PIN[ch-1], OUTPUT);
  }
  for (uint8_t ch = 1; ch <= INPUT_COUNT; ch++) pinMode(IN_PIN[ch-1], INPUT_PULLUP);
  delay(30);
  for (uint8_t ch = 1; ch <= INPUT_COUNT; ch++){       // 부팅 순간의 입력 상태를 기준으로(가짜 엣지 방지)
    bool a = readInputActive(ch);
    inStateRaw[ch] = inStateDeb[ch] = a; inLastChange[ch] = millis();
  }
  for (uint8_t i = 0; i < ULTRA_COUNT; i++) if (ULTRA_TRIG[i]){ digitalWrite(ULTRA_TRIG[i], LOW); pinMode(ULTRA_TRIG[i], OUTPUT); pinMode(ULTRA_ECHO[i], INPUT); }
  for (uint8_t i = 0; i < LOAD_COUNT; i++) if (LOAD_SCK[i]){ digitalWrite(LOAD_SCK[i], LOW); pinMode(LOAD_SCK[i], OUTPUT); pinMode(LOAD_DOUT[i], INPUT); }
  for (uint8_t i = 0; i < STEP_COUNT; i++) if (STEP_STEP[i]){
    digitalWrite(STEP_STEP[i], LOW); digitalWrite(STEP_DIR[i], LOW);
    pinMode(STEP_STEP[i], OUTPUT); pinMode(STEP_DIR[i], OUTPUT);
  }

  mcpInit();

  Wire.beginTransmission(0x3C);
  oledPresent = (Wire.endTransmission() == 0);
  if (oledPresent){
    u8g2.begin();
    u8g2.setBusClock(400000);
    u8g2.setFont(OLED_FONT);
    u8g2.setFontPosTop();
    oledDraw(ledText);                     // 마지막으로 저장한 문구 표시
  }
}

void loop(){
  pollSerial();
  scanInputs();
  scanKeypad();
  serviceSequences();
  serviceValueTriggers();
  serviceLed();
  serviceEepromWriter(false);
}
