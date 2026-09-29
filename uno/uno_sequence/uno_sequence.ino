// ============================================================
//  UNO PLC 보드 펌웨어  v1.9.1  (v1.6에 "입력 반전" 추가. v1.6 = 시퀀스 6개→12개, v1.5 = 입력 12채널)
//    v1.9.1 변경: 릴레이(출력)별로 "출력 반전(A접점/B접점)" 설정 추가 — 릴레이 모듈마다 ON/OFF 신호 극성이
//               반대인 경우가 있어서(예: active-LOW 모듈 vs active-HIGH 모듈), "■ 테스트 초기화"로
//               강제 OFF를 보냈을 때 보드에 따라 실제로는 켜지는 것처럼 보이는 문제가 있었습니다.
//               채널별로 반전을 켜고 끌 수 있게 하여, 보드(릴레이 모듈)가 달라도 웹페이지에서 맞춰 쓸 수 있습니다.
//               기본값은 전부 반전 없음(기존 방식, active-LOW)이라 예전 보드는 그대로 동작합니다. 설정은 EEPROM 593번대에 저장됩니다.
//    v1.9 변경: 동작 순서 각 줄을 "지연(ms)" 대신 "조건 대기"(지정 채널이 원하는 상태가 될 때까지 시간제한 없이 대기)로도
//               쓸 수 있게 SEQSTEP에 선택적 인자(대기채널, 대기상태) 추가. 내부적으로 sd[]의 최상위 비트를 "대기 여부"
//               플래그로 쓰고(그래서 지연 값은 0~32767ms로 줄었습니다), 남은 하위 비트에 채널·상태를 담습니다.
//               ("순서대로 실행" SETSEQORDER/GETSEQORDER? 명령은 아직 미구현 — 웹페이지가 이를 감지해 경고만 남기고
//               나머지 기능은 정상 동작합니다.)
//    v1.8 변경: 전원 투입 시 릴레이 시작 상태(SETBOOT/GETBOOT) 추가 — 기본은 OFF 시작, 체크한 릴레이만 ON으로 시작.
//               설정은 EEPROM 490번대에 저장됩니다.
//    v1.7 변경: 채널별 "입력 반전" 설정 추가 — 반전하면 핀이 HIGH일 때 "눌림"으로 봅니다(기본은 LOW=눌림).
//               A0~A3 처럼 아무것도 안 달아도 LOW로 읽히는 핀에 5V 접점을 달아 쓸 때 사용. 설정은 EEPROM 590번대에 저장됩니다.
//    v1.6 변경: 시퀀스 최대 6개 → 12개.  시퀀스 EEPROM 저장 위치를 600번 → 20번으로 옮김(12개가 EE_LCD 자리와 겹치지 않도록).
//               v1.5 이하 보드에 이미 저장돼 있던 시퀀스는 처음 부팅할 때 자동으로 새 자리로 옮겨집니다(다시 저장할 필요 없음).
//  릴레이 4채널(R1~R4) / 입력 12채널(CH1~CH12)
//
//  동작 모드
//    2 = 시퀀스       : 시작 조건(트리거) + 추가 조건(인터록) → 릴레이 동작 순서(지연 포함). 웹페이지 "시퀀스 만들기"에서 저장 (기본값)
//
//  웹페이지(Mega2560_16Relay_control.html)와 같은 ASCII 명령을 사용합니다. (115200bps, 줄 끝 \r\n)
//
//  ── 기본 배선 (웹페이지 "3. 출력 핀", "4. 입력 버튼"에서 바꿀 수 있고 EEPROM에 저장됩니다) ──
//    릴레이  R1~R4   = 핀 3,4,5,6                          (active-LOW: LOW = ON)
//    입력    CH1~CH6  = 핀 7~12                             (INPUT_PULLUP: 버튼/센서가 GND로 붙으면 "눌림")
//    입력    CH7~CH12 = 핀 2, 13, A0, A1, A2, A3(=14~17)    (마찬가지로 INPUT_PULLUP)
//    ※ UNO는 핀이 제한적이라 12채널이 사실상 최대치입니다. A4·A5(=18,19)는 OLED·외부 EEPROM용
//      I2C 버스 전용이라 입력으로 못 씁니다. D13은 보드 내장 LED와 같이 물려 있어 동작은 하지만
//      풀업 임계값이 살짝 낮아질 수 있으니, 가능하면 CH1~CH11까지 먼저 채우는 걸 권장합니다.
//
//  ── 명령 ──
//    ID?            → ID UNO_PLC 1.0
//    GETMODE? / SETMODE 2         (모드는 2 하나뿐)
//    PIN <ch> ON|OFF               (응답 없음)
//    STATUS? / INPUT?              → STATUS 0,1,0,0 / INPUT 0,0,1,0,0,0
//    GETCONFIG? / SETOUT <ch> <pin> / SETIN <ch> <pin>
//    GETINV? / SETINV <ch> <0|1>   (입력 반전: 1 = 핀이 HIGH일 때 눌림. GETINV? → INV 0,0,1,...  12개)
//    GETOUTINV? / SETOUTINV <ch> <0|1>   (출력 반전(A접점/B접점): 0=기존 방식(active-LOW), 1=반전(active-HIGH).
//                                  GETOUTINV? → OUTINV 0,0,0,0  4개. 릴레이 모듈이 반대로 동작하는 보드에서 채널별로 맞춰 씀)
//    GETRMODE? / SETRMODE 1        (전체 기본값 조회용 — 실제 반전은 이제 GETOUTINV?/SETOUTINV로 채널별 설정)
//  ── 시퀀스 명령 (모드 2) ──
//    SEQCLR                        시퀀스 편집 시작 (편집 중에는 시퀀스 동작 일시 정지)
//    SEQDEF <i> <입력ch> <cond> <enabled>   i = 0~11, 입력ch 0 = 자동 트리거 없음(수동 실행만),
//                                  cond 1=눌림 0=뗌 2=양쪽
//    SEQCOND <i> <입력ch> <state>            추가 조건(인터록): 트리거 순간 이 채널이 state(1=눌림 0=뗌)여야 실행. 최대 4개
//    SEQSTEP <i> <릴레이ch> <state> <지연ms> [<대기ch> <대기상태>]
//                                  동작 1줄: 릴레이 ON(1)/OFF(0) → 지연ms(0~32767) 기다린 뒤 다음 줄. 최대 10줄
//                                  마지막 두 인자(대기ch 1~N_IN, 대기상태 1=눌릴 때까지/0=뗄 때까지)를 주면
//                                  지연 대신 그 채널이 "새로 그 상태가 될 때까지" 시간제한 없이 기다립니다(v1.9).
//    SEQSAVE / SEQABORT            EEPROM에 저장 후 적용 / 편집 취소
//    SEQGET? / SEQSTAT? / SEQRUN <i> / SEQSTOP
//    SETBOOT <릴레이ch> <0|1> / GETBOOT?   전원 투입 시 시작 상태(1=ON으로 시작). GETBOOT? → BOOT 0,1,0,0 (v1.8)
//
//  ── 0.96" OLED (I2C, 128x64) 로 한글 문구 표시 ──
//    배선: OLED VCC→5V, GND→GND, SCL→A5, SDA→A4  (외부 EEPROM 24LC256도 같은 A4/A5 버스에 함께 연결, 주소 0x50)
//    ※ A4·A5는 OLED·외부 EEPROM 전용(I2C)으로 고정 사용합니다.
//    한글은 보드에 폰트가 없어서, 웹페이지에서 글씨를 이미지(비트맵)로 만들어 보드로 전송해
//    외부 EEPROM에 저장해두고, 신호가 오면 그 이미지를 그대로 OLED에 띄우는 방식입니다.
//    LCDCLR                              문구 이벤트 편집 시작
//    LCDDEF <i> <종류> <ch> <state> <슬롯>   i=0~7, 종류 0=시작시(ch·state 0) 1=입력신호(ch=1~6,state 1=눌림 0=뗌)
//                                         2=릴레이신호(ch=1~4,state 1=ON 0=OFF), 슬롯=0~31(외부 EEPROM 이미지 번호)
//    LCDSAVE / LCDABORT / LCDGET?
//    LCDIMG <슬롯> <옵셋> <HEX바이트...>     이미지 데이터 저장(옵셋 0~1023, 한 번에 최대 64바이트)
//    LCDSHOW <슬롯>                       그 슬롯 이미지를 지금 바로 화면에 띄움(확인용)
// ============================================================
#include <EEPROM.h>
#include <Wire.h>

#define FW_VERSION "1.9.1"

// [추가] OLED(SSD1306)·외장EEPROM(24LC256)은 둘 다 I2C(Wire)로 연결하는 부품인데,
// 보드마다 실제로 배선했는지가 다릅니다. 컴파일 시 껐다 켰다(재업로드 필요) 하는 대신,
// 웹페이지에서 "OLED 사용함/사용 안 함"으로 바로 켜고 끌 수 있도록 EEPROM에 저장되는
// 런타임 설정(oledEnabled)으로 관리합니다. I2C 장치가 없는 상태에서 괜히 통신을 시도하면
// setup()이 멈춰버릴 수 있어서, 기본값은 "사용 안 함"입니다.
bool oledEnabled = false;

// ---------- 하드웨어 ----------
#define N_OUT 4
#define N_IN  12
// [v1.9.1] 예전에는 전체 보드가 고정으로 active-LOW(LOW=ON)였는데, 릴레이 모듈에 따라 반대(active-HIGH)인
// 경우가 있어서 채널별로 반전을 켜고 끌 수 있게 했습니다. outInvMask의 비트 i가 0(기본)이면 릴레이(i+1)는
// 예전과 똑같이 active-LOW로 동작(기존 보드는 그대로 호환), 1이면 반전(active-HIGH)으로 동작합니다.

uint8_t outPin[N_OUT] = {3, 4, 5, 6};
uint8_t inPin[N_IN]   = {7, 8, 9, 10, 11, 12, 2, 13, 14, 15, 16, 17};   // CH1~CH12 (14~17 = A0~A3)
bool    relayOn[N_OUT];
uint8_t outInvMask = 0;        // 비트 i = 1 이면 릴레이(i+1)는 반전(active-HIGH: HIGH일 때 ON) — 채널별 출력 반전(A접점/B접점)
uint16_t inInvMask = 0;       // 비트 i = 1 이면 CH(i+1)은 "핀이 HIGH일 때 눌림"(입력 반전)
uint8_t  outBootMask = 0;     // 비트 i = 1 이면 릴레이(i+1)은 전원 투입 시 ON으로 시작(기본 0 = OFF로 시작) (v1.8)

// ---------- 동작 모드 ----------
#define MODE_SEQ     2
uint8_t runMode = MODE_SEQ;   // 시퀀스 모드 하나만 남음

// ---------- 타이밍 ----------
#define BOOT_HOLD_MS        3000UL   // 전원 투입 후 3초간 입력 무시(원래 스케치의 delay(3000)과 같은 역할)
#define DEBOUNCE_MS         30UL     // 입력이 이 시간 동안 안정돼야 "신호"로 인정
#define CNT_EDIT_TIMEOUT_MS 10000UL  // 편집을 시작해놓고 이 시간 동안 명령이 없으면 자동 취소

// ---------- EEPROM 배치 ----------
#define EE_MAGIC   0    // 0xC5
#define EE_VER     1
#define EE_MODE    2
#define EE_OUTPIN  3    // 4바이트
#define EE_INPIN   7    // 12바이트 (구 펌웨어에서는 6바이트였음 — v1.5부터 12바이트로 확장, 주소 19부터는 그대로 이동)
#define EE_OLED    19   // 1바이트: OLED(I2C) 사용함(1)/사용 안 함(0)
// (20~585 : 예전 횟수 시퀀스 설정 자리였고, v1.6부터 아래 EE_SEQ(시퀀스 12개)가 20번부터 사용)
// (850~875 : 예전 키패드 설정 자리 — 이제 사용하지 않음. 다른 항목 주소는 그대로 유지)
#define EE_SEQ     20   // [nSeqs][체크섬][시퀀스 데이터...]  (12*39+2 = 470 바이트, 20~489)   ← v1.5 이하는 600번(6개)
#define EE_BOOT    490  // [마스크][체크] 2바이트 — 전원 투입 시 릴레이 시작 상태 (v1.8)
#define EE_INV     590  // [마스크 하위][마스크 상위][체크] 3바이트 — 입력 반전 설정 (v1.7)
#define EE_OUTINV  593  // [마스크][체크] 2바이트 — 출력(릴레이) 반전 설정(v1.9.1). 비트 i=1이면 릴레이(i+1)는 반전(active-HIGH)
#define EE_SEQ_OLD 600  // v1.5 이하가 시퀀스(최대 6개, 236바이트)를 저장하던 자리 — 부팅 때 내용이 있으면 EE_SEQ로 옮기고 이 자리는 지움
#define MAX_SEQ_OLD 6
#define EE_LCD     880  // [magic][n][이벤트8개*4바이트] = 34바이트 (913까지). 이미지 자체는 외부 EEPROM(24LC256)에 저장
#define EE_MAGIC_VALUE 0xC5
#define EE_VER_VALUE   2   // v1.5에서 입력 6→12채널로 EEPROM 배치가 바뀌어 1→2로 올림(구 보드는 자동으로 새 기본값으로 재설정됨)

// ---------- 시퀀스 (모드 2) ----------
#define MAX_SEQ   12
#define MAX_CONDS 4
#define MAX_STEPS 10
struct SeqDef {
  uint8_t  trigCh;              // 0 = 자동 트리거 없음, 1~N_IN
  uint8_t  trigCond;            // 1 = 눌림, 0 = 뗌, 2 = 양쪽
  uint8_t  enabled;
  uint8_t  nConds;
  uint8_t  nSteps;
  uint8_t  cc[MAX_CONDS];       // (입력ch << 1) | state(1=눌림 0=뗌)
  uint8_t  sr[MAX_STEPS];       // (릴레이ch << 1) | state
  uint16_t sd[MAX_STEPS];       // 최상위 비트(0x8000)=1이면 "조건 대기" 줄: 하위 4비트=대기채널-1, 5번째 비트=대기상태(1=눌림)
                                 // 최상위 비트=0이면 일반 "지연" 줄: 나머지 15비트(0~32767ms)가 다음 줄까지 기다리는 시간 (v1.9)
};
SeqDef  seqs[MAX_SEQ];
uint8_t nSeqs = 0;
bool    sqRun[MAX_SEQ];
uint8_t sqStep[MAX_SEQ];
unsigned long sqDue[MAX_SEQ];
bool    sqPrev[MAX_SEQ];
bool    sqCondWait[MAX_SEQ];  // true면 지금 이 줄이 "조건 대기" 중(릴레이 동작은 이미 실행됨) (v1.9)
bool    sqWaitBase[MAX_SEQ];  // 그 대기를 "시작한 시점"의 채널 상태 — 이미 그 상태였어도 곧장 통과시키지 않고, 실제로 신호가 바뀌어야 다음 줄로 진행
bool    seqEditing = false;
unsigned long seqEditLastMs = 0;

// ---------- 입력 디바운스 ----------
bool inLast[N_IN], inStable[N_IN];
unsigned long inChangedAt[N_IN];

// ---------- 기타 상태 ----------
bool bootHold = true;

// ---------- 시리얼 수신 ----------
char lineBuf[160];         // LCDIMG 는 최대 64바이트(=128 hex 글자)를 한 줄로 받으므로 넉넉히 잡음
uint8_t lineLen = 0;
bool lineOverflow = false;

// ============================================================
//  유틸
// ============================================================
static bool validPin(long p) { return p >= 2 && p <= 19; }   // D2~D13, A0~A5(14~19). 0,1은 USB 시리얼용

static void initRelayPin(uint8_t ch) {                // ch = 1~N_OUT (핀 번호가 아니라 채널로 받아서 그 채널의 반전 설정을 적용)
  bool inv = ((outInvMask >> (ch - 1)) & 1) != 0;      // 1 = 이 릴레이는 반전(active-HIGH)
  digitalWrite(outPin[ch - 1], inv ? LOW : HIGH);       // 먼저 OFF 레벨을 깔아두고(반전 없음=HIGH가 OFF, 반전=LOW가 OFF)
  pinMode(outPin[ch - 1], OUTPUT);                     // 그 다음 출력으로 바꿔서 켜졌다 꺼지는 순간 깜빡임을 막음
}

static void setRelay(uint8_t ch, bool on) {           // ch = 1~N_OUT
  if (ch < 1 || ch > N_OUT) return;
  relayOn[ch - 1] = on;
  bool inv = ((outInvMask >> (ch - 1)) & 1) != 0;      // 1 = 반전(active-HIGH: 이 채널은 HIGH일 때 ON)
  bool activeLow = !inv;                                // 반전 안 함(기본) = active-LOW(기존 릴레이 보드 방식)
  digitalWrite(outPin[ch - 1], (on == activeLow) ? LOW : HIGH);
}

static void allRelaysOff() {
  for (uint8_t ch = 1; ch <= N_OUT; ch++) setRelay(ch, false);
}

static inline bool pinPressed(uint8_t i) {           // i = 0~N_IN-1, true = 눌림. 기본은 LOW=눌림, 입력 반전이면 HIGH=눌림
  bool low = (digitalRead(inPin[i]) == LOW);
  return (((inInvMask >> i) & 1) != 0) ? !low : low;
}

static inline bool inRaw(uint8_t ch) {                // ch = 1~N_IN, true = 눌림
  return pinPressed(ch - 1);
}

static bool holdActive() {
  if (bootHold && millis() >= BOOT_HOLD_MS) bootHold = false;
  return bootHold;
}

static void softReboot() {
  Serial.flush();
  delay(30);
#if defined(__AVR__)
  cli();
  asm volatile("jmp 0");
#else
  hostReboot();   // 테스트 환경 전용
#endif
}

// Wire 라이브러리에 setWireTimeout 이 있는 버전(아두이노 IDE/코어 1.8.3 이상)에서만 호출하고, 없으면 아무것도 안 함
// [수정] 예전에는 SFINAE 템플릿(오버로드 두 개)으로 "있으면 호출, 없으면 무시"를 구현했는데,
// 아두이노 IDE가 컴파일 전에 함수 프로토타입을 자동 생성하는 과정(ctags)이 이 템플릿 문법을
// 제대로 못 읽고 엉뚱한 줄을 끼워 넣어 "'w' was not declared", "storage class..." 같은
// 전혀 관계없어 보이는 오류를 냈습니다. 템플릿 대신 버전 매크로로 바꿔 이 문제를 없앴습니다.
#if defined(ARDUINO) && (ARDUINO >= 10803)
static inline void wireTimeout(TwoWire &w) { w.setWireTimeout(25000UL, true); }
#else
static inline void wireTimeout(TwoWire &) {}
#endif

static void replyOK() { Serial.println(F("OK")); }
static void replyErr(const __FlashStringHelper *why) { Serial.print(F("ERR ")); Serial.println(why); }

// ============================================================
//  EEPROM
// ============================================================
static void loadInv() {                               // 입력 반전 설정 읽기: 체크가 맞지 않으면(처음 쓰는 보드 등) 반전 없음
  uint8_t lo = EEPROM.read(EE_INV), hi = EEPROM.read(EE_INV + 1), chk = EEPROM.read(EE_INV + 2);
  uint16_t m = (uint16_t)lo | ((uint16_t)hi << 8);
  if (chk == (uint8_t)(lo ^ hi ^ 0x5A) && (m >> N_IN) == 0) inInvMask = m; else inInvMask = 0;
}
static void saveInv() {
  uint8_t lo = (uint8_t)(inInvMask & 0xFF), hi = (uint8_t)(inInvMask >> 8);
  EEPROM.update(EE_INV, lo); EEPROM.update(EE_INV + 1, hi); EEPROM.update(EE_INV + 2, (uint8_t)(lo ^ hi ^ 0x5A));
}
static void loadBootMask() {                          // 체크가 안 맞으면(처음 쓰는 보드 등) 전부 OFF로 시작
  uint8_t m = EEPROM.read(EE_BOOT), chk = EEPROM.read(EE_BOOT + 1);
  outBootMask = (chk == (uint8_t)(m ^ 0x3C)) ? m : 0;
}
static void saveBootMask() {
  EEPROM.update(EE_BOOT, outBootMask);
  EEPROM.update(EE_BOOT + 1, (uint8_t)(outBootMask ^ 0x3C));
}
static void loadOutInv() {                            // 출력(릴레이) 반전 설정 읽기: 체크가 안 맞으면(처음 쓰는 보드 등) 전부 반전 없음(기존 active-LOW)
  uint8_t m = EEPROM.read(EE_OUTINV), chk = EEPROM.read(EE_OUTINV + 1);
  if (chk == (uint8_t)(m ^ 0xA5) && (m >> N_OUT) == 0) outInvMask = m; else outInvMask = 0;
}
static void saveOutInv() {
  EEPROM.update(EE_OUTINV, outInvMask);
  EEPROM.update(EE_OUTINV + 1, (uint8_t)(outInvMask ^ 0xA5));
}

static void saveMaps() {
  for (uint8_t i = 0; i < N_OUT; i++) EEPROM.update(EE_OUTPIN + i, outPin[i]);
  for (uint8_t i = 0; i < N_IN;  i++) EEPROM.update(EE_INPIN + i, inPin[i]);
}

static void loadConfig() {
  if (EEPROM.read(EE_MAGIC) == EE_MAGIC_VALUE && EEPROM.read(EE_VER) == EE_VER_VALUE) {
    uint8_t o[N_OUT], in[N_IN];
    bool ok = true;
    for (uint8_t i = 0; i < N_OUT; i++) o[i]  = EEPROM.read(EE_OUTPIN + i);
    for (uint8_t i = 0; i < N_IN;  i++) in[i] = EEPROM.read(EE_INPIN + i);
    for (uint8_t i = 0; i < N_OUT; i++) if (!validPin(o[i]))  ok = false;
    for (uint8_t i = 0; i < N_IN;  i++) if (!validPin(in[i])) ok = false;
    for (uint8_t i = 0; ok && i < N_OUT + N_IN; i++) {           // 핀 중복 검사
      uint8_t a = (i < N_OUT) ? o[i] : in[i - N_OUT];
      for (uint8_t j = i + 1; j < N_OUT + N_IN; j++) {
        uint8_t b = (j < N_OUT) ? o[j] : in[j - N_OUT];
        if (a == b) ok = false;
      }
    }
    if (ok) {
      memcpy(outPin, o, N_OUT);
      memcpy(inPin, in, N_IN);
    }
    uint8_t oe = EEPROM.read(EE_OLED);
    oledEnabled = (oe == 1);   // 0/1이 아닌 값(예전 펌웨어라 한 번도 안 써진 경우)이면 안전하게 "사용 안 함"
  } else {                                                        // 처음 쓰는 보드: 기본값으로 초기화
    EEPROM.update(EE_MAGIC, EE_MAGIC_VALUE);
    EEPROM.update(EE_VER, EE_VER_VALUE);
    EEPROM.update(EE_MODE, MODE_SEQ);
    runMode = MODE_SEQ;
    saveMaps();
    oledEnabled = false;
    EEPROM.update(EE_OLED, 0);
  }
}

// ============================================================
//  입력 디바운스
// ============================================================
static void updateInputs() {
  unsigned long now = millis();
  for (uint8_t i = 0; i < N_IN; i++) {
    bool raw = pinPressed(i);
    if (raw != inLast[i]) { inLast[i] = raw; inChangedAt[i] = now; }
    else if (raw != inStable[i] && (now - inChangedAt[i]) >= DEBOUNCE_MS) inStable[i] = raw;
  }
}

// ============================================================
//  시퀀스 엔진 (모드 2)
//    시작 조건(트리거)이 일어나는 "그 순간"에, 추가 조건(인터록) 채널이 모두 지정한 상태일 때만
//    동작 순서를 처음부터 실행한다. 각 줄은 "릴레이 동작 → 지연ms 대기 → 다음 줄" 순서이며,
//    실행 중에도 다른 시퀀스와 입력 감시는 멈추지 않는다(delay() 를 쓰지 않음).
//    이미 실행 중인 시퀀스는 트리거가 또 와도 무시한다(끝난 뒤부터 다시 시작 가능).
// ============================================================
static uint8_t calcChkBytes(const uint8_t *b, uint16_t len, uint8_t n) {
  uint8_t chk = 0xA5 ^ n;
  for (uint16_t i = 0; i < len; i++) chk = (uint8_t)((chk << 1) | (chk >> 7)) ^ b[i];
  return chk;
}

static bool seqsValid(uint8_t n) {
  if (n > MAX_SEQ) return false;
  for (uint8_t i = 0; i < n; i++) {
    const SeqDef &S = seqs[i];
    if (S.trigCh > N_IN || S.trigCond > 2 || S.enabled > 1) return false;
    if (S.nConds > MAX_CONDS || S.nSteps > MAX_STEPS) return false;
    for (uint8_t c = 0; c < S.nConds; c++) {
      uint8_t ch = S.cc[c] >> 1;
      if (ch < 1 || ch > N_IN) return false;
    }
    for (uint8_t r = 0; r < S.nSteps; r++) {
      uint8_t rl = S.sr[r] >> 1;
      if (rl < 1 || rl > N_OUT) return false;
      if (S.sd[r] & 0x8000) {                                 // "조건 대기" 줄: 대기 채널이 실제 입력 범위 안에 있는지만 확인
        uint8_t wch = (uint8_t)((S.sd[r] & 0x0F) + 1);
        if (wch < 1 || wch > N_IN) return false;
      }
      // 최상위 비트가 0이면 나머지 15비트가 그대로 지연ms(0~32767)이므로 범위를 벗어날 수 없음
    }
  }
  return true;
}

static void resetSeqRuntime() {
  for (uint8_t i = 0; i < MAX_SEQ; i++) { sqRun[i] = false; sqStep[i] = 0; sqCondWait[i] = false; sqWaitBase[i] = false; }
}

static void resyncSeqPrev() {                         // 가짜 에지 방지
  for (uint8_t i = 0; i < nSeqs; i++)
    sqPrev[i] = (seqs[i].trigCh >= 1) ? inStable[seqs[i].trigCh - 1] : false;
}

static bool writeSeqs() {
  uint16_t len = (uint16_t)nSeqs * sizeof(SeqDef);
  const uint8_t *src = (const uint8_t *)seqs;
  uint8_t chk = calcChkBytes(src, len, nSeqs);
  EEPROM.update(EE_SEQ, nSeqs);
  EEPROM.update(EE_SEQ + 1, chk);
  for (uint16_t i = 0; i < len; i++) EEPROM.update(EE_SEQ + 2 + i, src[i]);
  if (EEPROM.read(EE_SEQ) != nSeqs || EEPROM.read(EE_SEQ + 1) != chk) return false;
  for (uint16_t i = 0; i < len; i++) if (EEPROM.read(EE_SEQ + 2 + i) != src[i]) return false;
  return true;
}

static bool tryLoadSeqsAt(uint16_t addr, uint8_t maxN) {   // addr 자리의 시퀀스 묶음이 온전하면 seqs/nSeqs에 읽어 오고 true
  uint8_t n = EEPROM.read(addr);
  uint8_t chk = EEPROM.read(addr + 1);
  if (n > maxN) return false;
  uint8_t *dst = (uint8_t *)seqs;
  uint16_t len = (uint16_t)n * sizeof(SeqDef);
  for (uint16_t i = 0; i < len; i++) dst[i] = EEPROM.read(addr + 2 + i);
  if (calcChkBytes(dst, len, n) == chk && seqsValid(n)) { nSeqs = n; return true; }
  return false;
}

static void loadSeqs() {
  memset(seqs, 0, sizeof(seqs));
  nSeqs = 0;
  if (!tryLoadSeqsAt(EE_SEQ, MAX_SEQ)) {
    memset(seqs, 0, sizeof(seqs));                    // 처음 쓰는 보드이거나 손상 → 빈 상태
    nSeqs = 0;
    if (tryLoadSeqsAt(EE_SEQ_OLD, MAX_SEQ_OLD)) {     // v1.5 이하로 저장해 둔 시퀀스가 있으면 새 자리로 옮김
      if (writeSeqs()) EEPROM.update(EE_SEQ_OLD, 0xFF);   // 옮기기에 성공했을 때만 옛 자리를 지움(다음 부팅부터 다시 옮기지 않도록)
    } else {
      memset(seqs, 0, sizeof(seqs));
      nSeqs = 0;
    }
  }
  resetSeqRuntime();
  resyncSeqPrev();
}

static void seqStart(uint8_t i) {
  if (i >= nSeqs || sqRun[i] || seqs[i].nSteps == 0) return;
  sqRun[i] = true; sqStep[i] = 0; sqDue[i] = millis(); sqCondWait[i] = false;
}

static bool seqInterlockOK(const SeqDef &S) {
  for (uint8_t c = 0; c < S.nConds; c++) {
    bool want = (S.cc[c] & 1) != 0;
    if (inStable[(S.cc[c] >> 1) - 1] != want) return false;
  }
  return true;
}

static void seqEngine() {
  updateInputs();
  if (seqEditing) return;
  bool holding = holdActive();
  unsigned long now = millis();
  for (uint8_t i = 0; i < nSeqs; i++) {
    SeqDef &S = seqs[i];
    if (S.trigCh >= 1) {
      bool cur = inStable[S.trigCh - 1];
      if (holding) { sqPrev[i] = cur; }
      else if (cur != sqPrev[i]) {
        sqPrev[i] = cur;
        bool fire = cur ? (S.trigCond == 1 || S.trigCond == 2) : (S.trigCond == 0 || S.trigCond == 2);
        if (fire && S.enabled && seqInterlockOK(S)) seqStart(i);
      }
    }
    while (sqRun[i]) {   // 지연이 0이면 같은 순간에 여러 줄이 연달아 실행됨. "조건 대기" 줄은 신호가 바뀔 때까지 여러 loop()에 걸쳐 여기서 멈춤
      uint8_t r = sqStep[i];
      if (sqCondWait[i]) {                                          // 이 줄의 릴레이 동작은 이미 실행됨 — 조건만 확인
        uint8_t wch = (uint8_t)((S.sd[r] & 0x0F) + 1);
        bool wantOn = (S.sd[r] & 0x10) != 0;
        bool curW = inStable[wch - 1];
        if (curW != wantOn || curW == sqWaitBase[i]) break;         // 아직 조건 안 맞거나, 대기 시작 때와 같은 상태에서 바뀐 적이 없음
        sqCondWait[i] = false;
      } else {
        if ((long)(now - sqDue[i]) < 0) break;                      // 아직 지연 시간이 안 지남
        setRelay(S.sr[r] >> 1, (S.sr[r] & 1) != 0);
        if (S.sd[r] & 0x8000) {                                     // 이 줄은 "조건 대기"로 전환(시작 시점 상태를 기록)
          sqCondWait[i] = true;
          uint8_t wch = (uint8_t)((S.sd[r] & 0x0F) + 1);
          sqWaitBase[i] = inStable[wch - 1];
          break;
        }
        sqDue[i] = now + (S.sd[r] & 0x7FFF);
      }
      sqStep[i] = r + 1;
      if (sqStep[i] >= S.nSteps) { sqRun[i] = false; break; }
      if ((S.sd[r] & 0x7FFF) > 0) break;
    }
  }
}


// ============================================================
//  0.96" OLED (SSD1306, I2C) + 외부 EEPROM(24LC256, I2C) 로 한글 문구 표시
//    보드에는 한글 폰트가 없으므로, 웹페이지가 글씨를 그림(1비트 비트맵, 128x64=1024바이트)으로
//    만들어 보내면 그 그림을 외부 EEPROM에 그대로 저장해두고, 이벤트가 오면 화면에 그대로 띄운다.
// ============================================================
#define OLED_ADDR   0x3C
#define EXT_EE_ADDR 0x50
#define LCD_MAX_EV  8
#define LCD_EV_BOOT   0
#define LCD_EV_INPUT  1
#define LCD_EV_OUTPUT 2
#define LCD_MAGIC 0x4C

struct LcdEvent {
  uint8_t type;   // 0 시작시 / 1 입력신호 / 2 릴레이신호
  uint8_t ch;     // 입력 1~N_IN, 릴레이 1~N_OUT, 시작시는 0(안씀)
  uint8_t state;  // 입력 1=눌림 0=뗌 / 릴레이 1=ON 0=OFF
  uint8_t slot;   // 외부 EEPROM 이미지 슬롯 0~31
};
LcdEvent lcdEv[LCD_MAX_EV];
uint8_t nLcdEv = 0;
bool lcdEditing = false;
unsigned long lcdEditLastMs = 0;
bool lcdInPrev[N_IN];
bool lcdOutPrev[N_OUT];

static void oledCmd(uint8_t c) {
  if (!oledEnabled) return;
  Wire.beginTransmission(OLED_ADDR);
  Wire.write((uint8_t)0x00);
  Wire.write(c);
  Wire.endTransmission();
}
static void oledInit() {
  if (!oledEnabled) return;
  static const uint8_t seq[] = {
    0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00, 0x40,
    0x8D, 0x14, 0x20, 0x00, 0xA1, 0xC8, 0xDA, 0x12,
    0x81, 0xCF, 0xD9, 0xF1, 0xDB, 0x40, 0xA4, 0xA6, 0xAF
  };
  for (uint8_t i = 0; i < sizeof(seq); i++) oledCmd(seq[i]);
}
static void oledSetFullWindow() {
  if (!oledEnabled) return;
  oledCmd(0x21); oledCmd(0x00); oledCmd(0x7F);   // 열(column) 0~127
  oledCmd(0x22); oledCmd(0x00); oledCmd(0x07);   // 페이지 0~7 (8페이지 x 8행 = 64행)
}
static void oledWriteData(const uint8_t *buf, uint8_t len) {
  if (!oledEnabled) return;
  Wire.beginTransmission(OLED_ADDR);
  Wire.write((uint8_t)0x40);
  for (uint8_t i = 0; i < len; i++) Wire.write(buf[i]);
  Wire.endTransmission();
}
static void oledClear() {
  if (!oledEnabled) return;
  oledSetFullWindow();
  uint8_t zero[32]; memset(zero, 0, sizeof(zero));
  for (uint16_t i = 0; i < 1024; i += 32) oledWriteData(zero, 32);
}
static bool extEepromWrite(uint16_t addr, const uint8_t *buf, uint8_t len) {   // len<=32 (24LC256 페이지 64바이트보다 작게 잘라 안전하게)
  if (!oledEnabled) return false;
  Wire.beginTransmission(EXT_EE_ADDR);
  Wire.write((uint8_t)(addr >> 8));
  Wire.write((uint8_t)(addr & 0xFF));
  for (uint8_t i = 0; i < len; i++) Wire.write(buf[i]);
  if (Wire.endTransmission() != 0) return false;
  delay(6);                                       // 내부 쓰기 사이클 대기
  return true;
}
static bool extEepromRead(uint16_t addr, uint8_t *buf, uint8_t len) {
  if (!oledEnabled) return false;
  Wire.beginTransmission(EXT_EE_ADDR);
  Wire.write((uint8_t)(addr >> 8));
  Wire.write((uint8_t)(addr & 0xFF));
  if (Wire.endTransmission(false) != 0) return false;
  uint8_t n = Wire.requestFrom((int)EXT_EE_ADDR, (int)len);
  for (uint8_t i = 0; i < n && i < len; i++) buf[i] = Wire.read();
  return n == len;
}
static void lcdShowSlot(uint8_t slot) {
  if (!oledEnabled) return;
  if (slot > 31) return;
  oledSetFullWindow();
  uint8_t buf[32];
  uint16_t base = (uint16_t)slot * 1024;
  for (uint16_t off = 0; off < 1024; off += 32) {
    if (!extEepromRead(base + off, buf, 32)) memset(buf, 0, sizeof(buf));
    oledWriteData(buf, 32);
  }
}

// I2C(OLED) 사용 여부를 바꾼다. 켤 때는 그 자리에서 바로 버스를 열고 초기화까지 해서,
// 재부팅 없이 즉시 화면이 켜지도록 한다.
static void setOledEnabled(bool on) {
  oledEnabled = on;
  EEPROM.update(EE_OLED, on ? 1 : 0);
  if (on) {
    Wire.begin();
    wireTimeout(Wire);   // I2C 선이 없거나 불량이어도 보드가 멈추지 않도록 25ms 뒤 포기
    oledInit();
    oledClear();
  }
}

static bool lcdEvValid(uint8_t n) {
  if (n > LCD_MAX_EV) return false;
  for (uint8_t i = 0; i < n; i++) {
    const LcdEvent &E = lcdEv[i];
    if (E.type > LCD_EV_OUTPUT) return false;
    if (E.type == LCD_EV_INPUT  && (E.ch < 1 || E.ch > N_IN  || E.state > 1)) return false;
    if (E.type == LCD_EV_OUTPUT && (E.ch < 1 || E.ch > N_OUT || E.state > 1)) return false;
    if (E.slot > 31) return false;
  }
  return true;
}
static void lcdResyncPrev() {
  for (uint8_t i = 0; i < N_IN;  i++) lcdInPrev[i]  = inStable[i];
  for (uint8_t i = 0; i < N_OUT; i++) lcdOutPrev[i] = relayOn[i];
}
static void lcdSaveEv() {
  EEPROM.update(EE_LCD, LCD_MAGIC);
  EEPROM.update(EE_LCD + 1, nLcdEv);
  for (uint8_t i = 0; i < nLcdEv; i++) {
    EEPROM.update(EE_LCD + 2 + i * 4 + 0, lcdEv[i].type);
    EEPROM.update(EE_LCD + 2 + i * 4 + 1, lcdEv[i].ch);
    EEPROM.update(EE_LCD + 2 + i * 4 + 2, lcdEv[i].state);
    EEPROM.update(EE_LCD + 2 + i * 4 + 3, lcdEv[i].slot);
  }
}
static void loadLcd() {
  memset(lcdEv, 0, sizeof(lcdEv));
  nLcdEv = 0;
  if (EEPROM.read(EE_LCD) == LCD_MAGIC) {
    uint8_t n = EEPROM.read(EE_LCD + 1);
    if (n <= LCD_MAX_EV) {
      for (uint8_t i = 0; i < n; i++) {
        lcdEv[i].type  = EEPROM.read(EE_LCD + 2 + i * 4 + 0);
        lcdEv[i].ch    = EEPROM.read(EE_LCD + 2 + i * 4 + 1);
        lcdEv[i].state = EEPROM.read(EE_LCD + 2 + i * 4 + 2);
        lcdEv[i].slot  = EEPROM.read(EE_LCD + 2 + i * 4 + 3);
      }
      if (lcdEvValid(n)) nLcdEv = n; else memset(lcdEv, 0, sizeof(lcdEv));
    }
  }
  lcdResyncPrev();
}
static void lcdEngine() {                             // 입력/릴레이 신호가 바뀌는 "그 순간"에 지정한 이미지를 띄운다
  updateInputs();
  if (lcdEditing) return;
  if (holdActive()) { lcdResyncPrev(); return; }
  bool inChanged[N_IN], outChanged[N_OUT];
  for (uint8_t i = 0; i < N_IN;  i++) inChanged[i]  = (inStable[i] != lcdInPrev[i]);
  for (uint8_t i = 0; i < N_OUT; i++) outChanged[i] = (relayOn[i]  != lcdOutPrev[i]);
  for (uint8_t i = 0; i < nLcdEv; i++) {
    const LcdEvent &E = lcdEv[i];
    if (E.type == LCD_EV_INPUT  && inChanged[E.ch - 1]  && inStable[E.ch - 1] == (E.state == 1)) lcdShowSlot(E.slot);
    else if (E.type == LCD_EV_OUTPUT && outChanged[E.ch - 1] && relayOn[E.ch - 1] == (E.state == 1)) lcdShowSlot(E.slot);
  }
  for (uint8_t i = 0; i < N_IN;  i++) lcdInPrev[i]  = inStable[i];
  for (uint8_t i = 0; i < N_OUT; i++) lcdOutPrev[i] = relayOn[i];
}

// ============================================================
//  명령 처리
// ============================================================
static char *tok(char **p) {
  char *s = *p;
  while (*s == ' ') s++;
  if (!*s) { *p = s; return NULL; }
  char *e = s;
  while (*e && *e != ' ') e++;
  if (*e) { *e = 0; e++; }
  *p = e;
  return s;
}

static int8_t hexNibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

static bool parseNum(char **p, long *out) {
  char *t = tok(p);
  if (!t) return false;
  char *s = t;
  if (*s == '-') s++;
  if (!*s) return false;
  for (char *c = s; *c; c++) if (*c < '0' || *c > '9') return false;
  *out = atol(t);
  return true;
}

static bool pinUsedByOther(uint8_t pin, bool isOut, uint8_t idx) {
  for (uint8_t i = 0; i < N_OUT; i++) if (!(isOut && i == idx) && outPin[i] == pin) return true;
  for (uint8_t i = 0; i < N_IN;  i++) if (!(!isOut && i == idx) && inPin[i] == pin) return true;
  return false;
}

static void handleLine(char *line) {
  for (char *q = line; *q; q++) if (*q >= 'a' && *q <= 'z') *q -= 32;
  char *p = line;
  char *cmd = tok(&p);
  if (!cmd) return;
  long a, b, c, d, e, f;

  if (!strcmp(cmd, "ID?")) { Serial.print(F("ID UNO_PLC ")); Serial.println(F(FW_VERSION)); return; }
  if (!strcmp(cmd, "PING")) { Serial.println(F("PONG")); return; }

  if (!strcmp(cmd, "OLED?")) { Serial.print(F("OLED ")); Serial.println(oledEnabled ? 1 : 0); return; }
  if (!strcmp(cmd, "SETOLED")) {
    if (!parseNum(&p, &a) || (a != 0 && a != 1)) { replyErr(F("ARGS")); return; }
    setOledEnabled(a == 1);
    replyOK();
    return;
  }

  if (!strcmp(cmd, "PIN")) {                          // 응답 없음 (웹페이지가 응답을 기다리지 않음)
    if (!parseNum(&p, &a)) return;
    char *st = tok(&p);
    if (!st || a < 1 || a > N_OUT) return;
    if (!strcmp(st, "ON")) setRelay(a, true);
    else if (!strcmp(st, "OFF")) setRelay(a, false);
    return;
  }

  if (!strcmp(cmd, "STATUS?")) {
    Serial.print(F("STATUS "));
    for (uint8_t i = 0; i < N_OUT; i++) { if (i) Serial.print(','); Serial.print(relayOn[i] ? 1 : 0); }
    Serial.println();
    return;
  }
  if (!strcmp(cmd, "INPUT?")) {
    Serial.print(F("INPUT "));
    for (uint8_t i = 0; i < N_IN; i++) { if (i) Serial.print(','); Serial.print(inRaw(i + 1) ? 1 : 0); }
    Serial.println();
    return;
  }

  if (!strcmp(cmd, "GETRMODE?")) { Serial.print(F("RMODE ")); Serial.println((outInvMask == 0) ? 1 : 0); return; }   // 참고용: 전 채널이 기본(active-LOW)이면 1. 채널별 반전은 GETOUTINV?/SETOUTINV 사용
  if (!strcmp(cmd, "SETRMODE")) { replyErr(F("USE_SETOUTINV")); return; }   // v1.9.1부터 채널별 SETOUTINV로 대체됨

  if (!strcmp(cmd, "GETOUTINV?")) {
    Serial.print(F("OUTINV "));
    for (uint8_t i = 0; i < N_OUT; i++) { if (i) Serial.print(','); Serial.print((outInvMask >> i) & 1); }
    Serial.println();
    return;
  }
  if (!strcmp(cmd, "SETOUTINV")) {                    // SETOUTINV <ch> <0|1>  (0=기존 방식·active-LOW, 1=반전·active-HIGH)
    if (!parseNum(&p, &a) || !parseNum(&p, &b) || a < 1 || a > N_OUT || (b != 0 && b != 1)) { replyErr(F("ARGS")); return; }
    uint8_t ch = (uint8_t)a;
    if (b) outInvMask |= (uint8_t)(1u << (ch - 1)); else outInvMask &= (uint8_t)~(1u << (ch - 1));
    saveOutInv();
    setRelay(ch, relayOn[ch - 1]);                    // 지금 ON/OFF 상태는 유지한 채 새 반전 설정에 맞춰 핀 레벨만 다시 씀
    replyOK();
    return;
  }

  if (!strcmp(cmd, "GETCONFIG?")) {
    Serial.print(F("CONFIG OUT "));
    for (uint8_t i = 0; i < N_OUT; i++) { if (i) Serial.print(','); Serial.print(i + 1); Serial.print(':'); Serial.print(outPin[i]); }
    Serial.print(F(" IN "));
    for (uint8_t i = 0; i < N_IN; i++) { if (i) Serial.print(','); Serial.print(i + 1); Serial.print(':'); Serial.print(inPin[i]); }
    Serial.println();
    return;
  }

  if (!strcmp(cmd, "SETOUT") || !strcmp(cmd, "SETIN")) {
    bool isOut = (cmd[3] == 'O');
    if (!parseNum(&p, &a) || !parseNum(&p, &b) || a < 1 || a > (isOut ? N_OUT : N_IN)) { replyErr(F("ARGS")); return; }
    if (!validPin(b)) { replyErr(F("PIN_RANGE")); return; }
    uint8_t i = (uint8_t)(a - 1);
    if (pinUsedByOther((uint8_t)b, isOut, i)) { replyErr(F("PIN_IN_USE")); return; }
    if (isOut) {
      if (outPin[i] != b) {
        bool invOld = ((outInvMask >> i) & 1) != 0;
        digitalWrite(outPin[i], invOld ? LOW : HIGH);   // 예전 핀은 OFF 레벨로(채널별 반전 설정 반영)
        outPin[i] = (uint8_t)b;
        initRelayPin((uint8_t)a);
        setRelay((uint8_t)a, relayOn[i]);
        saveMaps();
      }
    } else {
      if (inPin[i] != b) {
        inPin[i] = (uint8_t)b;
        pinMode(inPin[i], INPUT_PULLUP);
        inLast[i] = inStable[i] = pinPressed(i);  // 가짜 에지 방지
        inChangedAt[i] = millis();
        saveMaps();
      }
    }
    replyOK();
    return;
  }

  if (!strcmp(cmd, "GETINV?")) {
    Serial.print(F("INV "));
    for (uint8_t i = 0; i < N_IN; i++) { if (i) Serial.print(','); Serial.print((inInvMask >> i) & 1); }
    Serial.println();
    return;
  }
  if (!strcmp(cmd, "SETINV")) {                       // SETINV <ch> <0|1>
    if (!parseNum(&p, &a) || !parseNum(&p, &b) || a < 1 || a > N_IN || (b != 0 && b != 1)) { replyErr(F("ARGS")); return; }
    uint8_t i = (uint8_t)(a - 1);
    if (b) inInvMask |= (uint16_t)(1u << i); else inInvMask &= (uint16_t)~(1u << i);
    saveInv();
    inLast[i] = inStable[i] = pinPressed(i);          // 가짜 에지 방지
    inChangedAt[i] = millis();
    resyncSeqPrev();
    lcdResyncPrev();
    replyOK();
    return;
  }

  if (!strcmp(cmd, "GETBOOT?")) {                     // 전원 투입 시 릴레이 시작 상태 (v1.8)
    Serial.print(F("BOOT "));
    for (uint8_t i = 0; i < N_OUT; i++) { if (i) Serial.print(','); Serial.print((outBootMask >> i) & 1); }
    Serial.println();
    return;
  }
  if (!strcmp(cmd, "SETBOOT")) {                      // SETBOOT <릴레이ch> <0|1>
    if (!parseNum(&p, &a) || !parseNum(&p, &b) || a < 1 || a > N_OUT || (b != 0 && b != 1)) { replyErr(F("ARGS")); return; }
    uint8_t i = (uint8_t)(a - 1);
    if (b) outBootMask |= (uint8_t)(1u << i); else outBootMask &= (uint8_t)~(1u << i);
    saveBootMask();
    replyOK();
    return;
  }

  if (!strcmp(cmd, "GETMODE?")) { Serial.print(F("MODE ")); Serial.println(runMode); return; }
  if (!strcmp(cmd, "SETMODE")) {
    if (parseNum(&p, &a) && a == MODE_SEQ) replyOK();
    else replyErr(F("ARGS"));
    return;
  }

  // ----- LCD (OLED 문구) -----
  if (!strcmp(cmd, "LCDCLR")) {
    memset(lcdEv, 0, sizeof(lcdEv));
    nLcdEv = 0;
    lcdEditing = true;
    lcdEditLastMs = millis();
    replyOK();
    return;
  }
  if (!strcmp(cmd, "LCDDEF")) {
    if (!lcdEditing) { replyErr(F("NOT_EDITING")); return; }
    lcdEditLastMs = millis();
    if (!parseNum(&p, &a) || !parseNum(&p, &b) || !parseNum(&p, &c) || !parseNum(&p, &d) || !parseNum(&p, &e)
        || a < 0 || a >= LCD_MAX_EV || b < 0 || b > 2 || d < 0 || d > 1 || e < 0 || e > 31) {
      replyErr(F("ARGS")); return;
    }
    if (b == LCD_EV_INPUT  && (c < 1 || c > N_IN))  { replyErr(F("ARGS")); return; }
    if (b == LCD_EV_OUTPUT && (c < 1 || c > N_OUT)) { replyErr(F("ARGS")); return; }
    if (b == LCD_EV_BOOT) c = 0;
    lcdEv[a].type = (uint8_t)b; lcdEv[a].ch = (uint8_t)c; lcdEv[a].state = (uint8_t)d; lcdEv[a].slot = (uint8_t)e;
    if (a + 1 > nLcdEv) nLcdEv = (uint8_t)(a + 1);
    replyOK();
    return;
  }
  if (!strcmp(cmd, "LCDSAVE")) {
    if (!lcdEditing) { replyErr(F("NOT_EDITING")); return; }
    if (!lcdEvValid(nLcdEv)) { replyErr(F("INVALID")); return; }
    lcdSaveEv();
    lcdEditing = false;
    lcdResyncPrev();
    Serial.print(F("OK LCDSAVE ")); Serial.println(nLcdEv);
    return;
  }
  if (!strcmp(cmd, "LCDABORT")) {
    if (lcdEditing) { lcdEditing = false; loadLcd(); }
    replyOK();
    return;
  }
  if (!strcmp(cmd, "LCDGET?")) {
    Serial.print(F("LCDINFO ")); Serial.println(nLcdEv);
    for (uint8_t i = 0; i < nLcdEv; i++) {
      Serial.print(F("LCDE ")); Serial.print(i); Serial.print(' ');
      Serial.print(lcdEv[i].type); Serial.print(' '); Serial.print(lcdEv[i].ch); Serial.print(' ');
      Serial.print(lcdEv[i].state); Serial.print(' '); Serial.println(lcdEv[i].slot);
    }
    Serial.println(F("LCDEND"));
    return;
  }
  if (!strcmp(cmd, "LCDIMG")) {
    if (!oledEnabled) { replyErr(F("OLED_OFF")); return; }
    if (!parseNum(&p, &a) || !parseNum(&p, &b) || a < 0 || a > 31 || b < 0 || b > 1023) { replyErr(F("ARGS")); return; }
    char *hex = tok(&p);
    if (!hex) { replyErr(F("ARGS")); return; }
    uint8_t buf[64]; uint8_t n = 0;
    for (char *h = hex; h[0] && h[1] && n < 64; h += 2) {
      int8_t hi = hexNibble(h[0]), lo = hexNibble(h[1]);
      if (hi < 0 || lo < 0) { replyErr(F("HEX")); return; }
      buf[n++] = (uint8_t)((hi << 4) | lo);
    }
    if (n == 0 || ((uint16_t)b + n) > 1024) { replyErr(F("ARGS")); return; }
    uint16_t base = (uint16_t)a * 1024 + (uint16_t)b;
    for (uint8_t off = 0; off < n; off += 32) {          // 24LC256 페이지(64B)보다 작게 32B씩 나눠 쓴다
      uint8_t len = (uint8_t)((n - off > 32) ? 32 : (n - off));
      if (!extEepromWrite(base + off, buf + off, len)) { replyErr(F("EEPROM_EXT")); return; }
    }
    replyOK();
    return;
  }
  if (!strcmp(cmd, "LCDSHOW")) {
    if (!oledEnabled) { replyErr(F("OLED_OFF")); return; }
    if (!parseNum(&p, &a) || a < 0 || a > 31) { replyErr(F("ARGS")); return; }
    lcdShowSlot((uint8_t)a);
    replyOK();
    return;
  }

  // ----- 시퀀스 (모드 2) -----
  if (!strcmp(cmd, "SEQCLR")) {
    memset(seqs, 0, sizeof(seqs));
    nSeqs = 0;
    resetSeqRuntime();
    seqEditing = true;
    seqEditLastMs = millis();
    replyOK();
    return;
  }
  if (!strcmp(cmd, "SEQDEF")) {
    if (!seqEditing) { replyErr(F("NOT_EDITING")); return; }
    seqEditLastMs = millis();
    if (!parseNum(&p, &a) || !parseNum(&p, &b) || !parseNum(&p, &c) || !parseNum(&p, &d)
        || a < 0 || a >= MAX_SEQ || b < 0 || b > N_IN || c < 0 || c > 2 || (d != 0 && d != 1)) {
      replyErr(F("ARGS")); return;
    }
    memset(&seqs[a], 0, sizeof(SeqDef));
    seqs[a].trigCh = (uint8_t)b; seqs[a].trigCond = (uint8_t)c; seqs[a].enabled = (uint8_t)d;
    if (a + 1 > nSeqs) nSeqs = (uint8_t)(a + 1);
    replyOK();
    return;
  }
  if (!strcmp(cmd, "SEQCOND")) {
    if (!seqEditing) { replyErr(F("NOT_EDITING")); return; }
    seqEditLastMs = millis();
    if (!parseNum(&p, &a) || !parseNum(&p, &b) || !parseNum(&p, &c)
        || a < 0 || a >= nSeqs || b < 1 || b > N_IN || (c != 0 && c != 1)) { replyErr(F("ARGS")); return; }
    SeqDef &S = seqs[a];
    if (S.nConds >= MAX_CONDS) { replyErr(F("FULL")); return; }
    S.cc[S.nConds++] = (uint8_t)((b << 1) | c);
    replyOK();
    return;
  }
  if (!strcmp(cmd, "SEQSTEP")) {                      // SEQSTEP <i> <릴레이ch> <state> <지연ms> [<대기ch> <대기상태>]
    if (!seqEditing) { replyErr(F("NOT_EDITING")); return; }
    seqEditLastMs = millis();
    if (!parseNum(&p, &a) || !parseNum(&p, &b) || !parseNum(&p, &c) || !parseNum(&p, &d)
        || a < 0 || a >= nSeqs || b < 1 || b > N_OUT || (c != 0 && c != 1) || d < 0) {
      replyErr(F("ARGS")); return;
    }
    SeqDef &S = seqs[a];
    if (S.nSteps >= MAX_STEPS) { replyErr(F("FULL")); return; }
    bool hasWait = parseNum(&p, &e) && parseNum(&p, &f);   // 뒤에 두 숫자가 더 있으면 "조건 대기" 줄 (v1.9)
    uint16_t sdVal;
    if (hasWait) {
      if (e < 1 || e > N_IN || (f != 0 && f != 1)) { replyErr(F("ARGS")); return; }
      sdVal = (uint16_t)(0x8000 | (uint16_t)((e - 1) & 0x0F) | (f ? 0x10 : 0));
    } else {
      if (d > 32767) { replyErr(F("ARGS")); return; }
      sdVal = (uint16_t)d;
    }
    S.sr[S.nSteps] = (uint8_t)((b << 1) | c);
    S.sd[S.nSteps] = sdVal;
    S.nSteps++;
    replyOK();
    return;
  }
  if (!strcmp(cmd, "SEQSAVE")) {
    if (!seqEditing) { replyErr(F("NOT_EDITING")); return; }
    if (!seqsValid(nSeqs)) { replyErr(F("INVALID")); return; }
    if (!writeSeqs()) { replyErr(F("EEPROM")); return; }
    seqEditing = false;
    resetSeqRuntime();
    resyncSeqPrev();
    uint16_t steps = 0;
    for (uint8_t i = 0; i < nSeqs; i++) steps += seqs[i].nSteps;
    Serial.print(F("OK SEQSAVE ")); Serial.print(nSeqs); Serial.print(' '); Serial.println(steps);
    return;
  }
  if (!strcmp(cmd, "SEQABORT")) {
    if (seqEditing) { seqEditing = false; loadSeqs(); }
    replyOK();
    return;
  }
  if (!strcmp(cmd, "SEQGET?")) {
    Serial.print(F("SEQINFO ")); Serial.println(nSeqs);
    for (uint8_t i = 0; i < nSeqs; i++) {
      const SeqDef &S = seqs[i];
      Serial.print(F("SEQD ")); Serial.print(i); Serial.print(' '); Serial.print(S.trigCh); Serial.print(' ');
      Serial.print(S.trigCond); Serial.print(' '); Serial.print(S.enabled); Serial.print(' ');
      Serial.print(S.nConds); Serial.print(' '); Serial.println(S.nSteps);
      for (uint8_t c2 = 0; c2 < S.nConds; c2++) {
        Serial.print(F("SEQC ")); Serial.print(i); Serial.print(' ');
        Serial.print(S.cc[c2] >> 1); Serial.print(' '); Serial.println(S.cc[c2] & 1);
      }
      for (uint8_t r = 0; r < S.nSteps; r++) {
        Serial.print(F("SEQS ")); Serial.print(i); Serial.print(' ');
        Serial.print(S.sr[r] >> 1); Serial.print(' '); Serial.print(S.sr[r] & 1); Serial.print(' ');
        if (S.sd[r] & 0x8000) {                       // "조건 대기" 줄: 지연은 0으로, 뒤에 대기채널·대기상태를 덧붙여 응답 (v1.9)
          uint8_t wch = (uint8_t)((S.sd[r] & 0x0F) + 1);
          bool wantOn = (S.sd[r] & 0x10) != 0;
          Serial.print(0); Serial.print(' '); Serial.print(wch); Serial.print(' '); Serial.println(wantOn ? 1 : 0);
        } else {
          Serial.println(S.sd[r] & 0x7FFF);
        }
      }
    }
    Serial.println(F("SEQEND"));
    return;
  }
  if (!strcmp(cmd, "SEQSTAT?")) {
    Serial.print(F("SEQSTAT "));
    if (!nSeqs) Serial.print('-');
    for (uint8_t i = 0; i < nSeqs; i++) { if (i) Serial.print(','); Serial.print(sqRun[i] ? 1 : 0); }
    Serial.println();
    return;
  }
  if (!strcmp(cmd, "SEQRUN")) {
    if (!parseNum(&p, &a) || a < 0 || a >= nSeqs) { replyErr(F("ARGS")); return; }
    if (runMode != MODE_SEQ || seqEditing) { replyErr(F("NOT_SEQ_MODE")); return; }
    seqStart((uint8_t)a);
    replyOK();
    return;
  }
  if (!strcmp(cmd, "SEQSTOP")) { resetSeqRuntime(); replyOK(); return; }

  replyErr(F("UNKNOWN"));
}

static void service() {                               // 시리얼 수신 처리 (loop()에서 계속 호출됨)
  while (Serial.available()) {
    char ch = (char)Serial.read();
    if (ch == '\n' || ch == '\r') {
      if (lineOverflow) { replyErr(F("LINE_TOO_LONG")); lineOverflow = false; lineLen = 0; }
      else if (lineLen > 0) { lineBuf[lineLen] = 0; lineLen = 0; handleLine(lineBuf); }
    } else if (lineOverflow) {
      // 줄이 끝날 때까지 버림
    } else if (lineLen < sizeof(lineBuf) - 1) {
      lineBuf[lineLen++] = ch;
    } else {
      lineOverflow = true;
    }
  }
  if (seqEditing && (millis() - seqEditLastMs) > CNT_EDIT_TIMEOUT_MS) {   // 시퀀스 편집 중 PC가 끊기면 저장돼 있던 내용으로 복귀
    seqEditing = false;
    loadSeqs();
  }
  if (lcdEditing && (millis() - lcdEditLastMs) > CNT_EDIT_TIMEOUT_MS) {   // LCD 문구 편집 중 PC가 끊기면 저장돼 있던 내용으로 복귀
    lcdEditing = false;
    loadLcd();
  }
  lcdEngine();          // 모드에 상관없이 신호를 감시해 문구를 띄운다
}

// ============================================================
//  setup / loop
// ============================================================
void setup() {
  Serial.begin(115200);
  Serial.println(F("BOOT"));         // 진단용: 웹페이지 통신 로그에 BOOT → READY 가 보이면 정상 시작
  loadConfig();
  loadInv();                  // 입력 반전 설정(없으면 모두 0)
  loadBootMask();              // 전원 투입 시 시작 상태(없으면 모두 OFF) (v1.8)
  loadOutInv();                // 출력(릴레이) 반전 설정(없으면 모두 0 = 기존 active-LOW) (v1.9.1)
  for (uint8_t i = 0; i < N_OUT; i++) { relayOn[i] = false; initRelayPin(i + 1); }
  for (uint8_t i = 0; i < N_OUT; i++) { if ((outBootMask >> i) & 1) setRelay(i + 1, true); }
  for (uint8_t i = 0; i < N_IN; i++) {
    pinMode(inPin[i], INPUT_PULLUP);
    inLast[i] = inStable[i] = pinPressed(i);
    inChangedAt[i] = millis();
  }
  loadSeqs();

  if (oledEnabled) {          // 저장된 설정이 "사용함"이면 부팅할 때 바로 I2C를 켠다
    Wire.begin();
    wireTimeout(Wire);        // I2C 선이 합선/불량이어도 보드가 멈추지 않도록 25ms 뒤 포기
    oledInit();
    oledClear();
  }
  // oledEnabled가 꺼져 있으면(기본값) I2C를 아예 쓰지 않습니다 — 장치가 없는데 버스를
  // 여는 것 자체가 불필요한 지연/위험 요소이기 때문입니다. 웹페이지에서 "OLED 사용함"으로
  // 바꾸면(SETOLED 1) 그 즉시 켜지고, 다음 부팅부터도 이 설정이 그대로 적용됩니다.
  loadLcd();
  for (uint8_t i = 0; i < nLcdEv; i++) {
    if (lcdEv[i].type == LCD_EV_BOOT) { lcdShowSlot(lcdEv[i].slot); break; }
  }
  Serial.println(F("READY"));
}

void loop() {
  service();
  seqEngine();
}
