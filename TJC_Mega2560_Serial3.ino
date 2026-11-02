// TJC 화면 <-> Mega2560 (Serial3) 시험용 스케치
// 배선: Mega TX3(14) -> 화면 RX / Mega RX3(15) <- 화면 TX / GND 공통
// 주의: Mega는 5V 로직. 화면이 3.3V 로직이면 TX3(14) -> 화면 RX 는 저항 분배(예: 2k+1k)
//
// 화면(USART HMI) 쪽 약속:
//  - page 0 에 2상태 버튼(Dual-state button) 16개: 이름 bt1 ~ bt16, 컴포넌트 ID 1 ~ 16
//  - 각 버튼의 "누름 이벤트"에서 "Send Component ID" 체크

const unsigned long TJC_BAUD = 9600;   // 화면 통신속도와 같아야 함

const uint8_t RELAY_PIN[16] = {54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69}; // A0~A15
const bool RELAY_ACTIVE_LOW = true;    // LOW일 때 켜지는 릴레이보드면 true

bool relayOn[16];
uint8_t rx[16];
uint8_t rxLen = 0;
uint8_t ffCount = 0;

void tjcSend(const char* s) {
  Serial3.print(s);
  Serial3.write(0xFF); Serial3.write(0xFF); Serial3.write(0xFF);
}

void tjcShowRelay(uint8_t i) {
  char b[24];
  snprintf(b, sizeof(b), "bt%u.val=%u", i + 1, relayOn[i] ? 1 : 0);
  tjcSend(b);
}

void setRelay(uint8_t i, bool on) {
  relayOn[i] = on;
  digitalWrite(RELAY_PIN[i], (on != RELAY_ACTIVE_LOW) ? HIGH : LOW);
  tjcShowRelay(i);
}

void handleFrame() {
  // 터치 이벤트: 0x65, 페이지, 컴포넌트ID, 눌림(1)/뗌(0)
  if (rxLen >= 4 && rx[0] == 0x65 && rx[1] == 0 && rx[3] == 1) {
    uint8_t id = rx[2];
    if (id >= 1 && id <= 16) {
      setRelay(id - 1, !relayOn[id - 1]);
      Serial.print(F("터치: bt")); Serial.print(id);
      Serial.println(relayOn[id - 1] ? F(" -> ON") : F(" -> OFF"));
    }
  }
}

void tjcByte(uint8_t b) {
  if (b == 0xFF) {
    if (++ffCount >= 3) { handleFrame(); rxLen = 0; ffCount = 0; }
    return;
  }
  ffCount = 0;
  if (rxLen < sizeof(rx)) rx[rxLen++] = b;
}

void setup() {
  Serial.begin(115200);
  for (uint8_t i = 0; i < 16; i++) {
    relayOn[i] = false;
    digitalWrite(RELAY_PIN[i], RELAY_ACTIVE_LOW ? HIGH : LOW); // 먼저 OFF 레벨
    pinMode(RELAY_PIN[i], OUTPUT);
  }
  Serial3.begin(TJC_BAUD);
  delay(500);                         // 화면 부팅 대기
  tjcSend("page 0");
  for (uint8_t i = 0; i < 16; i++) tjcShowRelay(i);
  Serial.println(F("TJC 시험 시작"));
}

void loop() {
  while (Serial3.available()) tjcByte((uint8_t)Serial3.read());
}
