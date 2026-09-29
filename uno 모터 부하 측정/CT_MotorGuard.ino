// ============================================================
//  CT 모터 가드 (CT_MotorGuard)  v1.0
//  전용 보드: 모터가 걸려서(구속) 못 돌 때 CT 전류센서로 감지 →
//             정지 → 대기 → 반대 방향 신호 → 역회전으로 풀기 →
//             다시 정지 → 대기 → 정방향 복귀. 계속 걸리면 N회까지
//             재시도 후 알람 정지.
//  ※ UNO PLC(시퀀스) 보드와는 별개의 "전용" 보드/스케치입니다.
//
//  ── 하드웨어 (기본값 — SET 명령으로 바꾸고 EEPROM에 저장 가능) ──
//    RUN 릴레이  = 핀 3   (모터 스피드컨트롤러 "구동" 신호. active-LOW: LOW=ON)
//    DIR 릴레이  = 핀 4   (모터 스피드컨트롤러 "방향" 신호. OFF=정방향, ON=역방향)
//    CT 센서 OUT = A0     (ZMCT103C 등, 보드 자체 가변저항으로 1차 게인 조절 후 이 핀으로 입력)
//    ※ RUN 릴레이가 필요 없는 배선(항상 구동 상태, 방향 신호만 바꾸는 경우)이면
//       RUN 핀을 아무 데나 안 쓰는 핀으로 두거나, SETMON 1로 켠 뒤 그냥 무시해도 됩니다.
//
//  ── 동작 원리 ──
//    CT는 AC 전류를 측정하므로 A0에는 원래 2.5V(중간전압) 근처에서 위아래로 흔들리는
//    파형이 들어옵니다. 매 루프마다 analogRead(A0)를 하면서 SAMPLE_WINDOW_MS(기본 60ms,
//    50/60Hz 한 주기 이상) 동안의 최댓값-최솟값(peak-to-peak)을 구해 "raw 부하값"으로 씁니다.
//    이 raw 값이 THRESHOLD를 HOLD_MS 이상 계속 넘으면 "걸림"으로 판단합니다.
//    ※ 실제 몇 A인지 환산하는 대신 "raw 값"을 그대로 씁니다 — CT 보드 가변저항 게인이
//      제품마다 달라서, 모터를 정상 회전/강제로 눌러 멈춘 상태에서 RAW?/실시간 값을 직접
//      찍어보고 그 사이의 값으로 THRESHOLD를 잡는 것이 가장 정확합니다(그래서 이 보드를
//      먼저 만들어 "측정"부터 하는 것입니다).
//
//  ── 상태(STATUS? 의 state 값) ──
//    0 = MON_OFF        모니터링 꺼짐(SETMON 0) — 릴레이 모두 OFF
//    1 = SETTLE         기동/재시작 직후 유예시간(SETTLE_MS) — 돌입전류로 인한 오탐 방지, 릴레이 상태 유지
//    2 = NORMAL          정상 감시 중
//    3 = STALL_STOP      걸림 감지 → RUN 끄는 중(바로 다음 루프에서 꺼짐)
//    4 = STOP_WAIT        RUN 끈 뒤 STOP_DELAY_MS 대기 중(회전 중 방향전환 금지 규정 때문)
//    5 = REVERSING        DIR 반전 후 RUN 켜서 역회전 중(REV_MS 동안)
//    6 = REV_STOP_WAIT    역회전 끝, RUN 끄고 RESUME_DELAY_MS 대기 중
//    7 = ALARM            재시도 횟수 초과 → 릴레이 모두 OFF, RESET 명령 전까지 정지 유지
//
//  ── 명령 (115200bps, 줄 끝 \r\n, 웹페이지 CT_MotorGuard.html과 세트) ──
//    ID?                          → ID CT_GUARD 1.0
//    STATUS?                      → STATUS <state> <raw> <run 0|1> <dir 0|1> <retry> <alarm 0|1>
//    RAW?                         → RAW <raw>                (지금 순간의 부하값, 켜짐/꺼짐 상관없이 계속 측정)
//    GETCONFIG?                   → CONFIG <runPin> <dirPin> <ctPin(0~5=A0~A5)> <threshold> <holdMs>
//                                    <stopDelayMs> <revMs> <resumeDelayMs> <retryMax> <settleMs>
//    SETRUNPIN <pin>  / SETDIRPIN <pin>     릴레이 핀 번호(2~13)
//    SETCTPIN <0~5>                CT 입력 아날로그 핀(0=A0 1=A1 2=A2 3=A3 4=A4 5=A5)
//    SETTH <0~1023>                걸림 판단 임계값(raw)
//    SETHOLD <ms>                  임계값을 이 시간 이상 넘어야 "걸림" 확정(순간 스파이크 무시)
//    SETSTOPDELAY <ms>             RUN 끈 뒤 방향 바꾸기 전 대기시간
//    SETREVMS <ms>                 역회전 지속시간
//    SETRESUMEDELAY <ms>           역회전 끝나고 정지 후 정방향 복귀 전 대기시간
//    SETRETRY <0~10>               역회전으로 풀어도 또 걸릴 때 최대 재시도 횟수(초과하면 ALARM)
//    SETSETTLE <ms>                기동/복귀 직후 걸림감지를 쉬는 시간(돌입전류 무시용)
//    SAVECFG                       위 설정들을 EEPROM에 저장
//    SETMON <0|1>                  모니터링 시작/정지(0이면 릴레이 강제 OFF, 수동 테스트만 가능)
//    MANUAL RUN <0|1>              SETMON 0 상태에서만 동작(배선 확인용 수동 테스트)
//    MANUAL DIR <0|1>              SETMON 0 상태에서만 동작(배선 확인용 수동 테스트)
//    RESET                         ALARM 해제하고 SETTLE 상태로 정상 복귀(재시도 카운트 0으로)
// ============================================================
#include <EEPROM.h>

#define FW_VERSION "1.0"

// ---------- 핀/설정 (기본값) ----------
uint8_t runPin = 3;
uint8_t dirPin = 4;
uint8_t ctPinIdx = 0;              // 0~5 = A0~A5
uint16_t threshold   = 200;         // raw peak-to-peak 임계값(기본값 — 반드시 실측 후 조정)
uint16_t holdMs       = 300;        // 이 시간 이상 넘어야 걸림 확정
uint16_t stopDelayMs  = 500;        // 정지→반전 대기
uint16_t revMs         = 1500;       // 역회전 지속시간
uint16_t resumeDelayMs = 500;        // 역회전 정지→정방향 복귀 대기
uint8_t  retryMax      = 3;
uint16_t settleMs      = 2000;       // 기동/복귀 직후 걸림감지 유예(돌입전류)

bool monEnabled = false;            // SETMON 0/1
bool runOn = false, dirRev = false; // 현재 릴레이 상태(항상 active-LOW로 물리 핀 제어)

// ---------- EEPROM ----------
#define EE_MAGIC 0
#define EE_MAGIC_VALUE 0xC7
#define EE_CFG   1     // 구조체 그대로 저장 + 체크바이트 1개

struct Cfg {
  uint8_t runPin, dirPin, ctPinIdx;
  uint16_t threshold, holdMs, stopDelayMs, revMs, resumeDelayMs, settleMs;
  uint8_t retryMax;
};

static uint8_t cfgChk(const Cfg &c) {
  const uint8_t *b = (const uint8_t*)&c;
  uint8_t chk = 0x5A;
  for (uint8_t i = 0; i < sizeof(Cfg); i++) chk = (uint8_t)((chk << 1) | (chk >> 7)) ^ b[i];
  return chk;
}

static void applyCfg(const Cfg &c) {
  runPin = c.runPin; dirPin = c.dirPin; ctPinIdx = c.ctPinIdx;
  threshold = c.threshold; holdMs = c.holdMs; stopDelayMs = c.stopDelayMs;
  revMs = c.revMs; resumeDelayMs = c.resumeDelayMs; settleMs = c.settleMs;
  retryMax = c.retryMax;
}

static void saveCfg();   // 전방 선언

static void loadCfg() {
  if (EEPROM.read(EE_MAGIC) != EE_MAGIC_VALUE) {   // 처음 쓰는 보드: 기본값을 그대로 EEPROM에 저장
    EEPROM.update(EE_MAGIC, EE_MAGIC_VALUE);
    saveCfg();
    return;
  }
  Cfg c; EEPROM.get(EE_CFG, c);
  uint8_t chk = EEPROM.read(EE_CFG + sizeof(Cfg));
  if (chk == cfgChk(c)) applyCfg(c);               // 체크바이트가 맞을 때만 적용, 아니면 기본값 유지
}

static void saveCfg() {
  Cfg c = {runPin, dirPin, ctPinIdx, threshold, holdMs, stopDelayMs, revMs, resumeDelayMs, settleMs, retryMax};
  EEPROM.put(EE_CFG, c);
  EEPROM.update(EE_CFG + sizeof(Cfg), cfgChk(c));
}

// ---------- 릴레이 (active-LOW 고정: LOW=ON) ----------
static void applyRun()  { pinMode(runPin, OUTPUT); digitalWrite(runPin, runOn  ? LOW : HIGH); }
static void applyDir()  { pinMode(dirPin, OUTPUT); digitalWrite(dirPin, dirRev ? LOW : HIGH); }
static void setRun(bool on)  { runOn = on;  applyRun(); }
static void setDir(bool rev) { dirRev = rev; applyDir(); }

// ---------- CT 측정 (peak-to-peak, 논블로킹 윈도우) ----------
#define SAMPLE_WINDOW_MS 60UL     // 50/60Hz 최소 한 주기 이상 잡기 위한 윈도우
uint16_t sampMin = 1023, sampMax = 0;
unsigned long sampWinStart = 0;
uint16_t rawLoad = 0;             // 최근 윈도우에서 계산된 peak-to-peak 값(=부하 지표)

static uint8_t ctPin() { return A0 + ctPinIdx; }

static void ctSampleTick() {
  int v = analogRead(ctPin());
  if (v < sampMin) sampMin = v;
  if (v > sampMax) sampMax = v;
  unsigned long now = millis();
  if (now - sampWinStart >= SAMPLE_WINDOW_MS) {
    rawLoad = (uint16_t)(sampMax - sampMin);
    sampMin = 1023; sampMax = 0;
    sampWinStart = now;
  }
}

// ---------- 상태 머신 ----------
enum State { ST_MON_OFF=0, ST_SETTLE=1, ST_NORMAL=2, ST_STALL_STOP=3, ST_STOP_WAIT=4,
             ST_REVERSING=5, ST_REV_STOP_WAIT=6, ST_ALARM=7 };
uint8_t state = ST_MON_OFF;
unsigned long stateDue = 0;         // 다음 전이까지 남은 시각(millis 기준)
unsigned long overSince = 0;        // 임계값을 넘기 시작한 시각(0=안 넘는 중)
uint8_t retryCount = 0;

static void enterState(uint8_t s, unsigned long delayMs = 0) {
  state = s; stateDue = millis() + delayMs;
}

static void startMonitoring() {
  retryCount = 0;
  overSince = 0;
  setDir(false);           // 정방향으로 시작
  setRun(true);
  enterState(ST_SETTLE, settleMs);
}

static void stopAll() { setRun(false); }

static void stateMachine() {
  unsigned long now = millis();
  switch (state) {
    case ST_MON_OFF:
      return; // SETMON 1 이 올 때까지 아무것도 안 함(릴레이는 MANUAL 명령으로만 바뀜)

    case ST_SETTLE:
      if ((long)(now - stateDue) >= 0) { overSince = 0; enterState(ST_NORMAL); }
      return;

    case ST_NORMAL: {
      if (rawLoad >= threshold) {
        if (overSince == 0) overSince = now;
        if (now - overSince >= holdMs) {          // 걸림 확정
          overSince = 0;
          enterState(ST_STALL_STOP);
        }
      } else {
        overSince = 0;
      }
      return;
    }

    case ST_STALL_STOP:
      stopAll();
      enterState(ST_STOP_WAIT, stopDelayMs);
      return;

    case ST_STOP_WAIT:
      if ((long)(now - stateDue) >= 0) {
        setDir(true);              // 역방향
        setRun(true);
        enterState(ST_REVERSING, revMs);
      }
      return;

    case ST_REVERSING:
      if ((long)(now - stateDue) >= 0) {
        stopAll();
        enterState(ST_REV_STOP_WAIT, resumeDelayMs);
      }
      return;

    case ST_REV_STOP_WAIT:
      if ((long)(now - stateDue) >= 0) {
        setDir(false);             // 정방향 복귀
        if (retryCount >= retryMax) {
          stopAll();
          enterState(ST_ALARM);
        } else {
          retryCount++;
          setRun(true);
          overSince = 0;
          enterState(ST_SETTLE, settleMs);   // 복귀 직후 돌입전류 유예
        }
      }
      return;

    case ST_ALARM:
      stopAll();
      return;
  }
}

// ---------- 시리얼 ----------
char lineBuf[64]; uint8_t lineLen = 0;

static void replyOK() { Serial.println(F("OK")); }
static void replyErr(const __FlashStringHelper *why) { Serial.print(F("ERR ")); Serial.println(why); }

static long toL(const char *s) { return atol(s); }

static void handleLine(char *line) {
  char *save; char *cmd = strtok_r(line, " ", &save);
  if (!cmd) return;

  if (!strcmp(cmd, "ID?")) { Serial.println(F("ID CT_GUARD 1.0")); return; }

  if (!strcmp(cmd, "STATUS?")) {
    Serial.print(F("STATUS ")); Serial.print(state); Serial.print(' ');
    Serial.print(rawLoad); Serial.print(' ');
    Serial.print(runOn ? 1 : 0); Serial.print(' ');
    Serial.print(dirRev ? 1 : 0); Serial.print(' ');
    Serial.print(retryCount); Serial.print(' ');
    Serial.println(state == ST_ALARM ? 1 : 0);
    return;
  }

  if (!strcmp(cmd, "RAW?")) { Serial.print(F("RAW ")); Serial.println(rawLoad); return; }

  if (!strcmp(cmd, "GETCONFIG?")) {
    Serial.print(F("CONFIG "));
    Serial.print(runPin); Serial.print(' ');
    Serial.print(dirPin); Serial.print(' ');
    Serial.print(ctPinIdx); Serial.print(' ');
    Serial.print(threshold); Serial.print(' ');
    Serial.print(holdMs); Serial.print(' ');
    Serial.print(stopDelayMs); Serial.print(' ');
    Serial.print(revMs); Serial.print(' ');
    Serial.print(resumeDelayMs); Serial.print(' ');
    Serial.print(retryMax); Serial.print(' ');
    Serial.println(settleMs);
    return;
  }

  char *a1 = strtok_r(NULL, " ", &save);

  if (!strcmp(cmd, "SETRUNPIN") && a1) { runPin = (uint8_t)toL(a1); applyRun(); replyOK(); return; }
  if (!strcmp(cmd, "SETDIRPIN") && a1) { dirPin = (uint8_t)toL(a1); applyDir(); replyOK(); return; }
  if (!strcmp(cmd, "SETCTPIN")  && a1) { long v=toL(a1); if(v<0||v>5){replyErr(F("ARGS"));return;} ctPinIdx=(uint8_t)v; replyOK(); return; }
  if (!strcmp(cmd, "SETTH")      && a1) { long v=toL(a1); if(v<0||v>1023){replyErr(F("ARGS"));return;} threshold=(uint16_t)v; replyOK(); return; }
  if (!strcmp(cmd, "SETHOLD")    && a1) { holdMs=(uint16_t)toL(a1); replyOK(); return; }
  if (!strcmp(cmd, "SETSTOPDELAY") && a1) { stopDelayMs=(uint16_t)toL(a1); replyOK(); return; }
  if (!strcmp(cmd, "SETREVMS")     && a1) { revMs=(uint16_t)toL(a1); replyOK(); return; }
  if (!strcmp(cmd, "SETRESUMEDELAY") && a1) { resumeDelayMs=(uint16_t)toL(a1); replyOK(); return; }
  if (!strcmp(cmd, "SETRETRY")     && a1) { long v=toL(a1); if(v<0||v>10){replyErr(F("ARGS"));return;} retryMax=(uint8_t)v; replyOK(); return; }
  if (!strcmp(cmd, "SETSETTLE")    && a1) { settleMs=(uint16_t)toL(a1); replyOK(); return; }

  if (!strcmp(cmd, "SAVECFG")) { saveCfg(); replyOK(); return; }

  if (!strcmp(cmd, "SETMON") && a1) {
    bool on = toL(a1) != 0;
    monEnabled = on;
    if (on) startMonitoring();
    else { stopAll(); setDir(false); enterState(ST_MON_OFF); }
    replyOK(); return;
  }

  if (!strcmp(cmd, "RESET")) {
    retryCount = 0; overSince = 0;
    if (monEnabled) { setDir(false); setRun(true); enterState(ST_SETTLE, settleMs); }
    replyOK(); return;
  }

  if (!strcmp(cmd, "MANUAL") && a1) {
    if (monEnabled) { replyErr(F("MON_ON")); return; }
    char *sub = a1; char *a2 = strtok_r(NULL, " ", &save);
    if (!a2) { replyErr(F("ARGS")); return; }
    bool on = toL(a2) != 0;
    if (!strcmp(sub, "RUN")) { setRun(on); replyOK(); return; }
    if (!strcmp(sub, "DIR")) { setDir(on); replyOK(); return; }
    replyErr(F("ARGS")); return;
  }

  replyErr(F("UNKNOWN"));
}

static void service() {
  while (Serial.available()) {
    char ch = (char)Serial.read();
    if (ch == '\n' || ch == '\r') {
      if (lineLen > 0) { lineBuf[lineLen] = 0; lineLen = 0; handleLine(lineBuf); }
    } else if (lineLen < sizeof(lineBuf) - 1) {
      lineBuf[lineLen++] = ch;
    }
  }
}

// ---------- setup / loop ----------
void setup() {
  Serial.begin(115200);
  Serial.println(F("BOOT"));
  loadCfg();
  pinMode(runPin, OUTPUT); pinMode(dirPin, OUTPUT);
  setRun(false); setDir(false);
  sampWinStart = millis();
  Serial.println(F("READY"));
}

void loop() {
  service();
  ctSampleTick();
  stateMachine();
}
