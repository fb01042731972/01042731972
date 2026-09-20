// ============================================================
//  UNO 시퀀스 스케치 (UNO_PLC_Control.html 에서 자동 생성 · 순수 아두이노 업로드용)
//  생성: 2026. 9. 20. 오후 10:08:51   시퀀스 9개
//  릴레이 = active-LOW(LOW=ON), 입력 = INPUT_PULLUP(버튼이 GND로 붙으면 "눌림")
//  전원 투입 후 3초간은 입력을 무시합니다. 시리얼 통신은 사용하지 않습니다.
// ============================================================
#define N_OUT 4
#define N_IN  12
#define N_SEQ 9
const bool RELAY_ACTIVE_LOW = true;
const uint8_t OUT_PIN[N_OUT] = {3, 4, 5, 6};   // R1~R4
const uint8_t IN_PIN[N_IN]   = {7, 8, 9, 10, 11, 12, 2, 13, 14, 15, 16, 17};   // CH1~CH12
const uint8_t IN_INV[N_IN]   = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1};   // 1 = 입력 반전(핀이 HIGH일 때 눌림)

#define DEBOUNCE_MS  30UL
#define BOOT_HOLD_MS 3000UL

struct Step { uint8_t relay; uint8_t on; uint16_t delayMs; };   // 릴레이 동작 후 delayMs 기다린 뒤 다음 줄
struct Seq  {
  uint8_t trigCh;    // 0 = 자동 시작 없음, 1~12 = CH
  uint8_t trigCond;  // 1 = 누를 때, 0 = 놓을 때, 2 = 양쪽
  uint8_t nConds; uint8_t condCh[4]; uint8_t condOn[4];   // 인터록: 트리거 순간 CH가 눌림(1)/뗌(0)이어야 함
  uint8_t nSteps; Step steps[10];
};

const Seq SEQS[N_SEQ] = {
  // 1. 센서 신호를 받을때  1 타이머 작동 · CH1 — D7 · 센서 신호  · 누를때+뗄때 + 조건[CH6(캡핑기 올림 도착 신호) 둘다]
  { 1, 2, 0, {0,0,0,0}, {0,0,0,0}, 1, { {1,1,0} } },
  // 2. 광센서 1 타이머 신호 완료 · CH2 — D8 · 1 타이머 신호 · 누를때+뗄때 + 조건[CH6(캡핑기 올림 도착 신호) 둘다 · CH2(1 타이머 신호) 둘다]
  { 2, 2, 0, {0,0,0,0}, {0,0,0,0}, 1, { {2,1,0} } },
  // 3. 용기 누름 실린더 센서 작동 및 캡핑기 작동 · CH3 — D9 · 용기 누름 센서 신호 · 누를때+뗄때 + 조건[CH6(캡핑기 올림 도착 신호) 둘다 · CH2(1 타이머 신호) 둘다 · CH3(용기 누름 센서 신호) 둘다]
  { 3, 2, 0, {0,0,0,0}, {0,0,0,0}, 1, { {3,1,0} } },
  // 4. 캡핑기 내림센서 작동 신호 받음 · CH4 — D10 · 캡핑기 내림 도착 신호 · 누를때+뗄때 + 조건[CH4(캡핑기 내림 도착 신호) 둘다]
  { 4, 2, 0, {0,0,0,0}, {0,0,0,0}, 1, { {4,1,0} } },
  // 5. 캡핑기 내림 2 타이머 신호 완료 · CH5 — D11 · 2 타이머  신호 · 누를때+뗄때 + 조건[CH2(1 타이머 신호) 둘다 · CH3(용기 누름 센서 신호) 둘다 · CH5(2 타이머  신호) 둘다]
  { 5, 2, 0, {0,0,0,0}, {0,0,0,0}, 1, { {3,0,0} } },
  // 6. 올림 SET · CH6 — D12 · 캡핑기 올림 도착 신호 · 누를때+뗄때 + 조건[CH5(2 타이머  신호) 눌림]
  { 6, 2, 1, {5,0,0,0}, {1,0,0,0}, 4, { {1,0,0}, {2,0,0}, {3,0,0}, {4,0,0} } },
  // 7. 1 번 테스트 스위치 캡핑기 내림 · CH7 — D2 · 1 번 캡핑기 내림  · 누를때+뗄때 + 조건[CH4(캡핑기 내림 도착 신호) 둘다]
  { 7, 2, 0, {0,0,0,0}, {0,0,0,0}, 1, { {3,1,0} } },
  // 8. 2번 용기 잡음 실린더 작동 · CH8 — D13 · 2 번 용기 누름 실린더 · 누를때+뗄때 + 조건[CH4(캡핑기 내림 도착 신호) 둘다]
  { 8, 2, 0, {0,0,0,0}, {0,0,0,0}, 1, { {3,1,0} } },
  // 9. 3번 스위치 복원 (테스트) · CH9 — D14 · 3 번 복귀 · 누를때+뗄때
  { 9, 2, 0, {0,0,0,0}, {0,0,0,0}, 2, { {3,0,0}, {2,0,0} } }
};

bool inLast[N_IN], inStable[N_IN];
unsigned long inChangedAt[N_IN];
bool sqRun[N_SEQ + 1], sqPrev[N_SEQ + 1];
uint8_t sqStep[N_SEQ + 1];
unsigned long sqDue[N_SEQ + 1];
bool relayOn[N_OUT];

void setRelay(uint8_t ch, bool on) {
  relayOn[ch - 1] = on;
  digitalWrite(OUT_PIN[ch - 1], (on == RELAY_ACTIVE_LOW) ? LOW : HIGH);
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
  for (uint8_t c = 0; c < S.nConds; c++)
    if (inStable[S.condCh[c] - 1] != (S.condOn[c] != 0)) return false;
  return true;
}

void startSeq(uint8_t s) {
  if (s >= N_SEQ || sqRun[s] || SEQS[s].nSteps == 0) return;
  sqRun[s] = true; sqStep[s] = 0; sqDue[s] = millis();
}


void setup() {
  for (uint8_t i = 0; i < N_OUT; i++) {
    relayOn[i] = false;
    digitalWrite(OUT_PIN[i], RELAY_ACTIVE_LOW ? HIGH : LOW);   // 먼저 OFF 레벨
    pinMode(OUT_PIN[i], OUTPUT);
  }
  for (uint8_t i = 0; i < N_IN; i++) {
    pinMode(IN_PIN[i], INPUT_PULLUP);
    inLast[i] = inStable[i] = ((digitalRead(IN_PIN[i]) == LOW) != (IN_INV[i] != 0));
    inChangedAt[i] = millis();
  }
  for (uint8_t s = 0; s < N_SEQ + 1; s++) { sqRun[s] = false; sqStep[s] = 0; sqPrev[s] = false; }
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
        bool fire = cur ? (S.trigCond == 1 || S.trigCond == 2) : (S.trigCond == 0 || S.trigCond == 2);
        if (fire && interlockOK(S)) startSeq(s);
      }
    }
    while (sqRun[s] && (long)(now - sqDue[s]) >= 0) {   // 지연이 0이면 같은 순간에 여러 줄이 연달아 실행됨
      uint8_t r = sqStep[s];
      setRelay(S.steps[r].relay, S.steps[r].on != 0);
      sqDue[s] = now + S.steps[r].delayMs;
      sqStep[s] = r + 1;
      if (sqStep[s] >= S.nSteps) { sqRun[s] = false; break; }
      if (S.steps[r].delayMs > 0) break;
    }
  }
}
