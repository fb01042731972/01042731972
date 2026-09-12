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
     GETKPCONFIG?                 -> KPCONFIG ROW r0,r1,r2,r3 COL c0,c1,c2,c3
     SEQCLR / SEQTRIG / SEQSTEP / SEQNAME / SEQCOUNT
                                   -> 보드(EEPROM) 독립 시퀀스 저장
     SEQGET?                      -> SEQ ... (여러 줄) ... SEQGETDONE
   보드가 스스로 보내는 비동기 라인:
     KEY <문자>                   -> 키패드 눌림 감지 시 언제든 전송
   =========================================================*/

#include <EEPROM.h>
#include <string.h>
#include <stdio.h>

/* ---------------- 출력 릴레이(R1~R16) ---------------- */
const int TOTAL_RELAYS = 16;
byte outPins[TOTAL_RELAYS];
byte relayState[TOTAL_RELAYS]; // 마지막으로 쓴 값(1=HIGH,0=LOW) - STATUS? 응답용

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

/* ---------------- 시퀀스(보드 독립 저장, 최대 8개 x 8단계) ---------------- */
const int MAX_SEQ = 8;
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
bool seqRunning = false;

/* ---------------- EEPROM 레이아웃 ---------------- */
#define EE_MAGIC       0
#define EE_MAGIC_VAL   0xA5
#define EE_OUT_PINS    1   // 16 bytes
#define EE_IN_COUNT    20  // 1 byte
#define EE_IN_PINS     21  // 32 bytes
#define EE_KP_ROW      60  // 4 bytes
#define EE_KP_COL      64  // 4 bytes
#define EE_SEQ_START   80
#define SEQ_REC_SIZE   47  // trigCh1+trigCond1+numSteps1+name12 + 8*(relay1+state1+delay2)

/* ================= 기본값 / EEPROM ================= */
void loadDefaults(){
  for(int i=0;i<TOTAL_RELAYS;i++) outPins[i] = 54+i;      // A0~A15
  inCount = 4;
  for(int i=0;i<MAX_INPUTS;i++) inPins[i] = 24+i;         // 24번부터(20,21은 I2C SDA/SCL 회피)
  kpRow[0]=30; kpRow[1]=31; kpRow[2]=32; kpRow[3]=33;
  kpCol[0]=34; kpCol[1]=35; kpCol[2]=36; kpCol[3]=37;
  for(int i=0;i<MAX_SEQ;i++){
    sequences[i].numSteps=0; sequences[i].trigCh=0; sequences[i].trigCond=1; sequences[i].name[0]=0;
  }
}

void saveSeqToEEPROM(int idx){
  int base = EE_SEQ_START + idx*SEQ_REC_SIZE;
  EEPROM.update(base+0, sequences[idx].trigCh);
  EEPROM.update(base+1, sequences[idx].trigCond);
  EEPROM.update(base+2, sequences[idx].numSteps);
  for(int c=0;c<11;c++) EEPROM.update(base+3+c, sequences[idx].name[c]);
  for(int st=0;st<MAX_STEPS;st++){
    int sb = base+15+st*4;
    EEPROM.update(sb+0, sequences[idx].steps[st].relay);
    EEPROM.update(sb+1, sequences[idx].steps[st].state);
    EEPROM.update(sb+2, sequences[idx].steps[st].delayMs & 0xFF);
    EEPROM.update(sb+3, (sequences[idx].steps[st].delayMs>>8) & 0xFF);
  }
}

void saveAllToEEPROM(){
  EEPROM.update(EE_MAGIC, EE_MAGIC_VAL);
  for(int i=0;i<TOTAL_RELAYS;i++) EEPROM.update(EE_OUT_PINS+i, outPins[i]);
  EEPROM.update(EE_IN_COUNT, inCount);
  for(int i=0;i<MAX_INPUTS;i++) EEPROM.update(EE_IN_PINS+i, inPins[i]);
  for(int i=0;i<4;i++) EEPROM.update(EE_KP_ROW+i, kpRow[i]);
  for(int i=0;i<4;i++) EEPROM.update(EE_KP_COL+i, kpCol[i]);
  for(int i=0;i<MAX_SEQ;i++) saveSeqToEEPROM(i);
}

void loadFromEEPROM(){
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
  for(int s=0;s<MAX_SEQ;s++){
    int base = EE_SEQ_START + s*SEQ_REC_SIZE;
    sequences[s].trigCh   = EEPROM.read(base+0);
    sequences[s].trigCond = EEPROM.read(base+1);
    sequences[s].numSteps = EEPROM.read(base+2);
    if(sequences[s].numSteps > MAX_STEPS) sequences[s].numSteps = 0;
    for(int c=0;c<11;c++) sequences[s].name[c] = EEPROM.read(base+3+c);
    sequences[s].name[11] = 0;
    for(int st=0; st<MAX_STEPS; st++){
      int sb = base+15+st*4;
      sequences[s].steps[st].relay = EEPROM.read(sb+0);
      sequences[s].steps[st].state = EEPROM.read(sb+1);
      unsigned int d = EEPROM.read(sb+2) | ((unsigned int)EEPROM.read(sb+3) << 8);
      sequences[s].steps[st].delayMs = d;
    }
  }
}

/* ================= 핀 초기화 ================= */
void applyOutPinModes(){
  for(int i=0;i<TOTAL_RELAYS;i++){
    pinMode(outPins[i], OUTPUT);
    digitalWrite(outPins[i], LOW); // 항상 OFF 상태로 안전하게 시작
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
  for(int i=0;i<4;i++){ pinMode(kpCol[i], INPUT_PULLUP); }
  for(int i=0;i<4;i++){ pinMode(kpRow[i], OUTPUT); digitalWrite(kpRow[i], HIGH); }
}

/* ================= 릴레이 제어 ================= */
void setRelay(int ch, bool on){
  if(ch < 1 || ch > TOTAL_RELAYS) return;
  digitalWrite(outPins[ch-1], on ? HIGH : LOW);
  relayState[ch-1] = on ? 1 : 0;
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
      outPins[ch-1]=pin; pinMode(pin, OUTPUT); digitalWrite(pin, relayState[ch-1]?HIGH:LOW);
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
      kpRow[idx]=pin; pinMode(pin,OUTPUT); digitalWrite(pin,HIGH);
      EEPROM.update(EE_KP_ROW+idx,pin);
      Serial.println("OK");
    }
    return;
  }
  if(strncmp(buf,"SETKPCOL ",9)==0){
    int idx,pin;
    if(sscanf(buf+9,"%d %d",&idx,&pin)==2 && idx>=0 && idx<4){
      kpCol[idx]=pin; pinMode(pin,INPUT_PULLUP);
      EEPROM.update(EE_KP_COL+idx,pin);
      Serial.println("OK");
    }
    return;
  }
  if(strcmp(buf,"GETKPCONFIG?")==0){ sendKpConfig(); return; }
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
      sequences[idx].trigCh=trigCh; sequences[idx].trigCond=trigCond; seqLastInputState[idx]=255;
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
}

/* ================= 키패드 스캔(4x4) ================= */
void scanKeypad(){
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
void runSequence(int idx){
  seqRunning = true;
  for(int st=0; st<sequences[idx].numSteps; st++){
    setRelay(sequences[idx].steps[st].relay, sequences[idx].steps[st].state==1);
    if(sequences[idx].steps[st].delayMs > 0) delay(sequences[idx].steps[st].delayMs);
  }
  seqRunning = false;
}

void updateInputsAndSequences(){
  unsigned long now = millis();
  for(int i=0;i<inCount;i++){
    int raw = digitalRead(inPins[i]) == LOW ? 1 : 0;
    if(raw != inRawLast[i]){ inRawLast[i] = raw; inLastChangeMs[i] = now; }
    if(now - inLastChangeMs[i] >= DEBOUNCE_MS){ inStableState[i] = raw; }
  }
  if(seqRunning) return; // 시퀀스 실행 중에는 새 트리거 검사 안 함(중첩 방지)
  for(int s=0;s<MAX_SEQ;s++){
    if(sequences[s].numSteps==0 || sequences[s].trigCh==0) continue;
    int ch = sequences[s].trigCh;
    if(ch < 1 || ch > inCount) continue;
    int cur = inStableState[ch-1];
    int target = sequences[s].trigCond;
    int prev = seqLastInputState[s];
    if(cur==target && prev!=target){ runSequence(s); }
    seqLastInputState[s] = cur;
  }
}

/* ================= setup / loop ================= */
void setup(){
  Serial.begin(115200); // 웹 프로그램 Baud rate 115200과 일치
  loadFromEEPROM();
  applyOutPinModes();
  applyInPinModes();
  applyKpPinModes();
  for(int i=0;i<MAX_SEQ;i++) seqLastInputState[i] = 255;
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
}
