/* =========================================================
   Mega2560_16릴레이_전용_제어프로그램.html 전용 펌웨어
   (16채널 릴레이 + 입력채널 + 4x4 키패드 + 보드 독립 시퀀스)

   웹 프로그램이 사용하는 명령어(★ 이 펌웨어가 반드시 응답해야 함):
     PIN <2자리채널> ON/OFF        -> 릴레이 출력
     STATUS?                      -> STATUS 1,0,...  (16개)
     SETOUT <2자리채널> <핀>       -> 출력 핀 매핑 저장
     GETCONFIG?                   -> CONFIG OUT 1:54,...,16:69 IN 1:24,...
     SETIN <2자리채널> <핀>        -> 입력 핀 매핑 저장
     INPUT?                       -> INPUT 1,0,...   (입력채널 개수만큼)
     SETKPROW/SETKPCOL <idx> <핀> -> 키패드 행/열 핀 매핑
     SETKPEN <0/1>                -> 키패드 사용/사용 안 함 (사용 안 함이면 스캔을 멈추고
                                       행/열 핀을 놓아줘서 다른 용도로 재사용 가능)
     GETKPCONFIG?                 -> KPCONFIG ROW r0,r1,r2,r3 COL c0,c1,c2,c3 EN 0/1
     SEQCLR / SEQTRIG / SEQSTEP / SEQNAME / SEQCOUNT
                                   -> 보드(EEPROM) 독립 시퀀스 저장
                                      (SEQTRIG의 조건 값: 0=OFF(뗌)일 때, 1=ON(눌림)일 때,
                                       2=BOTH, 누를 때+놓을 때 둘 다 — 상태가 바뀌는 순간마다 실행)
     SEQGET?                      -> SEQ ... (여러 줄) ... SEQGETDONE
     SETLED <문구>                -> OLED(SSD1306, I2C)에 문구 표시 + EEPROM 저장
     GETLED?                      -> LEDTEXT <문구>
     SETLEDTRIG <입력채널> <조건0/1> <초> <문구>
                                   -> 지정한 입력채널이 조건(0=OFF/1=ON)이 되는 순간
                                      지정한 초 동안 <문구>를 대신 표시하고, 시간이
                                      지나면 SETLED로 저장해둔 원래 문구로 자동 복귀.
                                      <입력채널>에 0을 주면 사용 안 함. 보드(EEPROM)에
                                      저장되어 컴퓨터 없이도 스스로 감시/동작함.
     GETLEDTRIG?                  -> LEDTRIG <입력채널> <조건> <초> <문구>
   보드가 스스로 보내는 비동기 라인:
     KEY <문자>                   -> 키패드 눌림 감지 시 언제든 전송

   ※ OLED 문구판(128x64, I2C SSD1306) 사용 시 Arduino IDE 라이브러리 매니저에서
     "U8g2" (olikraus) 라이브러리를 설치해야 컴파일됩니다. 배선은 Mega2560의
     하드웨어 I2C 핀(SDA=20, SCL=21)에 VCC/GND와 함께 연결하면 됩니다.
   =========================================================*/

#include <EEPROM.h>
#include <string.h>
#include <stdio.h>
#include <U8g2lib.h>   // 라이브러리 매니저에서 "U8g2"(olikraus) 설치 필요
#include <Wire.h>

/* ---------------- OLED 문구판(128x64 SSD1306, I2C) ---------------- */
U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled(U8G2_R0, /*reset=*/U8X8_PIN_NONE);
const int LED_TEXT_MAX = 39; // 널문자 포함 40바이트
char ledText[LED_TEXT_MAX+1] = "";

/* ---------------- LED 자동 알림 문구(입력 신호로 몇 초간 표시 후 원래 문구로 복귀) ---------------- */
byte ledTrigCh = 0;              // 0=사용 안 함, 1~inCount=입력채널 번호
byte ledTrigCond = 1;            // 1=ON(눌림), 0=OFF(뗌)일 때 발동
unsigned int ledTrigSec = 3;     // 알림 문구를 표시할 시간(초)
char ledTrigText[LED_TEXT_MAX+1] = ""; // 발동 시 표시할 문구
byte ledTrigLastState = 255;     // 엣지 검출용(255=미초기화)
bool ledTempActive = false;      // 지금 알림 문구를 표시 중인지
unsigned long ledTempEndMs = 0;  // 알림 문구를 원래 문구로 되돌릴 시각(millis())

/* ---------------- 출력 릴레이(R1~R16) ---------------- */
const int TOTAL_RELAYS = 16;
byte outPins[TOTAL_RELAYS];
byte relayState[TOTAL_RELAYS]; // 논리 상태(1=ON/동작, 0=OFF) - STATUS? 응답도 이 논리값 그대로 보냄
bool relayActiveLow = false; // 릴레이 보드 종류(웹페이지 "릴레이 작동방식" 설정과 동기화).
                              // false=active-HIGH(LOW가 꺼짐), true=active-LOW(HIGH가 꺼짐)
                              // 이 값을 몰라서 예전 코드는 전원 켤 때 무조건 LOW로 써서
                              // active-LOW 보드에서는 부팅하자마자 릴레이가 전부 켜지는 문제가 있었음

/* 논리적 ON/OFF(on=true면 "동작 중")를 relayActiveLow 설정에 맞는 실제 핀 레벨(HIGH/LOW)로 변환.
   "PC로 수동 조작"이든 "PC 없이 보드 혼자 시퀀스 실행"이든 릴레이를 켜고 끄는 자리는 반드시
   setRelay() 하나만 거치도록 만들어서, 여기 한 곳만 고치면 모든 경로에 동일하게 적용되게 함
   (예전에는 PC가 보낸 PIN 명령의 ON/OFF 글자를 웹페이지가 미리 뒤집어서 보내는 방식이었는데,
    보드 혼자 시퀀스를 실행할 때는 이 변환을 거치지 않아서 active-LOW 릴레이 보드에서
    ON/OFF가 반대로 나가는 문제가 있었음). */
inline int relayPhysLevel(bool on){
  return (relayActiveLow ? !on : on) ? HIGH : LOW;
}

/* ---------------- 입력 채널 ---------------- */
const int MAX_INPUTS = 32;
byte inPins[MAX_INPUTS];
byte inCount = 0;
byte inStableState[MAX_INPUTS]; // 디바운스 통과한 최종 상태(1/0)
byte inRawLast[MAX_INPUTS];
unsigned long inLastChangeMs[MAX_INPUTS];
const unsigned long DEBOUNCE_MS = 30; // 배선이 길 때 노이즈 오작동 방지용 디바운스

/* ---------------- 4x4 키패드 ---------------- */
byte kpRow[4];
byte kpCol[4];
bool kpEnabled = true; // 키패드를 실제로 배선했는지 여부(꺼두면 스캔을 멈추고 핀을 다른 용도로 재사용 가능)
const char KP_LAYOUT[4][4] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
char kpRawPrev = 0;
unsigned long kpRawChangeMs = 0;
char kpDebounced = 0;
char kpSentKey = 0; // 눌려있는 동안 중복 전송 방지

/* ---------------- 시퀀스(외장 I2C EEPROM(AT24C256)에 저장, 최대 50개 x 8단계) ----------------
   ※ 왜 내장이 아니라 외장인가: 내장 EEPROM(4096바이트)은 이미 다른 설정(릴레이 핀맵·LED 문구 등)과
     공간을 나눠 써야 해서 시퀀스 개수를 많이 늘리기 어려움. 외장 AT24C256(32KB)을 추가로 달면
     시퀀스 전용 공간을 넉넉히 확보할 수 있음. 다만 메가2560 RAM(8KB) 한도 때문에 50개가
     "장시간 켜둬도 먹통 없이 안정적으로 도는" 안전선으로 계산된 값임(계산 근거는 대화 참고). */
const int MAX_SEQ = 50;
const int MAX_STEPS = 8;
struct SeqStep { byte relay; byte state; unsigned int delayMs; };
struct Seq {
  byte trigCh;      // 0=트리거 없음(보드 단독 실행 안 함)
  byte trigCond;    // 1=ON, 0=OFF
  byte numSteps;
  char name[12];
  SeqStep steps[MAX_STEPS];
};
Seq sequences[MAX_SEQ];
byte seqLastInputState[MAX_SEQ]; // 255=미초기화(엣지 검출용)
unsigned long lastSeqRunMs = 0;
const unsigned long SEQ_COOLDOWN_MS = 500; // 릴레이 스위칭 노이즈가 입력에 유입되어 시퀀스가
                                            // 스스로 계속 재실행되는 악순환(자기재생 루프)을 막기 위한
                                            // 쿨다운: 릴레이가 실제로 스위칭될 때마다 이 시간 동안은 새 트리거를 무시함
/* ---- 실행 중인 시퀀스들의 상태(millis() 기반, delay() 없이 한 단계씩 진행) ----
   예전에는 runSequence() 안에서 delay()로 각 단계 사이를 그냥 기다렸는데, 그동안 loop()가
   통째로 멈춰서 키패드·시리얼 명령·다른 채널 입력 감시·LED 자동 알림이 전부 같이 지연되는
   문제가 있었다. 지금은 "지금이 몇 단계이고, 다음 단계를 몇 시(millis)에 실행할지"만 기억해두고,
   매 loop()마다 updateRunningSequences()에서 시간이 됐는지만 확인 후 그때그때 한 단계씩만 진행한다.

   또한 "한 번에 시퀀스 하나만" 실행하던 예전 방식은, 서로 완전히 무관한 다른 버튼(예: 다른 장비)까지
   한 시퀀스가 도는 동안 몽땅 눌러도 씹히는 문제가 있었다. 그래서 여러 시퀀스를 최대 MAX_ACTIVE_SEQ개까지
   "각자" 동시에 진행시키되, 그중 어느 하나라도 같은 릴레이를 동시에 두 시퀀스가 건드리는 것만
   막는다(그건 실제로 위험 — 같은 릴레이에 서로 다른 명령이 겹치면 릴레이가 떨리거나 오동작할 수 있음). */
const int MAX_ACTIVE_SEQ = 4; // 동시에 진행 가능한 시퀀스 개수(서로 다른 릴레이를 쓴다는 전제하에)
struct ActiveSeq { int idx; int step; unsigned long dueMs; }; // idx=-1이면 빈 슬롯
ActiveSeq activeSeqs[MAX_ACTIVE_SEQ];


/* ---------------- 외장 I2C EEPROM(AT24C256) 상태 ----------------
   extEepromOK    : 부팅 시 칩이 정상 응답했는지 (false면 시퀀스 기능 전체를 안전하게 비활성화)
   extBootWarning : 부팅 시 읽은 데이터 중 체크섬이 틀린(손상된) 기록이 있었는지, 또는
                    통신이 간헐적으로 실패했는지 - 있었다면 켜자마자 OLED로 한 번 경고
   extLastWriteOK : 가장 최근 SEQ 저장 명령이 실제로 칩에 정상 기록됐는지(웹 UI가 EXTMEM?로 조회) */
bool extEepromOK = false;
bool extBootWarning = false;
bool extLastWriteOK = true;

/* ---------------- EEPROM 레이아웃 ----------------
   ※ 릴레이 방식(RELAY_MODE)·LED 설정은 항상 고정 주소를 쓰도록 시퀀스 블록보다
     앞에 배치했다. (예전엔 이 값들 주소가 "EE_SEQ_START + MAX_SEQ*SEQ_REC_SIZE"로
     계산되어, 시퀀스 최대 개수(MAX_SEQ)를 바꿀 때마다 같이 밀려서 이동했다.
     그 결과 보드가 이미 저장해둔 값을 새 주소의 빈 공간(0xFF, 미기록)에서 읽어와
     "active-LOW로 저장해뒀는데 active-HIGH로 잘못 인식"하는 등의 오작동이 있었음.
     이제는 MAX_SEQ를 몇으로 바꾸든 이 값들 주소는 절대 움직이지 않는다.) */
#define EE_MAGIC        0
#define EE_MAGIC_VAL    0xA6  // ★ 주소 배치를 바꿨으므로 매직 값도 반드시 함께 올려야 함.
                              // (이전 펌웨어가 이미 0xA5를 써놓은 보드가 있으므로, 값을
                              //  바꾸지 않으면 loadFromEEPROM()이 "이미 초기화됨"으로 오판해서
                              //  loadDefaults()+saveAllToEEPROM()을 건너뛰고, 옛 주소 배치로
                              //  저장된 낡은 바이트를 새 고정 주소에서 그대로 읽어버려 —
                              //  릴레이 방식/LED 설정/시퀀스가 전부 쓰레기값이 되는 문제가 있었음.
                              //  이 값을 바꾸면 이 펌웨어를 처음 올리는 순간 딱 한 번,
                              //  EEPROM 전체가 기본값으로 초기화된다. 이후 SETRMODE/SETLED/
                              //  시퀀스를 웹에서 한 번만 다시 설정해두면, 그 다음부터는
                              //  MAX_SEQ를 몇으로 바꾸든 이 문제가 재발하지 않는다.)
#define EE_OUT_PINS     1    // 16 bytes
#define EE_IN_COUNT     20   // 1 byte
#define EE_IN_PINS      21   // 32 bytes
#define EE_KP_ROW       60   // 4 bytes
#define EE_KP_COL       64   // 4 bytes
#define EE_KP_EN        68   // 1 byte (0=사용 안 함, 그 외=사용) - 미기록 시 0xFF이므로 기본 사용(true)으로 동작
#define EE_RELAY_MODE   70   // 1 byte (0=active-HIGH, 1=active-LOW) — 고정 주소
#define EE_LEDTRIG_CH   71   // 1 byte — 고정 주소
#define EE_LEDTRIG_COND 72   // 1 byte — 고정 주소
#define EE_LEDTRIG_SEC  73   // 2 bytes — 고정 주소
#define EE_LED_TEXT     75   // 40 bytes(문자열+널) — 고정 주소, 끝: 115
#define EE_LEDTRIG_TEXT 115  // 40 bytes(문자열+널) — 고정 주소, 끝: 155
// (시퀀스는 더 이상 내장 EEPROM에 저장하지 않음 — 아래 외장 I2C EEPROM 섹션 참고)

/* ---------------- 외장 I2C EEPROM(AT24C256, 32KB) — 시퀀스 전용 저장소 ----------------
   배선: 칩의 VCC/GND + SDA→메가2560 20번, SCL→메가2560 21번 (OLED와 같은 I2C 버스에 병렬 연결)
   주소 점퍼(A0/A1/A2) 전부 미연결 시 기본 주소 0x50 (OLED SSD1306의 0x3C와 겹치지 않음)

   레코드 구조(슬롯당 64바이트 = AT24C256 한 페이지, 한 번의 I2C 쓰기로 안전하게 기록):
     [0]trigCh [1]trigCond [2]numSteps [3..14]name(12) [15..46]steps(8*4bytes) [47]checksum
     [48..63] 예비(미사용)
   → 페이지 경계를 넘어 쓰는 일이 없어서 "쓰다가 한 페이지가 두 번에 걸쳐 나뉘어 손상되는" 흔한
     I2C EEPROM 버그를 원천적으로 피함. 게다가 매 쓰기마다 재시도(최대 3회)+되읽어 비교(verify),
     매 읽기마다 체크섬 검증까지 하므로, 자동화 기계에서 손상된 시퀀스가 조용히 실행되는 일이 없음
     (체크섬이 안 맞으면 그 시퀀스는 자동으로 "빈 시퀀스" 취급되어 트리거되지 않음). */
#define EXT_EEPROM_ADDR 0x50
#define EXT_SEQ_START   64   // 0~63번지는 예비(향후 매직/버전 등)로 비워둠
#define EXT_SLOT_SIZE   64   // 시퀀스 1개당 슬롯 크기(페이지 정렬)
#define EXT_REC_LEN     48   // 실제로 쓰는 바이트 수(1+1+1+12+32+1=48, 나머지 16바이트는 예비)
// 50개 기준 끝 주소: 64 + 50*64 = 3264 / 32768바이트 사용 (여유 많음 — RAM이 진짜 한계)

bool extI2cWriteRaw(unsigned int addr, const byte* data, byte len){
  Wire.beginTransmission(EXT_EEPROM_ADDR);
  Wire.write((byte)(addr >> 8));
  Wire.write((byte)(addr & 0xFF));
  for(byte i=0;i<len;i++) Wire.write(data[i]);
  if(Wire.endTransmission() != 0) return false; // I2C 오류(배선/전원 문제 등)
  delay(6); // AT24C256 쓰기 사이클 시간(데이터시트 최대 5ms) + 여유
  return true;
}
bool extI2cReadRaw(unsigned int addr, byte* data, byte len){
  Wire.beginTransmission(EXT_EEPROM_ADDR);
  Wire.write((byte)(addr >> 8));
  Wire.write((byte)(addr & 0xFF));
  if(Wire.endTransmission(false) != 0) return false; // repeated start로 버스 유지
  byte n = Wire.requestFrom((int)EXT_EEPROM_ADDR, (int)len);
  if(n != len) return false;
  for(byte i=0;i<len;i++){
    if(!Wire.available()) return false;
    data[i] = Wire.read();
  }
  return true;
}
bool extEepromReadRetry(unsigned int addr, byte* data, byte len){
  for(byte a=0;a<3;a++){ if(extI2cReadRaw(addr,data,len)) return true; delay(5); }
  return false;
}
bool extEepromWriteVerified(unsigned int addr, const byte* data, byte len){
  for(byte a=0;a<3;a++){
    if(extI2cWriteRaw(addr,data,len)){
      byte check[EXT_REC_LEN];
      if(extI2cReadRaw(addr, check, len) && memcmp(check, data, len)==0) return true;
    }
    delay(10);
  }
  return false; // 3회 모두 실패 - 배선/칩 불량 가능성
}
byte calcChecksum(const byte* data, byte len){
  byte sum = 0; for(byte i=0;i<len;i++) sum += data[i]; return sum;
}
bool extEepromDetect(){
  Wire.beginTransmission(EXT_EEPROM_ADDR);
  return (Wire.endTransmission() == 0);
}

/* ================= 기본값 / EEPROM ================= */
void loadDefaults(){
  for(int i=0;i<TOTAL_RELAYS;i++) outPins[i] = 54+i;      // A0~A15
  inCount = 4;
  for(int i=0;i<MAX_INPUTS;i++) inPins[i] = 24+i;         // 24번부터(20,21은 I2C SDA/SCL 회피)
  kpRow[0]=30; kpRow[1]=31; kpRow[2]=32; kpRow[3]=33;
  kpCol[0]=34; kpCol[1]=35; kpCol[2]=36; kpCol[3]=37;
  kpEnabled = true;
  relayActiveLow = false;
  for(int i=0;i<MAX_SEQ;i++){
    sequences[i].numSteps=0; sequences[i].trigCh=0; sequences[i].trigCond=1; sequences[i].name[0]=0;
  }
  ledText[0] = 0; // 문구 없음(빈 화면)
  ledTrigCh = 0; ledTrigCond = 1; ledTrigSec = 3; ledTrigText[0] = 0; // 자동 알림 기본값: 사용 안 함
}

void saveLedTextToEEPROM(){
  for(int i=0;i<=LED_TEXT_MAX;i++){
    EEPROM.update(EE_LED_TEXT+i, ledText[i]);
    if(ledText[i]==0) break;
  }
}

void drawLedTextRaw(const char* txt){
  oled.clearBuffer();
  oled.setFont(u8g2_font_unifont_t_korean1); // 한글/영문/숫자 표시 (Mega2560 AVR은 32KB 초과 폰트 배열 컴파일 불가하여 korean2 대신 korean1 사용)
  int lineH = 18;
  int y = lineH;
  int maxW = 128;
  int start = 0;
  int len = strlen(txt);
  // 글자 폭에 맞춰 자동으로 줄바꿈해서 최대 3줄까지 표시
  while(start < len && y <= lineH*3){
    int end = start;
    int w = 0;
    while(end < len){
      // UTF-8: 한글은 3바이트, 영문/숫자/기호는 1바이트
      int step = ((unsigned char)txt[end] >= 0xE0) ? 3 : 1;
      char tmp[4]={0}; memcpy(tmp, txt+end, step);
      int cw = oled.getUTF8Width(tmp);
      if(w+cw > maxW) break;
      w += cw; end += step;
    }
    if(end==start) end = start+1; // 안전장치(무한루프 방지)
    char line[40]={0};
    int n = end-start; if(n>39) n=39;
    memcpy(line, txt+start, n);
    oled.setCursor(0,y);
    oled.print(line);
    start = end; y += lineH;
  }
  oled.sendBuffer();
}
void drawLedText(){ drawLedTextRaw(ledText); } // 평소(기본) 문구 표시

void saveLedTrigToEEPROM(){
  EEPROM.update(EE_LEDTRIG_CH, ledTrigCh);
  EEPROM.update(EE_LEDTRIG_COND, ledTrigCond);
  EEPROM.update(EE_LEDTRIG_SEC, ledTrigSec & 0xFF);
  EEPROM.update(EE_LEDTRIG_SEC+1, (ledTrigSec>>8) & 0xFF);
  for(int i=0;i<=LED_TEXT_MAX;i++){
    EEPROM.update(EE_LEDTRIG_TEXT+i, ledTrigText[i]);
    if(ledTrigText[i]==0) break;
  }
}

void saveSeqToEEPROM(int idx){
  if(!extEepromOK){ extLastWriteOK = false; return; } // 칩이 없으면 쓰기 시도 자체를 하지 않음(오류 방지)
  byte buf[EXT_REC_LEN];
  buf[0] = sequences[idx].trigCh;
  buf[1] = sequences[idx].trigCond;
  buf[2] = sequences[idx].numSteps;
  for(int c=0;c<12;c++) buf[3+c] = sequences[idx].name[c];
  for(int st=0;st<MAX_STEPS;st++){
    int o = 15+st*4;
    buf[o+0] = sequences[idx].steps[st].relay;
    buf[o+1] = sequences[idx].steps[st].state;
    buf[o+2] = sequences[idx].steps[st].delayMs & 0xFF;
    buf[o+3] = (sequences[idx].steps[st].delayMs>>8) & 0xFF;
  }
  buf[47] = calcChecksum(buf, 47);
  unsigned int addr = EXT_SEQ_START + (unsigned int)idx*EXT_SLOT_SIZE;
  bool ok = extEepromWriteVerified(addr, buf, EXT_REC_LEN);
  extLastWriteOK = ok; // 웹 UI가 EXTMEM?으로 조회해서 실패를 바로 알 수 있게 함
}

void saveAllToEEPROM(){
  EEPROM.update(EE_MAGIC, EE_MAGIC_VAL);
  for(int i=0;i<TOTAL_RELAYS;i++) EEPROM.update(EE_OUT_PINS+i, outPins[i]);
  EEPROM.update(EE_IN_COUNT, inCount);
  for(int i=0;i<MAX_INPUTS;i++) EEPROM.update(EE_IN_PINS+i, inPins[i]);
  for(int i=0;i<4;i++) EEPROM.update(EE_KP_ROW+i, kpRow[i]);
  for(int i=0;i<4;i++) EEPROM.update(EE_KP_COL+i, kpCol[i]);
  EEPROM.update(EE_KP_EN, kpEnabled ? 1 : 0);
  for(int i=0;i<MAX_SEQ;i++) saveSeqToEEPROM(i);
  saveLedTextToEEPROM();
  EEPROM.update(EE_RELAY_MODE, relayActiveLow ? 1 : 0);
  saveLedTrigToEEPROM();
}

void loadFromEEPROM(){
  extEepromOK = extEepromDetect(); // 내장 EEPROM보다 먼저 확인: saveAllToEEPROM()이 곧바로
                                    // saveSeqToEEPROM()을 호출할 수 있으므로 이 값이 먼저 있어야 함
  if(EEPROM.read(EE_MAGIC) != EE_MAGIC_VAL){
    loadDefaults();
    saveAllToEEPROM();
    return;
  }
  for(int i=0;i<TOTAL_RELAYS;i++) outPins[i] = EEPROM.read(EE_OUT_PINS+i);
  inCount = EEPROM.read(EE_IN_COUNT);
  if(inCount > MAX_INPUTS) inCount = MAX_INPUTS;
  for(int i=0;i<MAX_INPUTS;i++) inPins[i] = EEPROM.read(EE_IN_PINS+i);
  for(int i=0;i<4;i++) kpRow[i] = EEPROM.read(EE_KP_ROW+i);
  for(int i=0;i<4;i++) kpCol[i] = EEPROM.read(EE_KP_COL+i);
  kpEnabled = (EEPROM.read(EE_KP_EN) != 0); // 이 값을 저장한 적 없는 이전 펌웨어의 보드는 미기록 값(0이 아님)이라 기본 true로 동작

  /* ---- 시퀀스: 외장 I2C EEPROM에서 읽기 ---- */
  extBootWarning = false;
  for(int s=0;s<MAX_SEQ;s++){
    bool useEmpty = true; // 이 시퀀스 슬롯을 "빈 시퀀스"로 둘지 여부(기본값=안전하게 비움)
    if(extEepromOK){
      byte buf[EXT_REC_LEN];
      unsigned int addr = EXT_SEQ_START + (unsigned int)s*EXT_SLOT_SIZE;
      if(extEepromReadRetry(addr, buf, EXT_REC_LEN)){
        bool blank = true;
        for(byte i=0;i<EXT_REC_LEN;i++){ if(buf[i]!=0xFF){ blank=false; break; } }
        if(!blank){
          if(calcChecksum(buf,47) == buf[47]){
            sequences[s].trigCh   = buf[0];
            sequences[s].trigCond = buf[1];
            sequences[s].numSteps = buf[2];
            if(sequences[s].numSteps > MAX_STEPS) sequences[s].numSteps = 0;
            for(int c=0;c<12;c++) sequences[s].name[c] = buf[3+c];
            sequences[s].name[11] = 0;
            for(int st=0; st<MAX_STEPS; st++){
              int o = 15+st*4;
              sequences[s].steps[st].relay = buf[o+0];
              sequences[s].steps[st].state = buf[o+1];
              sequences[s].steps[st].delayMs = buf[o+2] | ((unsigned int)buf[o+3] << 8);
            }
            useEmpty = false;
          } else {
            extBootWarning = true; // 통신은 됐지만 체크섬이 틀림 = 실제로 손상된 기록
          }
        }
        // blank(전부 0xFF)인 경우는 그냥 "아직 안 쓴 빈 슬롯"이라 경고 없이 조용히 비움 처리
      } else {
        extBootWarning = true; // 3회 재시도해도 통신 자체가 실패 = 배선/전원 문제 가능성
      }
    }
    if(useEmpty){
      sequences[s].numSteps=0; sequences[s].trigCh=0; sequences[s].trigCond=1; sequences[s].name[0]=0;
    }
  }

  // 이 값을 저장한 적 없는 이전 펌웨어의 보드는 미기록 영역(0xFF)일 수 있으므로
  // 유효한 문자열이 아니면 빈 문구로 처리한다.
  bool valid = true;
  for(int i=0;i<=LED_TEXT_MAX;i++){
    byte b = EEPROM.read(EE_LED_TEXT+i);
    ledText[i] = (char)b;
    if(b==0) break;
    if(i==LED_TEXT_MAX){ valid=false; ledText[0]=0; break; } // 널 종료 없이 끝까지 감 = 손상/미기록
  }
  if((unsigned char)ledText[0]==0xFF) ledText[0]=0;
  // 이 값을 저장한 적 없는 이전 펌웨어의 보드는 미기록(0xFF)이므로 기본값(active-HIGH, false)으로 동작
  relayActiveLow = (EEPROM.read(EE_RELAY_MODE) == 1);
  // LED 자동 알림(트리거) 설정 - 이 기능을 저장한 적 없는 이전 펌웨어의 보드는
  // 미기록 영역(0xFF)일 수 있으므로, 그런 경우 "사용 안 함"으로 안전하게 초기화한다.
  ledTrigCh = EEPROM.read(EE_LEDTRIG_CH);
  if(ledTrigCh == 0xFF) ledTrigCh = 0;
  ledTrigCond = EEPROM.read(EE_LEDTRIG_COND);
  if(ledTrigCond != 0 && ledTrigCond != 1) ledTrigCond = 1;
  {
    unsigned int sec = EEPROM.read(EE_LEDTRIG_SEC) | ((unsigned int)EEPROM.read(EE_LEDTRIG_SEC+1) << 8);
    ledTrigSec = (sec == 0xFFFF) ? 3 : sec;
  }
  bool trigTextValid = true;
  for(int i=0;i<=LED_TEXT_MAX;i++){
    byte b = EEPROM.read(EE_LEDTRIG_TEXT+i);
    ledTrigText[i] = (char)b;
    if(b==0) break;
    if(i==LED_TEXT_MAX){ trigTextValid=false; ledTrigText[0]=0; break; }
  }
  if((unsigned char)ledTrigText[0]==0xFF) ledTrigText[0]=0;
}

/* ================= 핀 초기화 ================= */
void applyOutPinModes(){
  for(int i=0;i<TOTAL_RELAYS;i++){
    pinMode(outPins[i], OUTPUT);
    digitalWrite(outPins[i], relayPhysLevel(false)); // 보드 종류에 맞는 안전한 OFF 레벨로 시작
    relayState[i] = 0;
  }
}
void applyInPinModes(){
  for(int i=0;i<inCount;i++){
    pinMode(inPins[i], INPUT_PULLUP); // 배선이 길 경우 외부 10kΩ 풀업 병행 권장
    int raw = digitalRead(inPins[i]) == LOW ? 1 : 0;
    inRawLast[i] = raw; inStableState[i] = raw; inLastChangeMs[i] = millis();
  }
}
void applyKpPinModes(){
  if(!kpEnabled) return; // 사용 안 함이면 핀 모드를 건드리지 않아 다른 용도(릴레이/입력)와 충돌하지 않음
  for(int i=0;i<4;i++){ pinMode(kpCol[i], INPUT_PULLUP); }
  for(int i=0;i<4;i++){ pinMode(kpRow[i], OUTPUT); digitalWrite(kpRow[i], HIGH); }
}
void releaseKpPinModes(){
  // 키패드를 끌 때, 이미 출력으로 잡아뒀던 행 핀을 입력(하이임피던스)으로 되돌려
  // 다른 용도로 재구성하기 전까지 안전한 상태로 만든다.
  for(int i=0;i<4;i++){ pinMode(kpRow[i], INPUT); }
  for(int i=0;i<4;i++){ pinMode(kpCol[i], INPUT); }
}

/* ================= 릴레이 제어 ================= */
void setRelay(int ch, bool on){
  if(ch < 1 || ch > TOTAL_RELAYS) return;
  digitalWrite(outPins[ch-1], relayPhysLevel(on)); // 논리 ON/OFF -> 보드 종류에 맞는 실제 핀 레벨
  relayState[ch-1] = on ? 1 : 0; // 논리 상태로 저장
}

/* ================= 응답 전송 ================= */
void sendStatus(){
  String s = "STATUS ";
  for(int i=0;i<TOTAL_RELAYS;i++){ s += String(relayState[i]); if(i<TOTAL_RELAYS-1) s += ","; }
  Serial.println(s);
}
void sendInputs(){
  String s = "INPUT ";
  for(int i=0;i<inCount;i++){ s += String(inStableState[i]); if(i<inCount-1) s += ","; }
  Serial.println(s);
}
void sendConfig(){
  String s = "CONFIG OUT ";
  for(int i=0;i<TOTAL_RELAYS;i++){ s += String(i+1)+":"+String(outPins[i]); if(i<TOTAL_RELAYS-1) s += ","; }
  s += " IN ";
  for(int i=0;i<inCount;i++){ s += String(i+1)+":"+String(inPins[i]); if(i<inCount-1) s += ","; }
  Serial.println(s);
}
void sendKpConfig(){
  String s = "KPCONFIG ROW ";
  for(int i=0;i<4;i++){ s += String(kpRow[i]); if(i<3) s += ","; }
  s += " COL ";
  for(int i=0;i<4;i++){ s += String(kpCol[i]); if(i<3) s += ","; }
  s += " EN " + String(kpEnabled ? 1 : 0);
  Serial.println(s);
}
void sendSeqGet(){
  for(int i=0;i<MAX_SEQ;i++){
    if(sequences[i].numSteps == 0) continue;
    String s = "SEQ ";
    s += String(i)+" "+String(sequences[i].trigCh)+" "+String(sequences[i].trigCond)+" "+String(sequences[i].numSteps)+" ";
    String nm = String(sequences[i].name);
    if(nm.length()==0) nm = "seq"+String(i+1);
    s += nm + " ";
    for(int st=0; st<sequences[i].numSteps; st++){
      s += String(sequences[i].steps[st].relay)+","+String(sequences[i].steps[st].state)+","+String(sequences[i].steps[st].delayMs);
      if(st < sequences[i].numSteps-1) s += ";";
    }
    Serial.println(s);
  }
  Serial.println("SEQGETDONE");
}

/* ================= 명령 처리 ================= */
void handleCommand(String cmdStr){
  char buf[100];
  cmdStr.toCharArray(buf, sizeof(buf));

  if(strncmp(buf,"PIN ",4)==0){
    int ch; char state[8];
    if(sscanf(buf+4, "%d %7s", &ch, state)==2){ setRelay(ch, strcmp(state,"ON")==0); }
    return;
  }
  if(strcmp(buf,"STATUS?")==0){ sendStatus(); return; }
  if(strcmp(buf,"INPUT?")==0){ sendInputs(); return; }
  if(strncmp(buf,"SETOUT ",7)==0){
    int ch,pin;
    if(sscanf(buf+7,"%d %d",&ch,&pin)==2 && ch>=1 && ch<=TOTAL_RELAYS){
      outPins[ch-1]=pin; pinMode(pin, OUTPUT); digitalWrite(pin, relayPhysLevel(relayState[ch-1]==1));
      EEPROM.update(EE_OUT_PINS+(ch-1), pin);
      Serial.println("OK");
    }
    return;
  }
  if(strncmp(buf,"SETIN ",6)==0){
    int ch,pin;
    if(sscanf(buf+6,"%d %d",&ch,&pin)==2 && ch>=1 && ch<=MAX_INPUTS){
      inPins[ch-1]=pin; if(ch>inCount) inCount=ch;
      pinMode(pin, INPUT_PULLUP);
      int raw = digitalRead(pin)==LOW ? 1 : 0;
      inRawLast[ch-1]=raw; inStableState[ch-1]=raw; inLastChangeMs[ch-1]=millis();
      EEPROM.update(EE_IN_PINS+(ch-1), pin);
      EEPROM.update(EE_IN_COUNT, inCount);
      Serial.println("OK");
    }
    return;
  }
  if(strcmp(buf,"GETCONFIG?")==0){ sendConfig(); return; }
  if(strncmp(buf,"SETKPROW ",9)==0){
    int idx,pin;
    if(sscanf(buf+9,"%d %d",&idx,&pin)==2 && idx>=0 && idx<4){
      kpRow[idx]=pin;
      if(kpEnabled){ pinMode(pin,OUTPUT); digitalWrite(pin,HIGH); }
      EEPROM.update(EE_KP_ROW+idx,pin);
      Serial.println("OK");
    }
    return;
  }
  if(strncmp(buf,"SETKPCOL ",9)==0){
    int idx,pin;
    if(sscanf(buf+9,"%d %d",&idx,&pin)==2 && idx>=0 && idx<4){
      kpCol[idx]=pin;
      if(kpEnabled) pinMode(pin,INPUT_PULLUP);
      EEPROM.update(EE_KP_COL+idx,pin);
      Serial.println("OK");
    }
    return;
  }
  if(strncmp(buf,"SETKPEN ",8)==0){
    int en;
    if(sscanf(buf+8,"%d",&en)==1){
      bool wasEnabled = kpEnabled;
      kpEnabled = (en != 0);
      EEPROM.update(EE_KP_EN, kpEnabled ? 1 : 0);
      if(kpEnabled && !wasEnabled){ applyKpPinModes(); kpRawPrev=0; kpDebounced=0; kpSentKey=0; }
      else if(!kpEnabled && wasEnabled){ releaseKpPinModes(); }
      Serial.println("OK");
    }
    return;
  }
  if(strcmp(buf,"GETKPCONFIG?")==0){ sendKpConfig(); return; }
  if(strncmp(buf,"SETLED ",7)==0){
    strncpy(ledText, buf+7, LED_TEXT_MAX);
    ledText[LED_TEXT_MAX] = 0;
    saveLedTextToEEPROM();
    drawLedText();
    Serial.println("OK");
    return;
  }
  if(strcmp(buf,"GETLED?")==0){
    Serial.print("LEDTEXT ");
    Serial.println(ledText);
    return;
  }
  if(strncmp(buf,"SETLEDTRIG ",11)==0){
    int ch, cond, sec, n=0;
    if(sscanf(buf+11, "%d %d %d%n", &ch, &cond, &sec, &n)==3){
      ledTrigCh = (ch<0 || ch>MAX_INPUTS) ? 0 : ch;
      ledTrigCond = cond ? 1 : 0;
      ledTrigSec = (sec<0) ? 0 : sec;
      const char* textStart = buf+11+n;
      while(*textStart==' ') textStart++;
      strncpy(ledTrigText, textStart, LED_TEXT_MAX);
      ledTrigText[LED_TEXT_MAX] = 0;
      // 방금 설정/재설정한 순간의 실제 입력 상태로 "이전 상태"를 맞춰서,
      // 저장하자마자 우연히 조건과 같은 상태였다는 이유만으로 즉시 오발동하는 것을 방지.
      if(ledTrigCh>=1 && ledTrigCh<=inCount) ledTrigLastState = inStableState[ledTrigCh-1];
      else ledTrigLastState = 255;
      saveLedTrigToEEPROM();
      Serial.println("OK");
    }
    return;
  }
  if(strcmp(buf,"GETLEDTRIG?")==0){
    Serial.print("LEDTRIG ");
    Serial.print(ledTrigCh); Serial.print(" ");
    Serial.print(ledTrigCond); Serial.print(" ");
    Serial.print(ledTrigSec); Serial.print(" ");
    Serial.println(ledTrigText);
    return;
  }
  if(strcmp(buf,"SEQCLR")==0){
    for(int i=0;i<MAX_SEQ;i++){
      sequences[i].numSteps=0; sequences[i].trigCh=0; sequences[i].trigCond=1; sequences[i].name[0]=0;
      saveSeqToEEPROM(i); seqLastInputState[i]=255;
    }
    Serial.println("OK");
    return;
  }
  if(strncmp(buf,"SEQTRIG ",8)==0){
    int idx,trigCh,trigCond;
    if(sscanf(buf+8,"%d %d %d",&idx,&trigCh,&trigCond)==3 && idx>=0 && idx<MAX_SEQ){
      sequences[idx].trigCh=trigCh; sequences[idx].trigCond=trigCond;
      // 트리거를 새로 설정(또는 재설정)하는 이 순간의 실제 입력 상태로 "이전 상태"를 맞춰둔다.
      // (setup()의 부팅 시 처리와 동일한 이유: 무조건 255로 두면, 마침 그 순간 입력이
      //  트리거 조건과 우연히 같을 경우 버튼/센서 동작이 없었는데도 "전체 시퀀스를 보드에
      //  저장" 등으로 SEQTRIG가 실행되자마자 시퀀스가 즉시 오발동하는 문제가 있었음)
      if(trigCh>=1 && trigCh<=inCount) seqLastInputState[idx] = inStableState[trigCh-1];
      else seqLastInputState[idx] = 255;
      saveSeqToEEPROM(idx);
      Serial.println("OK");
    }
    return;
  }
  if(strncmp(buf,"SEQSTEP ",8)==0){
    int idx,si,relay,state,delayMs;
    if(sscanf(buf+8,"%d %d %d %d %d",&idx,&si,&relay,&state,&delayMs)==5 && idx>=0 && idx<MAX_SEQ && si>=0 && si<MAX_STEPS){
      sequences[idx].steps[si].relay=relay;
      sequences[idx].steps[si].state=state;
      sequences[idx].steps[si].delayMs=delayMs;
      saveSeqToEEPROM(idx);
      Serial.println("OK");
    }
    return;
  }
  if(strncmp(buf,"SEQNAME ",8)==0){
    int idx; char nm[16];
    if(sscanf(buf+8,"%d %15s",&idx,nm)==2 && idx>=0 && idx<MAX_SEQ){
      strncpy(sequences[idx].name, nm, 11); sequences[idx].name[11]=0;
      saveSeqToEEPROM(idx);
      Serial.println("OK");
    }
    return;
  }
  if(strncmp(buf,"SEQCOUNT ",9)==0){
    int idx,count;
    if(sscanf(buf+9,"%d %d",&idx,&count)==2 && idx>=0 && idx<MAX_SEQ){
      sequences[idx].numSteps = count>MAX_STEPS?MAX_STEPS:(count<0?0:count);
      saveSeqToEEPROM(idx);
      Serial.println("OK");
    }
    return;
  }
  if(strcmp(buf,"SEQGET?")==0){ sendSeqGet(); return; }
  if(strncmp(buf,"SETRMODE ",9)==0){
    int m;
    if(sscanf(buf+9,"%d",&m)==1){
      relayActiveLow = (m != 0);
      EEPROM.update(EE_RELAY_MODE, relayActiveLow ? 1 : 0);
      // 지금 당장 출력을 되쓰지는 않는다(현재 켜둔 릴레이가 있으면 그대로 유지).
      // 이 설정은 다음 "전원을 켤 때" 안전한 OFF 레벨을 정하는 데만 쓰인다.
      Serial.println("OK");
    }
    return;
  }
  if(strcmp(buf,"GETRMODE?")==0){
    Serial.print("RMODE ");
    Serial.println(relayActiveLow ? 1 : 0);
    return;
  }
  if(strcmp(buf,"EXTMEM?")==0){
    // 웹 화면이 "외장 시퀀스 메모리(AT24C256)" 상태를 확인할 때 씀
    Serial.print("EXTMEM ");
    Serial.print(extEepromOK ? "CONNECTED" : "NOTFOUND"); Serial.print(" ");
    Serial.print(extBootWarning ? "BOOTWARN" : "NOWARN"); Serial.print(" ");
    Serial.println(extLastWriteOK ? "WRITEOK" : "WRITEFAIL");
    return;
  }
}

/* ================= 키패드 스캔(4x4) ================= */
void scanKeypad(){
  if(!kpEnabled) return; // 사용 안 함이면 스캔하지 않음(그 핀들은 다른 용도로 자유롭게 사용 가능)
  char raw = 0;
  for(int r=0;r<4;r++){
    digitalWrite(kpRow[r], LOW);
    delayMicroseconds(50);
    for(int c=0;c<4;c++){
      if(digitalRead(kpCol[c]) == LOW) raw = KP_LAYOUT[r][c];
    }
    digitalWrite(kpRow[r], HIGH);
  }
  unsigned long now = millis();
  if(raw != kpRawPrev){ kpRawPrev = raw; kpRawChangeMs = now; }
  if(now - kpRawChangeMs >= 25){ // 25ms 디바운스
    if(kpDebounced != raw){
      kpDebounced = raw;
      if(kpDebounced != 0 && kpDebounced != kpSentKey){
        Serial.print("KEY ");
        Serial.println(kpDebounced);
        kpSentKey = kpDebounced;
      } else if(kpDebounced == 0){
        kpSentKey = 0;
      }
    }
  }
}

/* ================= 입력 디바운스 + 보드 독립 시퀀스 실행 ================= */
/* 지금 어떤 활성 슬롯이든 relayCh 릴레이를 쓰고 있는 시퀀스가 있는지 확인.
   같은 릴레이를 서로 다른 시퀀스가 동시에 건드리는 것만 막기 위한 용도. */
bool relayBusyInActiveSlots(int relayCh){
  for(int i=0;i<MAX_ACTIVE_SEQ;i++){
    if(activeSeqs[i].idx==-1) continue;
    Seq &sq = sequences[activeSeqs[i].idx];
    for(int s=0;s<sq.numSteps;s++){
      if(sq.steps[s].relay==relayCh) return true;
    }
  }
  return false;
}
/* 시퀀스를 "시작"만 한다 — 실제 단계 진행은 매 loop()마다 updateRunningSequences()가 담당.
   서로 다른 릴레이를 쓰는 시퀀스는 최대 MAX_ACTIVE_SEQ개까지 동시에 진행할 수 있지만,
   지금 도는 시퀀스와 릴레이가 하나라도 겹치면(같은 릴레이를 동시에 두 곳에서 건드리면
   릴레이가 떨리거나 오동작할 수 있어 위험) 이번 트리거는 건너뛴다. */
void startSequence(int idx){
  if(sequences[idx].numSteps <= 0) return;
  for(int i=0;i<MAX_ACTIVE_SEQ;i++) if(activeSeqs[i].idx==idx) return; // 같은 시퀀스가 이미 실행 중
  for(int s=0;s<sequences[idx].numSteps;s++){
    if(relayBusyInActiveSlots(sequences[idx].steps[s].relay)) return; // 릴레이 충돌 → 건너뜀
  }
  for(int i=0;i<MAX_ACTIVE_SEQ;i++){
    if(activeSeqs[i].idx==-1){
      activeSeqs[i].idx=idx; activeSeqs[i].step=0; activeSeqs[i].dueMs=millis(); // 첫 단계는 바로 다음 loop()에서 즉시 실행
      return;
    }
  }
  // 4개 슬롯이 전부 차있으면(매우 드문 경우) 이번 트리거는 그냥 건너뜀
}
/* 매 loop()마다 호출: delay() 없이, 슬롯마다 "지금이 다음 단계를 실행할 시각인지"만 확인해서
   그 슬롯만 한 단계씩 진행한다. 대기 중인 슬롯은 아무 일도 안 하고 바로 지나치므로
   키패드/시리얼/다른 입력 감시가 그 사이에도 정상적으로 계속 돈다. */
void updateRunningSequences(){
  unsigned long now = millis();
  for(int i=0;i<MAX_ACTIVE_SEQ;i++){
    if(activeSeqs[i].idx==-1) continue;
    if((long)(now - activeSeqs[i].dueMs) < 0) continue; // 아직 이 슬롯의 다음 단계 시각이 안 됨
    Seq &sq = sequences[activeSeqs[i].idx];
    if(activeSeqs[i].step >= sq.numSteps){
      // 이 슬롯의 모든 단계(와 마지막 단계의 지연 시간까지) 완료
      activeSeqs[i].idx = -1;
      lastSeqRunMs = now;
      continue;
    }
    SeqStep &st = sq.steps[activeSeqs[i].step];
    setRelay(st.relay, st.state==1);
    activeSeqs[i].dueMs = now + st.delayMs; // delayMs가 0이면 바로 다음 loop()에서 이 슬롯의 다음 단계 진행
    activeSeqs[i].step++;
  }
}

void updateInputsAndSequences(){
  unsigned long now = millis();
  for(int i=0;i<inCount;i++){
    int raw = digitalRead(inPins[i]) == LOW ? 1 : 0;
    if(raw != inRawLast[i]){ inRawLast[i] = raw; inLastChangeMs[i] = now; }
    if(now - inLastChangeMs[i] >= DEBOUNCE_MS){ inStableState[i] = raw; }
  }
  bool coolingDown = (now - lastSeqRunMs < SEQ_COOLDOWN_MS); // 방금 실행 직후 노이즈 안정 대기
  for(int s=0;s<MAX_SEQ;s++){
    if(sequences[s].numSteps==0 || sequences[s].trigCh==0) continue;
    int ch = sequences[s].trigCh;
    if(ch < 1 || ch > inCount) continue;
    int cur = inStableState[ch-1];
    int target = sequences[s].trigCond; // 0=OFF, 1=ON, 2=BOTH(누를 때+놓을 때 둘 다)
    int prev = seqLastInputState[s];
    if(!coolingDown){
      if(target==2){
        // BOTH: 방향 상관없이 상태가 바뀌는 순간마다 실행(부팅 직후 prev==255일 때는 무시)
        if(prev!=255 && cur!=prev) startSequence(s);
      } else if(cur==target && prev!=target){
        startSequence(s);
      }
    }
    seqLastInputState[s] = cur;
  }
}

/* ================= LED 자동 알림(트리거) 감시 ================= */
void updateLedTrigger(){
  unsigned long now = millis();
  // 알림 문구를 표시 중이었다면, 지정된 시간이 지났을 때 원래(SETLED) 문구로 복귀
  if(ledTempActive && (long)(now - ledTempEndMs) >= 0){
    ledTempActive = false;
    drawLedText();
  }
  if(ledTrigCh==0 || ledTrigCh>inCount) return; // 사용 안 함이거나 잘못된 채널
  int cur = inStableState[ledTrigCh-1];
  int target = ledTrigCond;
  // 엣지 검출: 이전엔 조건이 아니었다가 지금 조건이 된 "그 순간"에만 1회 발동
  if(ledTrigLastState!=255 && cur==target && ledTrigLastState!=target){
    drawLedTextRaw(ledTrigText);
    ledTempActive = true;
    ledTempEndMs = now + (unsigned long)ledTrigSec*1000UL;
  }
  ledTrigLastState = cur;
}

/* ================= setup / loop ================= */
void setup(){
  Serial.begin(115200); // 웹 프로그램 Baud rate 115200과 일치
  for(int i=0;i<MAX_ACTIVE_SEQ;i++) activeSeqs[i].idx=-1; // 실행 중 슬롯 전부 비움
  Wire.begin(); // 외장 I2C EEPROM(AT24C256) 통신을 위해 loadFromEEPROM()보다 먼저 초기화해야 함
  loadFromEEPROM();
  applyOutPinModes();
  applyInPinModes();
  applyKpPinModes();
  oled.begin();
  // 컴퓨터 없이도 문제를 바로 알 수 있도록, 외장 메모리에 이상이 있으면 부팅 직후
  // 몇 초간 OLED에 경고를 띄운 뒤 평소 문구로 넘어간다.
  if(!extEepromOK){
    drawLedTextRaw("외장메모리 연결안됨! 시퀀스 사용불가 - 배선 확인");
    delay(4000);
  } else if(extBootWarning){
    drawLedTextRaw("경고: 시퀀스 메모리 오류 감지 - 웹에서 확인 필요");
    delay(4000);
  }
  drawLedText();
  // 전원 켠 순간의 실제 입력 상태로 "이전 상태"를 맞춰둔다.
  // (무조건 255로 두면, 부팅 시점에 입력이 우연히 트리거 조건과 같을 경우
  //  버튼을 누르지 않았는데도 시퀀스가 즉시 실행되어버리는 오작동이 발생했음)
  for(int i=0;i<MAX_SEQ;i++){
    if(sequences[i].trigCh>=1 && sequences[i].trigCh<=inCount){
      seqLastInputState[i] = inStableState[sequences[i].trigCh-1];
    } else {
      seqLastInputState[i] = 255;
    }
  }
  // LED 자동 알림도 시퀀스와 동일한 이유로: 부팅 시점의 실제 입력 상태로
  // "이전 상태"를 맞춰서, 우연히 조건과 같은 상태였다고 즉시 오발동하지 않게 함.
  if(ledTrigCh>=1 && ledTrigCh<=inCount){
    ledTrigLastState = inStableState[ledTrigCh-1];
  } else {
    ledTrigLastState = 255;
  }
}

String rxLine = "";

void loop(){
  // 1) 시리얼 명령 처리(비블로킹, 한 줄씩)
  while(Serial.available() > 0){
    char c = Serial.read();
    if(c == '\n'){
      rxLine.trim();
      if(rxLine.length() > 0) handleCommand(rxLine);
      rxLine = "";
    } else if(c != '\r'){
      rxLine += c;
      if(rxLine.length() > 90) rxLine = ""; // 이상 입력 방지
    }
  }

  // 2) 4x4 키패드 스캔 (눌리면 언제든 "KEY x" 비동기 전송)
  scanKeypad();

  // 3) 입력채널 디바운스 갱신 + 보드 단독(EEPROM) 시퀀스 트리거 검사
  updateInputsAndSequences();

  // 3-1) 진행 중인 시퀀스가 있으면, 다음 단계 실행 시각이 됐는지 확인해서 한 단계씩 진행
  //      (delay() 없이 진행하므로 이 사이에도 위/아래 다른 항목들이 계속 정상 동작함)
  updateRunningSequences();

  // 4) LED 자동 알림 문구 감시(지정한 입력이 조건이 되면 몇 초간 표시 후 원래 문구로 복귀)
  updateLedTrigger();
}
