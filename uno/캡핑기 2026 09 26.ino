// ============================================================
//  UNO 시퀀스 스케치 (UNO_PLC_Control.html 에서 자동 생성 · 순수 아두이노 업로드용)
//  생성: 2026. 9. 26. 오후 5:54:18   시퀀스 4개
//  릴레이 = active-LOW(LOW=ON), 입력 = INPUT_PULLUP(버튼이 GND로 붙으면 "눌림")
//  전원 투입 후 3초간은 입력을 무시합니다. 시리얼 통신은 사용하지 않습니다.
//  OUT_BOOT: 전원 투입 시 각 릴레이의 시작 상태(1=ON으로 시작, 기본 0=OFF로 시작) — 웹페이지 릴레이 박스 체크박스 값
//  OUT_INV : 릴레이별 출력 반전(1 = 이 릴레이만 반대 극성/active-HIGH로 동작) — 웹페이지 "출력 반전(A접점/B접점)" 체크박스 값
//  각 동작 줄은 waitCh=0이면 delayMs만큼 기다린 뒤 다음 줄, waitCh>=1이면 그 채널이 waitOn(1=눌림 0=뗌)이
//  될 때까지 "무한 대기"(타임아웃 없음)한 뒤 다음 줄로 넘어갑니다(delayMs는 이때 무시).
//  SEQ_ORDER_MODE: 1이면 시퀀스가 목록 순서(1→2→…→다시 1)대로만 트리거를 받습니다 — 웹페이지 "목록 순서대로만 실행" 체크박스 값
//  orderExempt: 1이면 SEQ_ORDER_MODE여도 이 시퀀스는 자기 차례를 기다리지 않고 바로 실행 — 웹페이지 "🚨 순서 무시(항상 우선 실행)" 체크박스 값(비상/테스트 스위치용)
//  condMode: 0=추가 조건을 모두 만족해야 동작(AND), 1=추가 조건 중 하나라도 만족하면 동작(OR) — 웹페이지 조건 박스의 AND/OR 선택값
// ============================================================
#define N_OUT 4
#define N_IN  12
#define N_SEQ 4
const bool RELAY_ACTIVE_LOW = true;
const bool SEQ_ORDER_MODE = false;
const uint8_t OUT_PIN[N_OUT] = {3, 4, 5, 6};   // R1~R4
const uint8_t IN_PIN[N_IN]   = {7, 8, 9, 10, 11, 12, 2, 13, 14, 15, 16, 17};   // CH1~CH12
const uint8_t IN_INV[N_IN]   = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};   // 1 = 입력 반전(핀이 HIGH일 때 눌림)
const uint8_t OUT_BOOT[N_OUT] = {0, 0, 1, 0};   // 1 = 전원 투입 시 이 릴레이 ON으로 시작 (기본 0 = OFF로 시작)
const uint8_t OUT_INV[N_OUT]  = {0, 0, 0, 0};   // 1 = 이 릴레이만 반대 극성(active-HIGH)으로 동작 — 웹페이지 "출력 반전" 체크박스 값

#define DEBOUNCE_MS  30UL
#define BOOT_HOLD_MS 3000UL

struct Step { uint8_t relay; uint8_t on; uint16_t delayMs; uint8_t waitCh; uint8_t waitOn; };
struct Seq  {
  uint8_t trigCh;    // 0 = 자동 시작 없음, 1~12 = CH
  uint8_t trigCond;  // 1 = 누를 때, 0 = 놓을 때, 2 = 양쪽, 3 = 누르고 있는 동안에만(떼면 즉시 OFF)
  uint8_t nConds; uint8_t condCh[1]; uint8_t condOn[1];   // 인터록: 트리거 순간 CH가 눌림(1)/뗌(0)이어야 함
  uint8_t condMode; // 0 = 모두 만족(AND), 1 = 하나라도 만족(OR)
  uint8_t orderExempt; // 1 = "순서 무시(항상 우선 실행)" — SEQ_ORDER_MODE여도 자기 차례를 기다리지 않고 바로 실행(비상/테스트 스위치용)
  uint8_t nSteps; Step steps[10];
};

const Seq SEQS[N_SEQ] = {
  // 1. 센서 신호를 받을때  1 타이머 작동 · CH1 — D7 · 센서 신호  · 누를때 + 조건(모두)[CH6(캡핑기 올림 도착 신호) 둘다]
  { 1, 1, 0, {0}, {0}, 0, 0, 8, { {1,1,0,2,1}, {2,1,0,3,1}, {3,1,0,4,1}, {4,1,0,5,1}, {3,0,0,6,1}, {2,0,0,0,0}, {1,0,0,0,0}, {4,0,0,0,0} } },
  // 2. 1번스위치 · CH2 — D8 · 1 타이머 신호 · 누를때 + 조건(모두)[CH6(캡핑기 올림 도착 신호) 눌림] [순서무시]
  { 2, 1, 1, {6}, {1}, 0, 1, 1, { {2,1,0,0,0} } },
  // 3. 2번 스위치 · CH3 — D9 · 용기 누름 센서 신호 · 누를때 + 조건(모두)[CH6(캡핑기 올림 도착 신호) 눌림]
  { 3, 1, 1, {6}, {1}, 0, 0, 1, { {3,1,0,0,0} } },
  // 4. 스위치 3 · CH6 — D12 · 캡핑기 올림 도착 신호 · 누를때
  { 6, 1, 0, {0}, {0}, 0, 0, 2, { {3,0,0,0,0}, {2,0,0,0,0} } }
};

bool inLast[N_IN], inStable[N_IN];
unsigned long inChangedAt[N_IN];
bool sqRun[N_SEQ + 1], sqPrev[N_SEQ + 1], sqCondWait[N_SEQ + 1];
bool sqWaitBase[N_SEQ + 1];   // 대기 시작 시점의 입력 상태 — 이미 그 상태였어도 바로 통과시키지 않고, 실제로 신호가 바뀌어야 통과시키기 위해 기록
uint8_t sqStep[N_SEQ + 1];
unsigned long sqDue[N_SEQ + 1];
bool relayOn[N_OUT];
uint8_t seqOrderIdx = 0;   // SEQ_ORDER_MODE가 true일 때, 지금 트리거가 허용된 시퀀스 번호

void setRelay(uint8_t ch, bool on) {
  relayOn[ch - 1] = on;
  bool activeLow = RELAY_ACTIVE_LOW != (OUT_INV[ch - 1] != 0);   // OUT_INV가 1이면 이 릴레이만 극성을 반대로 적용
  digitalWrite(OUT_PIN[ch - 1], (on == activeLow) ? LOW : HIGH);
}

void updateInputs() {
  unsigned long now = millis();
  for (uint8_t i = 0; i < N_IN; i++) {
    bool raw = ((digitalRead(IN_PIN[i]) == LOW) != (IN_INV[i] != 0));
    if (raw != inLast[i]) { inLast[i] = raw; inChangedAt[i] = now; }
    else if (raw != inStable[i] && (now - inChangedAt[i]) >= DEBOUNCE_MS) inStable[i] = raw;
  }
}

bool interlockOK(const Seq &S) {
  if (S.nConds == 0) return true;
  if (S.condMode == 1) {              // 하나라도 만족(OR)
    for (uint8_t c = 0; c < S.nConds; c++)
      if (inStable[S.condCh[c] - 1] == (S.condOn[c] != 0)) return true;
    return false;
  }
  for (uint8_t c = 0; c < S.nConds; c++)   // 모두 만족(AND)
    if (inStable[S.condCh[c] - 1] != (S.condOn[c] != 0)) return false;
  return true;
}

void startSeq(uint8_t s) {
  if (s >= N_SEQ || sqRun[s] || SEQS[s].nSteps == 0) return;
  sqRun[s] = true; sqStep[s] = 0; sqDue[s] = millis();
}


void setup() {
  for (uint8_t i = 0; i < N_OUT; i++) {
    bool on = (OUT_BOOT[i] != 0);
    relayOn[i] = on;
    bool activeLow = RELAY_ACTIVE_LOW != (OUT_INV[i] != 0);
    digitalWrite(OUT_PIN[i], (on == activeLow) ? LOW : HIGH);   // 먼저 최종 상태(on/off) 레벨을 깔아 깜빡임 방지
    pinMode(OUT_PIN[i], OUTPUT);
  }
  for (uint8_t i = 0; i < N_IN; i++) {
    pinMode(IN_PIN[i], INPUT_PULLUP);
    inLast[i] = inStable[i] = ((digitalRead(IN_PIN[i]) == LOW) != (IN_INV[i] != 0));
    inChangedAt[i] = millis();
  }
  for (uint8_t s = 0; s < N_SEQ + 1; s++) { sqRun[s] = false; sqStep[s] = 0; sqPrev[s] = false; sqCondWait[s] = false; sqWaitBase[s] = false; }
}

void loop() {
  updateInputs();
  unsigned long now = millis();
  bool holding = (now < BOOT_HOLD_MS);
  for (uint8_t s = 0; s < N_SEQ; s++) {
    const Seq &S = SEQS[s];
    if (S.trigCh >= 1) {
      bool cur = inStable[S.trigCh - 1];
      if (holding) sqPrev[s] = cur;
      else if (cur != sqPrev[s]) {
        sqPrev[s] = cur;
        bool fire = cur ? (S.trigCond == 1 || S.trigCond == 2 || S.trigCond == 3) : (S.trigCond == 0 || S.trigCond == 2);
        bool orderOK = !SEQ_ORDER_MODE || (s == seqOrderIdx) || (S.orderExempt != 0);
        if (fire && interlockOK(S) && orderOK) startSeq(s);
        if (!cur && S.trigCond == 3) {   // 누르고 있는 동안에만: 손을 떼면 즉시 중단 + 관련 릴레이 전부 OFF
          sqRun[s] = false; sqCondWait[s] = false;
          for (uint8_t k = 0; k < S.nSteps; k++) setRelay(S.steps[k].relay, false);
        }
      }
    }
    while (sqRun[s]) {
      uint8_t r = sqStep[s];
      if (sqCondWait[s]) {                                 // 이 줄의 릴레이 동작은 이미 실행됨 — 조건만 확인
        const Step &st = S.steps[r];
        bool curW = inStable[st.waitCh - 1];
        if (curW != (st.waitOn != 0) || curW == sqWaitBase[s]) break;   // 아직 조건 안 맞거나, 대기 시작 때와 같은 상태에서 바뀐 적이 없음 → 다음 loop()에서 다시 확인
        sqCondWait[s] = false;
      } else {
        if ((long)(now - sqDue[s]) < 0) break;              // 아직 지연 시간이 안 지남
        setRelay(S.steps[r].relay, S.steps[r].on != 0);
        if (S.steps[r].waitCh != 0) { sqCondWait[s] = true; sqWaitBase[s] = inStable[S.steps[r].waitCh - 1]; break; }   // 이 줄은 "조건 대기"로 전환(시작 시점 상태를 기록)
        sqDue[s] = now + S.steps[r].delayMs;
      }
      sqStep[s] = r + 1;
      if (sqStep[s] >= S.nSteps) {
        sqRun[s] = false;
        if (SEQ_ORDER_MODE && s == seqOrderIdx) seqOrderIdx = (uint8_t)((s + 1 < N_SEQ) ? (s + 1) : 0);
        break;
      }
      if (S.steps[r].waitCh == 0 && S.steps[r].delayMs > 0) break;   // 지연이 0이면 같은 순간에 여러 줄이 연달아 실행됨
    }
  }
}
