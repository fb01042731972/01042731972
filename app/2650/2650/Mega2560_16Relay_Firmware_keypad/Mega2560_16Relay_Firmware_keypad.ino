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
     RAWPIN <핀번호>               -> RAWPIN <핀번호> 0/1  (채널로 등록 안 한 임의 핀 즉석 확인용, INPUT_PULLUP로 읽음)
     RAWSCAN?                     -> RAWSCAN <핀>:0/1,<핀>:0/1,...  (핀 번호 몰라도 어떤 핀이
                                       눌렸는지 자동으로 찾는 "자동 감지" 기능용. 릴레이 출력 핀·
                                       키패드 핀·로드셀 핀·I2C(20,21)는 자동 제외)
     SETKPROW/SETKPCOL <idx> <핀> -> 키패드 행/열 핀 매핑
     SETKPEN <0/1>                -> 키패드 사용/사용 안 함 (사용 안 함이면 스캔을 멈추고
                                       행/열 핀을 놓아줘서 다른 용도로 재사용 가능)
     GETKPCONFIG?                 -> KPCONFIG ROW r0,r1,r2,r3 COL c0,c1,c2,c3 EN 0/1
     SEQCLR / SEQTRIG / SEQSTEP / SEQNAME / SEQCOUNT
                                   -> 보드(외장 I2C EEPROM) 독립 시퀀스 저장
     SEQTRIG <idx> <trigSrc> <trigCh> <trigCond> <trigDistMode> <trigDistCm>
                                   -> trigSrc: 0=입력채널 트리거(trigCh=입력채널 1~inCount),
                                      1=초음파 거리 트리거(trigCh=초음파 채널 1~4, trigDistMode:
                                      0=이하(≤)/1=이상(≥), trigDistCm=목표 거리cm)
                                      trigCond(공통): 0=OFF(조건을 벗어나는 순간), 1=ON(조건에
                                      도달하는 순간), 2=BOTH(양쪽 다 — 상태가 바뀌는 순간마다 실행)
     SEQSTEP <idx> <si> <type> <target> <state> <delayMs> <steps> <speedPps>
                                   -> type: 0=릴레이 동작(target=릴레이1~16, state=0/1 OFF/ON),
                                      1=스텝모터 이동(target=스텝모터1~6, steps=±이동 스텝수(부호=
                                      방향), speedPps=이동 속도). delayMs=이 단계 실행 후 다음
                                      단계까지 대기 시간(스텝모터는 이동을 시작만 하고 바로 다음
                                      단계로 넘어가므로, 이동 완료를 기다리려면 넉넉히 줘야 함)
     SEQGET?                      -> SEQ <idx> <trigSrc> <trigCh> <trigCond> <trigDistMode>
                                      <trigDistCm> <numSteps> <이름> <type,target,state,delayMs,
                                      steps,speedPps>;... (여러 줄) ... SEQGETDONE
                                       (추가 조건이 있는 시퀀스는 그 SEQ 줄 바로 다음에 SEQCOND 줄이 하나 더 나옴)
      SEQCOND <idx> <ON마스크> <OFF마스크>
                                    -> ★추가 조건(인터록). 시작 조건(SEQTRIG)이 일어나는 "그 순간"에
                                       입력채널들이 지정한 상태일 때만 시퀀스를 실행한다.
                                       ON마스크의 1인 비트 = 그 입력채널이 눌려 있어야(ON) 함,
                                       OFF마스크의 1인 비트 = 그 입력채널이 떼어져 있어야(OFF) 함.
                                       bit0=입력CH1 ... bit31=입력CH32, 부호 없는 10진수. 모든 조건은
                                       AND(전부 만족해야 실행). 등록되지 않은 채널(번호>입력채널 개수)이
                                       조건에 있으면 그 시퀀스는 절대 실행되지 않는다(안전 측).
                                       두 마스크가 모두 0이면 조건 없음. SEQCLR이 모든 조건을 지운다.
                                       외장 EEPROM(EXT_COND_START~)에 시퀀스와 별도로 저장되며, 조건 기록이
                                       손상되면 그 시퀀스는 조건 없이 실행되지 않고 통째로 비활성화된다.
      SEQCOND?                     -> SEQCOND OK MAXCH 32  (이 펌웨어가 SEQCOND를 지원하는지 확인용.
                                       아무것도 바꾸지 않음. 웹 페이지가 "보드에 저장" 직전에 사용)
     SETLED <문구>                -> OLED(SSD1306, I2C)에 문구 표시 + EEPROM 저장
     GETLED?                      -> LEDTEXT <문구>
     SETLEDTRIG <입력채널> <조건0/1> <초> <문구>
                                   -> 지정한 입력채널이 조건(0=OFF/1=ON)이 되는 순간
                                      지정한 초 동안 <문구>를 대신 표시하고, 시간이
                                      지나면 SETLED로 저장해둔 원래 문구로 자동 복귀.
                                      <입력채널>에 0을 주면 사용 안 함. 보드(EEPROM)에
                                      저장되어 컴퓨터 없이도 스스로 감시/동작함.
     GETLEDTRIG?                  -> LEDTRIG <입력채널> <조건> <초> <문구>
     SETSTEPPIN <채널1~6> <STEP핀> <DIR핀> <EN핀,0=미사용>
                                   -> 스텝모터 채널 핀 매핑 저장(EEPROM)
     GETSTEPCONFIG?                -> STEPCONFIG 1:step:dir:en,2:...,...
     STEPEN <채널> <0/1>           -> 드라이버 수동 활성/비활성(EN핀 있을 때만, active-LOW 가정)
     STEPMOVE <채널> <±스텝수> <속도pps>
                                   -> 상대 이동 시작(부호=방향). 이동 시작 시 EN핀이 있으면 자동 활성화됨.
                                      delay() 없이 진행되므로 여러 채널을 동시에 움직여도 서로,
                                      그리고 릴레이/시퀀스/키패드를 막지 않음.
     STEPSTOP <채널> / STEPSTOPALL -> 즉시 정지(가감속 없음)
     STEPZERO <채널>               -> 위치 카운터를 0으로 재설정(원점 표시용, 실제 이동 없음)
     STEPSTATUS?                   -> STEPSTATUS 1:이동중0/1:남은스텝:현재위치,2:...,...
     SETULTRAPIN <채널1~4> <TRIG핀> <ECHO핀>
                                   -> 초음파(HC-SR04류) 채널 핀 매핑 저장(EEPROM).
                                      TRIG==ECHO로 같은 핀 번호를 주면 SIG 1핀형(TRIG/ECHO 통합)
                                      모듈로 동작(핀 1개만 사용, pinMode를 순간적으로 바꿔가며 측정)
     GETULTRACONFIG?               -> ULTRACONFIG 1:trig:echo,2:...,...
     ULTRA?                        -> ULTRA 1:cm,2:cm,...  (설정된 채널만 그 순간 측정해서 응답.
                                      측정 자체가 최대 수십ms 걸리므로(초음파 원리상 불가피) 이 명령을
                                      보내는 동안만 짧게 멈춤. 에코 없음/미설정은 -1)
     ULTRAMM <채널1~4>             -> ULTRAMM <채널>:mm  (단일 채널을 mm 단위(반올림)로 측정.
                                      스텝모터 "자동 보정 이동" 기능이 이 명령으로 이동 전/후 거리차를
                                      재서 오차를 계산함. ULTRA?의 cm값(버림)보다 정밀하지만, 센서 자체의
                                      실측 정밀도는 보통 ±2~3mm 수준이라 1mm 미만 보정은 권장하지 않음)
   SETLOADPIN <채널1~4> <DOUT핀> <SCK핀>
                                   -> 로드셀(HX711 앰프) 채널 핀 매핑 저장(EEPROM). 라이브러리 없이
                                      HX711 프로토콜을 직접 비트뱅잉으로 읽으므로 별도 라이브러리 설치 불필요.
   GETLOADCONFIG?                -> LOADCONFIG 1:dout:sck,2:...,...
   LOADTARE <채널>                -> 지금 얹혀있는 무게를 0(제로)으로 맞춤(평균 여러 샘플 측정 후
                                      원시값을 offset으로 저장, EEPROM 보관). 저울 위를 비운 뒤 호출할 것.
   LOADCAL <채널> <기준무게g>      -> LOADTARE로 0을 맞춘 뒤, 알고 있는 무게(기준무게, 그램)를 얹은
                                      상태에서 호출하면 그 시점 원시값과 기준무게로 스케일(그램당
                                      원시값)을 계산해 EEPROM에 저장(2점 보정: 0점+기준점)
   GETLOADCAL?                   -> LOADCAL 1:offset:scale,2:...,...  (scale은 소수, 그램당 원시값 카운트)
   LOAD?                         -> LOAD 1:그램,2:...,...  (미설정/보정 전/측정실패는 -9999)
   ※ 시퀀스의 SEQTRIG에서 trigSrc=2를 주면 로드셀 무게를 초음파 거리와 동일한 방식(trigDistMode:
     0=이하(≤)/1=이상(≥), trigDistCm 필드를 그램 목표값으로 재사용)으로 릴레이 자동 트리거에 쓸 수 있음
     (보드 혼자서도, 즉 컴퓨터 없이도 동작). HX711은 응답 대기 시간이 있어(보통 최대 100ms) 초음파와
     동일하게 트리거로 쓰는 채널만 주기적으로(약 0.5초 간격) 백그라운드에서 재서 캐시해둔다.
   보드가 스스로 보내는 비동기 라인:
     KEY <문자>                   -> 키패드 눌림 감지 시 언제든 전송

   ※ OLED 문구판(128x64, I2C SSD1306) 사용 시 Arduino IDE 라이브러리 매니저에서
     "U8g2" (olikraus) 라이브러리를 설치해야 컴파일됩니다. 배선은 Mega2560의
     하드웨어 I2C 핀(SDA=20, SCL=21)에 VCC/GND와 함께 연결하면 됩니다.
   ※ 블루투스(무선) 연결: USB(Serial) 외에 Serial1(핀 18=TX1/19=RX1)에도 똑같은 명령을
     받고 응답한다. HC-05/HC-06류 클래식 블루투스 모듈을 여기에 연결하고 PC와 페어링하면
     생기는 가상 COM 포트를 웹 프로그램에서 그대로 선택하면 되며(통신 속도는 모듈 기본값인
     9600으로), USB를 뽑아도(외부 전원만 있으면) 무선으로 계속 조작할 수 있다. 자세한 배선/
     설정은 아래 Serial1.begin() 옆 주석 참고.
   =========================================================*/

#include <EEPROM.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>   // strtol/strtoul (SEQCOND 명령 파싱)
#include <stdint.h>   // uint32_t (추가 조건 비트마스크)
#include <limits.h>   // LONG_MIN (로드셀 원시값 읽기 실패 표시용)
#include <math.h>     // lround, isnan (로드셀 그램 환산·EEPROM 미기록 float 판별용)
#include <U8g2lib.h>   // 라이브러리 매니저에서 "U8g2"(olikraus) 설치 필요
#include <Wire.h>

/* ---------------- 통신 포트: USB(Serial, 하드웨어 핀 0/1) + 블루투스(Serial1, 하드웨어 핀 19=RX1/18=TX1) ----------------
   메가2560은 하드웨어 UART가 4개(Serial/Serial1/Serial2/Serial3)라서 소프트웨어시리얼 없이도
   USB와 블루투스를 동시에 쓸 수 있다. 배선: HC-05/HC-06류 클래식 블루투스 모듈의
   TXD → 메가2560 19(RX1), RXD → 메가2560 18(TX1)(모듈 RXD가 5V 내성이 아니면 저항 분배기로
   3.3V로 낮춰서 연결), VCC/GND 연결. 모듈을 PC의 블루투스 설정에서 페어링하면 가상 COM 포트로
   잡히므로, 웹 프로그램에서는 그 COM 포트를 선택하기만 하면 되고 코드 수정은 필요 없다
   (Web Serial API가 USB든 블루투스(SPP) COM 포트든 구분하지 않고 동일하게 다룸).
   HC-05/HC-06 공장 기본 보드레이트는 보통 9600이라 아래 Serial1.begin()도 9600으로 맞췄다 —
   AT 명령으로 모듈 보드레이트를 바꿨다면 이 숫자도 함께 바꿔야 한다(웹 페이지의 "통신 속도"
   드롭다운에서도 그 값을 선택해서 연결해야 함).
   USB와 블루투스 중 어느 쪽으로 명령이 들어왔든, 그 명령에 대한 응답은 항상 같은 포트로
   돌아가야 하므로(안 그러면 엉뚱한 쪽이 응답을 받아가 버림), 지금 처리 중인 명령이 들어온
   포트를 replyPort가 가리키게 해서 handleCommand()와 그 안의 모든 응답 함수가 이 포인터로
   응답을 보내게 했다. 다만 KEY(키패드 눌림) 같은 "보드가 스스로 보내는" 비동기 알림은 어느
   쪽이 요청한 게 아니므로 두 포트 모두에 똑같이 내보낸다(broadcastLine 참고). */
Stream* replyPort = &Serial;
void broadcastLine(const String &s){ Serial.println(s); Serial1.println(s); }

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

/* ---------------- 스텝모터(STEP/DIR(+선택 EN) 드라이버, A4988/DRV8825/TMC2208·2209류) ----------------
   메가2560의 남는 핀 예산 안에서 채널 수를 정한다(릴레이16 + 입력채널 + 키패드8이 대부분을 이미 씀).
   채널당 최소 2핀(STEP+DIR), EN핀은 채널별 선택. MAX_STEPPERS=6은 여유 있게 잡은 상한이며,
   실제로 몇 개까지 쓸 수 있는지는 입력채널을 몇 개나 배선했는지에 달려 있음(웹 화면이 핀 충돌을 알려줌).
   ※ 이 보드는 릴레이/키패드/시퀀스와 같은 loop() 안에서 소프트웨어로 STEP 펄스를 만들기 때문에,
     정밀 CNC 가공처럼 아주 빠르고 일정한 펄스가 필요한 용도에는 맞지 않는다(그런 용도는 별도의
     Mach3 방식 CNC 제어판을 쓸 것). 여기서는 이송/인덱싱처럼 "몇 스텝만큼 이동" 수준의 단순
     위치 제어를 목표로 하며, 키패드 스캔 등 다른 작업과 겹치면 실제 속도가 설정값보다 낮아질 수 있다. */
const int MAX_STEPPERS = 6;
struct StepperCh { byte stepPin, dirPin, enPin; }; // 0=미설정
StepperCh steppers[MAX_STEPPERS];
struct StepperMotion {
  bool active;          // 지금 이동 중인지
  bool dirPositive;     // 마지막 이동 방향(+/-)
  long stepsRemaining;  // 남은 스텝 수
  unsigned long pulseIntervalUs; // 펄스 사이 간격(마이크로초) = 1,000,000 / 속도(pps)
  unsigned long nextPulseUs;     // 다음 펄스를 낼 micros() 시각
  long position;        // 원점(STEPZERO) 대비 상대 위치(스텝 단위, 전원 재시작 시 0으로 초기화)
};
StepperMotion stepMotion[MAX_STEPPERS];

/* ---------------- 초음파 거리센서(HC-SR04류, TRIG+ECHO 2핀 또는 SIG 1핀형) ----------------
   릴레이/키패드/스텝모터가 대부분의 핀을 이미 쓰고 있어 MAX_ULTRA=4로 여유 있게만 잡았고,
   실제로 몇 개까지 쓸 수 있는지는 남은 핀 수에 달려있다(웹 화면이 핀 충돌을 알려줌).
   측정(pulseIn)은 원리상 최대 수십ms가 걸려 delay() 없이 만들 수 없으므로, 다른 기능처럼
   loop()에서 상시 배경 측정을 하지 않고 ULTRA? 명령을 받은 그 순간에만 설정된 채널을
   순서대로 측정해서 응답한다(측정하는 짧은 순간만 멈추고, 평소엔 다른 동작을 막지 않음). */
const int MAX_ULTRA = 4;
struct UltraCh { byte trigPin, echoPin; }; // 0=미설정. trigPin==echoPin(둘 다 0이 아님)이면 SIG 1핀형
UltraCh ultras[MAX_ULTRA];

/* ---------------- 로드셀(HX711 앰프, DOUT+SCK 2핀) ----------------
   초음파와 같은 이유로 MAX_LOAD=4로 여유 있게만 잡았고, 실제로 몇 개까지 쓸 수 있는지는
   남은 핀 수에 달려있다(웹 화면이 핀 충돌을 알려줌). HX711은 라이브러리 없이 DOUT/SCK를
   직접 비트뱅잉해서 읽는다(24bit + 25번째 펄스로 gain=128/채널A 고정). 응답 준비까지
   기다리는 시간이 있어(모듈 RATE 설정에 따라 보통 최대 ~100ms) 초음파의 pulseIn처럼
   "명령 받았을 때만 짧게 멈추고 재는" 방식이며, 시퀀스 트리거로 쓰는 채널만 별도로
   주기적 백그라운드 캐시(sampleLoadForSequenceTriggers)로 재둔다. */
const int MAX_LOAD = 4;
struct LoadCh { byte doutPin, sckPin; long offset; float scale; }; // scale=그램당 원시값 카운트(0이면 미보정)
LoadCh loads[MAX_LOAD];

/* ---------------- 시퀀스(외장 I2C EEPROM(AT24C256)에 저장, 최대 14개 x 12단계) ----------------
   ※ 왜 내장이 아니라 외장인가: 내장 EEPROM(4096바이트)은 이미 다른 설정(릴레이 핀맵·LED 문구 등)과
     공간을 나눠 써야 해서 시퀀스 개수를 많이 늘리기 어려움. 외장 AT24C256(32KB)을 추가로 달면
     시퀀스 전용 공간을 넉넉히 확보할 수 있음. 다만 메가2560 RAM(8KB) 한도 때문에 시퀀스 개수 x
     단계 수를 마음대로 늘릴 수는 없음("장시간 켜둬도 먹통 없이 안정적으로 도는" 안전선 계산 근거는
     대화 참고).
   ※ MAX_SEQ x MAX_STEPS를 바꿀 때 지켜야 할 것:
     1) sequences[] 배열이 SRAM(8KB)에 통째로 올라가므로, 시퀀스 1개당 RAM 사용량은 대략
        19 + 9*MAX_STEPS 바이트. 이 값 x MAX_SEQ가 너무 커지면(예전 50개x8단계=SRAM 95%
        사용) 다른 기능(OLED·시리얼 버퍼 등)과 부딪혀 보드가 먹통이 될 수 있음.
     2) EXT_REC_LEN(아래)이 EXT_SLOT_SIZE(128바이트)를 넘으면 안 됨 — 넘으면 슬롯 크기 자체를
        늘려야 하고, 그러면 외장 EEPROM 주소 배치가 바뀌어 기존에 저장해둔 시퀀스가 깨짐.
        MAX_STEPS=12가 슬롯 128바이트에 예비 공간 없이 정확히 맞아떨어지는 상한값. */
const int MAX_SEQ = 14; // 20 -> 14로 축소하는 대신 MAX_STEPS를 8 -> 12로 늘림(RAM 사용량은 오히려
                         // 기존 20x8(1820B)보다 약간 적은 14x12(1778B) 수준으로 유지).
                         // sequences[] 배열 크기가 늘어나면 SRAM을 더 쓰게 되므로, 꼭 필요한 만큼만 늘리세요.
const int MAX_STEPS = 12;
/* type: 0=릴레이 동작(target=릴레이 채널 1~16, state=0/1 OFF/ON, steps/speedPps는 안 씀),
         1=스텝모터 이동(target=스텝모터 채널 1~6, steps=이동할 스텝 수(부호=방향, 양수=정방향/
         음수=역방향), speedPps=이동 속도(step/초), state는 안 씀).
   delayMs는 두 타입 공통: 이 단계를 "시작"한 뒤 다음 단계로 넘어가기 전에 기다리는 시간
   (스텝모터는 이동을 "시작"만 시키고 곧장 다음 단계로 넘어가므로, 이동이 끝날 때까지
   기다렸다가 다음 단계를 실행하려면 delayMs를 예상 이동 시간(스텝수÷속도×1000ms)보다
   넉넉하게 잡아야 한다). */
struct SeqStep { byte type; byte target; byte state; unsigned int delayMs; int steps; unsigned int speedPps; };
struct Seq {
  byte trigSrc;        // 0=입력채널 트리거, 1=초음파 거리 트리거, 2=로드셀 무게 트리거
  byte trigCh;         // trigSrc=0: 입력채널 번호(1~inCount, 0=트리거 없음)
                        // trigSrc=1: 초음파 채널 번호(1~MAX_ULTRA, 0=트리거 없음)
                        // trigSrc=2: 로드셀 채널 번호(1~MAX_LOAD, 0=트리거 없음)
  byte trigCond;       // 0=OFF(조건을 벗어나는 순간), 1=ON(조건에 도달하는 순간), 2=BOTH(양쪽 다)
  byte trigDistMode;   // trigSrc=1,2 전용: 0=이하(≤), 1=이상(≥) — trigSrc=0이면 의미 없음
  unsigned int trigDistCm; // trigSrc=1 전용: 목표 거리(cm) / trigSrc=2 전용: 목표 무게(g) — 필드 재사용, trigSrc=0이면 의미 없음
  byte numSteps;
  char name[12];
  SeqStep steps[MAX_STEPS];
  uint32_t condOn;     // 추가 조건(인터록): 이 비트의 입력채널이 "눌림(ON)"이어야 함(bit0=CH1). 0이면 조건 없음
  uint32_t condOff;    // 추가 조건(인터록): 이 비트의 입력채널이 "뗌(OFF)"이어야 함
};
Seq sequences[MAX_SEQ];
byte seqLastInputState[MAX_SEQ]; // 255=미초기화(엣지 검출용) — trigSrc=0(입력채널)이든 1(초음파거리
                                  // 조건 충족여부를 0/1로 취급)이든 공통으로 재사용
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
uint32_t condSaveFailMask = 0; // 비트 i=1이면 i번 시퀀스의 추가 조건 기록이 외장 EEPROM에 실패한 상태
                                // (조건 기록 실패는 인터록이 사라질 수 있는 치명적 오류라, 이후 다른 저장이
                                //  성공해도 EXTMEM?가 계속 WRITEFAIL을 보고하도록 따로 기억해둔다)

/* ---------------- EEPROM 레이아웃 ----------------
   ※ 릴레이 방식(RELAY_MODE)·LED 설정은 항상 고정 주소를 쓰도록 시퀀스 블록보다
     앞에 배치했다. (예전엔 이 값들 주소가 "EE_SEQ_START + MAX_SEQ*SEQ_REC_SIZE"로
     계산되어, 시퀀스 최대 개수(MAX_SEQ)를 바꿀 때마다 같이 밀려서 이동했다.
     그 결과 보드가 이미 저장해둔 값을 새 주소의 빈 공간(0xFF, 미기록)에서 읽어와
     "active-LOW로 저장해뒀는데 active-HIGH로 잘못 인식"하는 등의 오작동이 있었음.
     이제는 MAX_SEQ를 몇으로 바꾸든 이 값들 주소는 절대 움직이지 않는다.) */
#define EE_MAGIC        0
#define EE_MAGIC_VAL    0xA9  // ★ 주소 배치를 바꿨으므로 매직 값도 반드시 함께 올려야 함.
                              //  0xA8→0xA9: 로드셀(HX711) 핀·보정값 저장 공간(EE_LOAD_*) 추가.
                              // (이전 펌웨어가 이미 0xA5/0xA6/0xA7을 써놓은 보드가 있으므로, 값을
                              //  바꾸지 않으면 loadFromEEPROM()이 "이미 초기화됨"으로 오판해서
                              //  loadDefaults()+saveAllToEEPROM()을 건너뛰고, 옛 주소 배치로
                              //  저장된 낡은 바이트를 새 고정 주소에서 그대로 읽어버려 —
                              //  릴레이 방식/LED 설정/시퀀스가 전부 쓰레기값이 되는 문제가 있었음.
                              //  이 값을 바꾸면 이 펌웨어를 처음 올리는 순간 딱 한 번,
                              //  EEPROM 전체가 기본값으로 초기화된다. 이후 SETRMODE/SETLED/
                              //  시퀀스를 웹에서 한 번만 다시 설정해두면, 그 다음부터는
                              //  MAX_SEQ를 몇으로 바꾸든 이 문제가 재발하지 않는다.
                              //  0xA6→0xA7: 스텝모터 핀 저장 공간(EE_STEP_PINS) 추가.
                              //  0xA7→0xA8: 초음파 센서 핀 저장 공간(EE_ULTRA_PINS) 추가.)
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
#define EE_STEP_PINS    155  // 6채널 x 3바이트(step,dir,en) = 18 bytes — 고정 주소, 끝: 173
#define EE_ULTRA_PINS   173  // 4채널 x 2바이트(trig,echo) = 8 bytes — 고정 주소, 끝: 181
#define EE_LOAD_PINS    181  // 4채널 x 2바이트(dout,sck) = 8 bytes — 고정 주소, 끝: 189
#define EE_LOAD_OFFSET  189  // 4채널 x 4바이트(long) = 16 bytes — 고정 주소, 끝: 205
#define EE_LOAD_SCALE   205  // 4채널 x 4바이트(float) = 16 bytes — 고정 주소, 끝: 221
// (시퀀스는 더 이상 내장 EEPROM에 저장하지 않음 — 아래 외장 I2C EEPROM 섹션 참고)

/* ---------------- 외장 I2C EEPROM(AT24C256, 32KB) — 시퀀스 전용 저장소 ----------------
   배선: 칩의 VCC/GND + SDA→메가2560 20번, SCL→메가2560 21번 (OLED와 같은 I2C 버스에 병렬 연결)
   주소 점퍼(A0/A1/A2) 전부 미연결 시 기본 주소 0x50 (OLED SSD1306의 0x3C와 겹치지 않음)

   레코드 구조(슬롯당 128바이트 = AT24C256 페이지(64바이트) 2개분):
     [0]trigSrc [1]trigCh [2]trigCond [3]trigDistMode [4..5]trigDistCm [6]numSteps [7..18]name(12)
     [19..126]steps(12개 x 9바이트: type,target,state,delayMs(2),steps(2),speedPps(2)) [127]checksum
     (예비 공간 없음 — 12단계가 128바이트 슬롯에 정확히 맞는 최대치)
   → 레코드가 페이지 2개에 걸치므로, I2C 쓰기/읽기는 항상 16바이트씩(64의 약수라 절대 페이지
     경계를 안 넘음) 잘라서 여러 번 나눠 처리한다(extI2cWriteRaw/extI2cReadRaw 참고) — AVR Wire
     라이브러리의 내부 버퍼 한도(보통 32바이트, 주소 2바이트 포함)도 함께 피해가는 크기다.
     매 쓰기마다 재시도(최대 3회)+되읽어 비교(verify), 매 읽기마다 체크섬 검증까지 하므로,
     자동화 기계에서 손상된 시퀀스가 조용히 실행되는 일이 없음(체크섬이 안 맞으면 그 시퀀스는
     자동으로 "빈 시퀀스" 취급되어 트리거되지 않음). */
#define EXT_EEPROM_ADDR 0x50
#define EXT_SEQ_START   64   // 0~63번지는 예비(향후 매직/버전 등)로 비워둠
#define EXT_SLOT_SIZE   128  // 시퀀스 1개당 슬롯 크기(64바이트 페이지 2개, 항상 페이지 정렬 시작)
#define EXT_REC_LEN     128  // 실제로 쓰는 바이트 수(1+1+1+1+2+1+12 + 12*9 + 체크섬1 = 128, 예비 공간 없음
                              // — MAX_STEPS를 12보다 더 늘리려면 EXT_SLOT_SIZE도 함께 늘려야 함(그러면
                              // 기존 저장 데이터의 주소 배치가 바뀌어 호환 안 됨))
#define EXT_COND_START  4096 // 추가 조건(인터록) 기록 영역 시작(시퀀스 영역 뒤, 64바이트 페이지 정렬).
#define EXT_COND_SLOT   16   // 시퀀스 1개당 조건 기록 슬롯 크기(항상 16바이트 단위 → 페이지 경계 안전)
                              // 레코드: [0]=0xC5(유효 표시) [1..4]condOn(LE) [5..8]condOff(LE) [9..14]예비(0) [15]체크섬(0~14)
                              // 시퀀스 본체 슬롯(EXT_SEQ_START~)을 전혀 건드리지 않고 따로 저장하므로, 이 기능을 추가하기
                              // 전에 저장해둔 시퀀스는 그대로 유효하다(조건 영역이 비어 있으면 "조건 없음").
#define EXT_COND_MAGIC  0xC5
static_assert(EXT_SEQ_START + MAX_SEQ*128 <= EXT_COND_START, "MAX_SEQ가 너무 커서 시퀀스 영역이 조건 영역(EXT_COND_START)을 침범합니다");
static_assert(EXT_COND_START + MAX_SEQ*EXT_COND_SLOT <= 32768, "조건 영역이 AT24C256(32KB)을 넘습니다");
#define EXT_I2C_CHUNK   16   // 한 번의 I2C 트랜잭션으로 보내는 데이터 바이트 수(64의 약수 → 페이지 경계 안전,
                              // 주소 2바이트를 더해도 AVR Wire 기본 버퍼(약 32바이트) 안에 넉넉히 들어감)
// 14개 기준 끝 주소: 64 + 14*128 = 1856 / 32768바이트 사용 (여유 많음 — RAM이 진짜 한계)

bool extI2cWriteRaw(unsigned int addr, const byte* data, byte len){
  unsigned int off = 0;
  while(off < len){
    byte n = (byte)((len - off > EXT_I2C_CHUNK) ? EXT_I2C_CHUNK : (len - off));
    Wire.beginTransmission(EXT_EEPROM_ADDR);
    Wire.write((byte)(((addr+off) >> 8) & 0xFF));
    Wire.write((byte)((addr+off) & 0xFF));
    for(byte i=0;i<n;i++) Wire.write(data[off+i]);
    if(Wire.endTransmission() != 0) return false; // I2C 오류(배선/전원 문제 등)
    delay(6); // AT24C256 쓰기 사이클 시간(데이터시트 최대 5ms) + 여유
    off += n;
  }
  return true;
}
bool extI2cReadRaw(unsigned int addr, byte* data, byte len){
  unsigned int off = 0;
  while(off < len){
    byte n = (byte)((len - off > EXT_I2C_CHUNK) ? EXT_I2C_CHUNK : (len - off));
    Wire.beginTransmission(EXT_EEPROM_ADDR);
    Wire.write((byte)(((addr+off) >> 8) & 0xFF));
    Wire.write((byte)((addr+off) & 0xFF));
    if(Wire.endTransmission(false) != 0) return false; // repeated start로 버스 유지
    byte got = Wire.requestFrom((int)EXT_EEPROM_ADDR, (int)n);
    if(got != n) return false;
    for(byte i=0;i<n;i++){
      if(!Wire.available()) return false;
      data[off+i] = Wire.read();
    }
    off += n;
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

/* UTF-8 문자 중간을 자르지 않는 안전한 문자열 복사(최대 maxBytes 바이트 + 널문자).
   한글은 UTF-8에서 3바이트라서, 그냥 바이트 수로만 자르면(strncpy 등) 한글 한 글자의
   중간에서 잘려 깨진 문자가 저장/표시되는 문제가 있었음(예: SEQNAME에 긴 한글 이름을
   보내면 마지막 글자가 깨짐). 자르는 지점의 다음 바이트가 UTF-8 "이어짐 바이트"
   (상위 2비트가 10xxxxxx)이면 그 앞 글자까지만 자르도록 되돌린다. */
void utf8SafeCopy(char* dst, const char* src, int maxBytes){
  int n = strlen(src);
  if(n > maxBytes) n = maxBytes;
  while(n > 0 && (((unsigned char)src[n]) & 0xC0) == 0x80) n--;
  memcpy(dst, src, n);
  dst[n] = 0;
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
    sequences[i].numSteps=0; sequences[i].trigSrc=0; sequences[i].trigCh=0; sequences[i].trigCond=1;
    sequences[i].trigDistMode=0; sequences[i].trigDistCm=10; sequences[i].name[0]=0;
    sequences[i].condOn=0; sequences[i].condOff=0;
  }
  ledText[0] = 0; // 문구 없음(빈 화면)
  ledTrigCh = 0; ledTrigCond = 1; ledTrigSec = 3; ledTrigText[0] = 0; // 자동 알림 기본값: 사용 안 함
  for(int i=0;i<MAX_STEPPERS;i++){ steppers[i].stepPin=0; steppers[i].dirPin=0; steppers[i].enPin=0; } // 기본값: 전부 미설정
  for(int i=0;i<MAX_ULTRA;i++){ ultras[i].trigPin=0; ultras[i].echoPin=0; } // 기본값: 전부 미설정
  for(int i=0;i<MAX_LOAD;i++){ loads[i].doutPin=0; loads[i].sckPin=0; loads[i].offset=0; loads[i].scale=0; } // 기본값: 전부 미설정/미보정
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
  buf[0] = sequences[idx].trigSrc;
  buf[1] = sequences[idx].trigCh;
  buf[2] = sequences[idx].trigCond;
  buf[3] = sequences[idx].trigDistMode;
  buf[4] = sequences[idx].trigDistCm & 0xFF;
  buf[5] = (sequences[idx].trigDistCm >> 8) & 0xFF;
  buf[6] = sequences[idx].numSteps;
  for(int c=0;c<12;c++) buf[7+c] = sequences[idx].name[c];
  for(int st=0;st<MAX_STEPS;st++){
    int o = 19+st*9;
    buf[o+0] = sequences[idx].steps[st].type;
    buf[o+1] = sequences[idx].steps[st].target;
    buf[o+2] = sequences[idx].steps[st].state;
    buf[o+3] = sequences[idx].steps[st].delayMs & 0xFF;
    buf[o+4] = (sequences[idx].steps[st].delayMs>>8) & 0xFF;
    int16_t stepsVal = (int16_t)sequences[idx].steps[st].steps;
    buf[o+5] = (byte)(stepsVal & 0xFF);
    buf[o+6] = (byte)((stepsVal>>8) & 0xFF);
    buf[o+7] = sequences[idx].steps[st].speedPps & 0xFF;
    buf[o+8] = (sequences[idx].steps[st].speedPps>>8) & 0xFF;
  }
  buf[127] = calcChecksum(buf, 127); // ★ 체크섬은 레코드 맨 끝(127번)에 둔다. 예전엔 MAX_STEPS=8 시절의 91번에 그대로 남아 있어
                                     //   9번째 단계(steps[8], 91번지)의 type 바이트를 체크섬 값이 덮어쓰고 있었고,
                                     //   9~12번째 단계는 체크섬 검사 범위 밖이었다.
  unsigned int addr = EXT_SEQ_START + (unsigned int)idx*EXT_SLOT_SIZE;
  bool ok = extEepromWriteVerified(addr, buf, EXT_REC_LEN);
  extLastWriteOK = ok && (condSaveFailMask == 0); // 웹 UI가 EXTMEM?으로 조회해서 실패를 바로 알 수 있게 함
}

/* ---------------- 추가 조건(인터록) 저장/읽기 ---------------- */
void saveCondToEEPROM(int idx){
  if(!extEepromOK){ extLastWriteOK = false; condSaveFailMask |= (1UL<<idx); return; }
  byte rec[EXT_COND_SLOT];
  memset(rec, 0, sizeof(rec));
  rec[0] = EXT_COND_MAGIC;
  uint32_t on = sequences[idx].condOn, off = sequences[idx].condOff;
  for(byte b=0;b<4;b++){ rec[1+b] = (byte)((on >> (8*b)) & 0xFF); rec[5+b] = (byte)((off >> (8*b)) & 0xFF); }
  rec[EXT_COND_SLOT-1] = calcChecksum(rec, EXT_COND_SLOT-1);
  bool ok = extEepromWriteVerified(EXT_COND_START + (unsigned int)idx*EXT_COND_SLOT, rec, EXT_COND_SLOT);
  if(ok) condSaveFailMask &= ~(1UL<<idx); else condSaveFailMask |= (1UL<<idx);
  extLastWriteOK = ok && (condSaveFailMask == 0);
}
/* 부팅 시 idx번 시퀀스의 추가 조건을 읽어 sequences[idx].condOn/condOff에 채운다.
   반환값 true  = 정상(조건 없음 포함: 영역이 통째로 0xFF이면 이 기능을 쓰기 전에 저장한 시퀀스라 "조건 없음")
   반환값 false = 통신 실패 또는 기록 손상 → 호출한 쪽이 그 시퀀스를 통째로 비활성화한다
                  (조건이 깨졌는데 조건 없이 실행되면 인터록이 사라져 위험하므로) */
bool loadCondFromExt(int idx){
  sequences[idx].condOn = 0; sequences[idx].condOff = 0;
  byte rec[EXT_COND_SLOT];
  if(!extEepromReadRetry(EXT_COND_START + (unsigned int)idx*EXT_COND_SLOT, rec, EXT_COND_SLOT)) return false;
  bool blank = true;
  for(byte i=0;i<EXT_COND_SLOT;i++){ if(rec[i]!=0xFF){ blank=false; break; } }
  if(blank) return true;
  if(rec[0] != EXT_COND_MAGIC || calcChecksum(rec, EXT_COND_SLOT-1) != rec[EXT_COND_SLOT-1]) return false;
  uint32_t on = 0, off = 0;
  for(byte b=0;b<4;b++){ on |= ((uint32_t)rec[1+b]) << (8*b); off |= ((uint32_t)rec[5+b]) << (8*b); }
  sequences[idx].condOn = on; sequences[idx].condOff = off;
  return true;
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
  for(int i=0;i<MAX_STEPPERS;i++){
    EEPROM.update(EE_STEP_PINS+i*3+0, steppers[i].stepPin);
    EEPROM.update(EE_STEP_PINS+i*3+1, steppers[i].dirPin);
    EEPROM.update(EE_STEP_PINS+i*3+2, steppers[i].enPin);
  }
  for(int i=0;i<MAX_ULTRA;i++){
    EEPROM.update(EE_ULTRA_PINS+i*2+0, ultras[i].trigPin);
    EEPROM.update(EE_ULTRA_PINS+i*2+1, ultras[i].echoPin);
  }
  for(int i=0;i<MAX_LOAD;i++){
    EEPROM.update(EE_LOAD_PINS+i*2+0, loads[i].doutPin);
    EEPROM.update(EE_LOAD_PINS+i*2+1, loads[i].sckPin);
    EEPROM.put(EE_LOAD_OFFSET+i*4, loads[i].offset);
    EEPROM.put(EE_LOAD_SCALE+i*4, loads[i].scale);
  }
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
          // 새 형식(체크섬 127번, 전체 127바이트 검사). 예전 펌웨어가 저장한 기록은 체크섬이 91번에 있으므로,
          // 8단계 이하 시퀀스에 한해 그것도 받아들인다(9단계 이상은 9번째 단계가 이미 깨져 있어 받아들이지 않고
          // 손상 처리 → 부팅 경고 + 비활성화. 웹에서 "보드에 저장"을 다시 하면 정상화됨).
          bool recOk = (calcChecksum(buf,127) == buf[127]);
          if(!recOk && buf[6] <= 8 && calcChecksum(buf,91) == buf[91]) recOk = true;
          if(recOk){
            sequences[s].trigSrc     = buf[0];
            sequences[s].trigCh      = buf[1];
            sequences[s].trigCond    = buf[2];
            sequences[s].trigDistMode= buf[3];
            sequences[s].trigDistCm  = buf[4] | ((unsigned int)buf[5] << 8);
            sequences[s].numSteps    = buf[6];
            if(sequences[s].numSteps > MAX_STEPS) sequences[s].numSteps = 0;
            for(int c=0;c<12;c++) sequences[s].name[c] = buf[7+c];
            sequences[s].name[11] = 0;
            for(int st=0; st<MAX_STEPS; st++){
              int o = 19+st*9;
              sequences[s].steps[st].type = buf[o+0];
              sequences[s].steps[st].target = buf[o+1];
              sequences[s].steps[st].state = buf[o+2];
              sequences[s].steps[st].delayMs = buf[o+3] | ((unsigned int)buf[o+4] << 8);
              int16_t stepsVal = (int16_t)(buf[o+5] | ((unsigned int)buf[o+6] << 8));
              sequences[s].steps[st].steps = stepsVal;
              sequences[s].steps[st].speedPps = buf[o+7] | ((unsigned int)buf[o+8] << 8);
            }
            // 추가 조건(인터록) 읽기 — 실패/손상이면 이 시퀀스는 비활성(useEmpty 유지) + 부팅 경고
            if(loadCondFromExt(s)) useEmpty = false; else extBootWarning = true;
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
      sequences[s].numSteps=0; sequences[s].trigSrc=0; sequences[s].trigCh=0; sequences[s].trigCond=1;
      sequences[s].condOn=0; sequences[s].condOff=0;
      sequences[s].trigDistMode=0; sequences[s].trigDistCm=10; sequences[s].name[0]=0;
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
  for(int i=0;i<MAX_STEPPERS;i++){
    steppers[i].stepPin = EEPROM.read(EE_STEP_PINS+i*3+0);
    steppers[i].dirPin  = EEPROM.read(EE_STEP_PINS+i*3+1);
    steppers[i].enPin   = EEPROM.read(EE_STEP_PINS+i*3+2);
  }
  for(int i=0;i<MAX_ULTRA;i++){
    ultras[i].trigPin = EEPROM.read(EE_ULTRA_PINS+i*2+0);
    ultras[i].echoPin = EEPROM.read(EE_ULTRA_PINS+i*2+1);
  }
  for(int i=0;i<MAX_LOAD;i++){
    loads[i].doutPin = EEPROM.read(EE_LOAD_PINS+i*2+0);
    loads[i].sckPin  = EEPROM.read(EE_LOAD_PINS+i*2+1);
    EEPROM.get(EE_LOAD_OFFSET+i*4, loads[i].offset);
    EEPROM.get(EE_LOAD_SCALE+i*4, loads[i].scale);
    if(isnan(loads[i].scale)) loads[i].scale = 0; // 미기록(0xFF..) 영역을 float로 읽으면 NaN이 될 수 있음
  }
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
void applyStepperPinModes(){
  for(int i=0;i<MAX_STEPPERS;i++){
    stepMotion[i].active=false; stepMotion[i].position=0; stepMotion[i].stepsRemaining=0;
    if(steppers[i].stepPin>0){ pinMode(steppers[i].stepPin, OUTPUT); digitalWrite(steppers[i].stepPin, LOW); }
    if(steppers[i].dirPin>0){ pinMode(steppers[i].dirPin, OUTPUT); digitalWrite(steppers[i].dirPin, LOW); }
    if(steppers[i].enPin>0){ pinMode(steppers[i].enPin, OUTPUT); digitalWrite(steppers[i].enPin, HIGH); } // 기본 비활성화(active-LOW 가정, 안전)
  }
}
void applyUltraPinModes(){
  for(int i=0;i<MAX_ULTRA;i++){
    if(ultras[i].trigPin>0 && ultras[i].trigPin!=ultras[i].echoPin){
      pinMode(ultras[i].trigPin, OUTPUT); digitalWrite(ultras[i].trigPin, LOW);
    }
    // SIG 1핀형(trigPin==echoPin)이거나 echo 전용 핀은, 측정 순간에만 readUltraCm()이 그때그때
    // pinMode를 바꿔가며 쓰므로 여기서는 미리 고정된 모드로 잡아두지 않는다.
  }
}
void applyLoadPinModes(){
  for(int i=0;i<MAX_LOAD;i++){
    if(loads[i].doutPin>0) pinMode(loads[i].doutPin, INPUT); // HX711 DOUT(데이터, 준비되면 LOW)
    if(loads[i].sckPin>0){ pinMode(loads[i].sckPin, OUTPUT); digitalWrite(loads[i].sckPin, LOW); } // SCK(클럭)
  }
}

/* ================= 릴레이 제어 ================= */
void setRelay(int ch, bool on){
  if(ch < 1 || ch > TOTAL_RELAYS) return;
  digitalWrite(outPins[ch-1], relayPhysLevel(on)); // 논리 ON/OFF -> 보드 종류에 맞는 실제 핀 레벨
  relayState[ch-1] = on ? 1 : 0; // 논리 상태로 저장
}

/* ================= 스텝모터 제어 ================= */
void stepperSetEnable(int ch1, bool en){
  int i = ch1-1;
  if(i<0 || i>=MAX_STEPPERS || steppers[i].enPin==0) return;
  digitalWrite(steppers[i].enPin, en ? LOW : HIGH); // active-LOW 가정: LOW=활성화(토크 유지), HIGH=비활성화
}
void stepperStartMove(int ch1, long steps, int speedPps){
  int i = ch1-1;
  if(i<0 || i>=MAX_STEPPERS) return;
  if(steppers[i].stepPin==0 || steppers[i].dirPin==0) return; // 핀이 아직 지정 안 됨
  if(speedPps < 1) speedPps = 1;
  if(speedPps > 3000) speedPps = 3000; // 이 보드는 loop() 안에서 소프트웨어로 펄스를 내므로
                                        // 실제 달성 속도는 키패드 스캔 등 다른 작업 부하에 따라
                                        // 이보다 낮을 수 있음(상한은 과도한 요청값을 막는 안전장치)
  bool dirPos = (steps >= 0);
  digitalWrite(steppers[i].dirPin, dirPos ? HIGH : LOW);
  if(steppers[i].enPin>0) digitalWrite(steppers[i].enPin, LOW); // 이동 시작 시 자동 활성화
  stepMotion[i].dirPositive = dirPos;
  stepMotion[i].stepsRemaining = labs(steps);
  stepMotion[i].pulseIntervalUs = 1000000UL / (unsigned long)speedPps;
  stepMotion[i].nextPulseUs = micros();
  stepMotion[i].active = (stepMotion[i].stepsRemaining > 0);
}
void stepperStop(int ch1){
  int i = ch1-1;
  if(i<0 || i>=MAX_STEPPERS) return;
  stepMotion[i].active = false;
}
/* 매 loop()마다 호출: 각 채널이 지금 펄스를 낼 시각인지만 확인해서 한 스텝씩만 진행한다.
   delay() 대신 micros()로 시간을 재므로, 여러 채널이 동시에 움직여도 서로 막지 않고
   릴레이/키패드/시리얼 처리도 그 사이에 정상적으로 계속 돈다. */
void updateSteppers(){
  unsigned long now = micros();
  for(int i=0;i<MAX_STEPPERS;i++){
    if(!stepMotion[i].active) continue;
    if((long)(now - stepMotion[i].nextPulseUs) < 0) continue; // 아직 다음 펄스 시각이 안 됨
    digitalWrite(steppers[i].stepPin, HIGH);
    delayMicroseconds(3); // 드라이버가 요구하는 최소 펄스 폭(대부분의 A4988/DRV8825류는 1~2us면 충분)
    digitalWrite(steppers[i].stepPin, LOW);
    stepMotion[i].position += stepMotion[i].dirPositive ? 1 : -1;
    stepMotion[i].stepsRemaining--;
    stepMotion[i].nextPulseUs = now + stepMotion[i].pulseIntervalUs;
    if(stepMotion[i].stepsRemaining <= 0) stepMotion[i].active = false;
  }
}

/* ================= 초음파 거리센서 ================= */
/* 채널 1개를 즉석에서 측정(트리거 펄스 + pulseIn으로 에코 폭 측정, 원리상 최대 수십ms 소요).
   반환값: cm(정수), 에코 없음/타임아웃/미설정이면 -1.
   TRIG==ECHO(0이 아님)면 SIG 1핀형: 핀을 OUTPUT으로 잠깐 바꿔 펄스를 보낸 뒤,
   곧바로 INPUT으로 되돌려 같은 핀에서 에코를 읽는다(일반 HC-SR04의 TRIG+ECHO 2핀 배선을
   1핀으로 합친 모듈용). */
/* 채널 1개의 초음파 왕복시간을 그대로 반환(us). 실패 시 -1.
   readUltraCm/readUltraMm이 여기서 값을 얻어 각각 cm(정수, 기존 방식 그대로 유지)과
   mm(반올림, 스텝모터 자동 보정용 고정밀 값)으로 변환한다. */
long readUltraDurUs(int ch1){
  int i = ch1-1;
  if(i<0 || i>=MAX_ULTRA) return -1;
  byte trigPin = ultras[i].trigPin, echoPin = ultras[i].echoPin;
  if(trigPin==0 || echoPin==0) return -1;
  bool sigMode = (trigPin==echoPin);
  if(sigMode){
    pinMode(trigPin, OUTPUT); digitalWrite(trigPin, LOW); delayMicroseconds(2);
    digitalWrite(trigPin, HIGH); delayMicroseconds(10);
    digitalWrite(trigPin, LOW);
    pinMode(trigPin, INPUT); // 같은 핀에서 곧바로 에코를 읽기 위해 입력으로 전환
    unsigned long dur = pulseIn(trigPin, HIGH, 25000UL); // 약 4m 왕복 기준 타임아웃(25ms)
    return dur==0 ? -1L : (long)dur;
  } else {
    pinMode(trigPin, OUTPUT); digitalWrite(trigPin, LOW); delayMicroseconds(2);
    digitalWrite(trigPin, HIGH); delayMicroseconds(10);
    digitalWrite(trigPin, LOW);
    pinMode(echoPin, INPUT);
    unsigned long dur = pulseIn(echoPin, HIGH, 25000UL);
    return dur==0 ? -1L : (long)dur;
  }
}
int readUltraCm(int ch1){
  long dur = readUltraDurUs(ch1);
  return dur<0 ? -1 : (int)(dur / 58L); // 음속 기준 통상 근사식: cm = 왕복시간(us) / 58 (버림, 기존 동작 그대로)
}
int readUltraMm(int ch1){
  long dur = readUltraDurUs(ch1);
  if(dur<0) return -1;
  return (int)((dur*10L + 29L) / 58L); // cm 공식과 같은 상수를 mm 단위로 반올림(스텝모터 자동 보정용 — HC-SR04류 실측 정밀도는 보통 ±2~3mm 수준)
}
void sendUltraReadings(){
  String s = "ULTRA ";
  for(int i=0;i<MAX_ULTRA;i++){
    int cm = (ultras[i].trigPin>0 && ultras[i].echoPin>0) ? readUltraCm(i+1) : -1;
    s += String(i+1)+":"+String(cm);
    if(i<MAX_ULTRA-1) s += ",";
  }
  replyPort->println(s);
}
void sendUltraMm(int ch1){
  int mm = (ch1>=1 && ch1<=MAX_ULTRA && ultras[ch1-1].trigPin>0 && ultras[ch1-1].echoPin>0) ? readUltraMm(ch1) : -1;
  replyPort->println(String("ULTRAMM ")+ch1+":"+mm);
}

/* ================= 로드셀(HX711) ================= */
/* HX711 프로토콜 1회 읽기(라이브러리 없이 직접 비트뱅잉). DOUT이 LOW로 떨어져야 준비된 것이며,
   최대 timeoutMs까지만 기다린다(모듈 RATE 설정에 따라 보통 최대 ~100ms 걸림). 준비 안 되면 LONG_MIN. */
long hx711ReadRawOnce(byte dout, byte sck, unsigned int timeoutMs){
  unsigned long t0 = millis();
  while(digitalRead(dout) == HIGH){ if(millis()-t0 > timeoutMs) return LONG_MIN; }
  long value = 0;
  for(int i=0;i<24;i++){
    digitalWrite(sck, HIGH);
    delayMicroseconds(1);
    value = (value<<1) | digitalRead(dout);
    digitalWrite(sck, LOW);
    delayMicroseconds(1);
  }
  digitalWrite(sck, HIGH); delayMicroseconds(1); digitalWrite(sck, LOW); delayMicroseconds(1); // 25번째 펄스: gain=128, 채널A 고정
  if(value & 0x800000UL) value |= 0xFF000000UL; // 24bit -> 32bit 부호 확장
  return value;
}
/* 채널 1개를 여러 번(samples) 재서 평균낸 원시값. 실패(미설정/타임아웃)면 LONG_MIN. */
long readLoadRawAvg(int ch1, byte samples){
  int i = ch1-1;
  if(i<0 || i>=MAX_LOAD) return LONG_MIN;
  byte dout = loads[i].doutPin, sck = loads[i].sckPin;
  if(dout==0 || sck==0) return LONG_MIN;
  long sum=0; byte got=0;
  for(byte s=0;s<samples;s++){
    long v = hx711ReadRawOnce(dout, sck, 150);
    if(v==LONG_MIN) continue;
    sum += v; got++;
    if(s < samples-1) delay(2); // 다음 변환 사이클까지 살짝 간격
  }
  if(got==0) return LONG_MIN;
  return sum / got;
}
/* 채널 1개의 무게(그램, 반올림). scale 미보정(0)이거나 측정 실패면 -9999. */
long readLoadGrams(int ch1, byte samples){
  int i = ch1-1;
  if(i<0 || i>=MAX_LOAD) return -9999;
  if(loads[i].scale == 0) return -9999; // 보정 전(LOADCAL 안 함)
  long raw = readLoadRawAvg(ch1, samples);
  if(raw == LONG_MIN) return -9999;
  return lround((double)(raw - loads[i].offset) / loads[i].scale);
}
void sendLoadConfig(){
  String s = "LOADCONFIG ";
  for(int i=0;i<MAX_LOAD;i++){
    s += String(i+1)+":"+String(loads[i].doutPin)+":"+String(loads[i].sckPin);
    if(i<MAX_LOAD-1) s += ",";
  }
  replyPort->println(s);
}
void sendLoadCal(){
  String s = "LOADCAL ";
  for(int i=0;i<MAX_LOAD;i++){
    s += String(i+1)+":"+String(loads[i].offset)+":"+String(loads[i].scale,4);
    if(i<MAX_LOAD-1) s += ",";
  }
  replyPort->println(s);
}
void sendLoadReadings(){
  String s = "LOAD ";
  for(int i=0;i<MAX_LOAD;i++){
    long g = (loads[i].doutPin>0 && loads[i].sckPin>0) ? readLoadGrams(i+1, 4) : -9999;
    s += String(i+1)+":"+String(g);
    if(i<MAX_LOAD-1) s += ",";
  }
  replyPort->println(s);
}

/* ================= 응답 전송 ================= */
void sendStatus(){
  String s = "STATUS ";
  for(int i=0;i<TOTAL_RELAYS;i++){ s += String(relayState[i]); if(i<TOTAL_RELAYS-1) s += ","; }
  replyPort->println(s);
}
void sendInputs(){
  String s = "INPUT ";
  for(int i=0;i<inCount;i++){ s += String(inStableState[i]); if(i<inCount-1) s += ","; }
  replyPort->println(s);
}
void sendConfig(){
  String s = "CONFIG OUT ";
  for(int i=0;i<TOTAL_RELAYS;i++){ s += String(i+1)+":"+String(outPins[i]); if(i<TOTAL_RELAYS-1) s += ","; }
  s += " IN ";
  for(int i=0;i<inCount;i++){ s += String(i+1)+":"+String(inPins[i]); if(i<inCount-1) s += ","; }
  replyPort->println(s);
}
void sendKpConfig(){
  String s = "KPCONFIG ROW ";
  for(int i=0;i<4;i++){ s += String(kpRow[i]); if(i<3) s += ","; }
  s += " COL ";
  for(int i=0;i<4;i++){ s += String(kpCol[i]); if(i<3) s += ","; }
  s += " EN " + String(kpEnabled ? 1 : 0);
  replyPort->println(s);
}
void sendStepConfig(){
  String s = "STEPCONFIG ";
  for(int i=0;i<MAX_STEPPERS;i++){
    s += String(i+1)+":"+String(steppers[i].stepPin)+":"+String(steppers[i].dirPin)+":"+String(steppers[i].enPin);
    if(i<MAX_STEPPERS-1) s += ",";
  }
  replyPort->println(s);
}
void sendStepStatus(){
  String s = "STEPSTATUS ";
  for(int i=0;i<MAX_STEPPERS;i++){
    s += String(i+1)+":"+String(stepMotion[i].active?1:0)+":"+String(stepMotion[i].stepsRemaining)+":"+String(stepMotion[i].position);
    if(i<MAX_STEPPERS-1) s += ",";
  }
  replyPort->println(s);
}
void sendUltraConfig(){
  String s = "ULTRACONFIG ";
  for(int i=0;i<MAX_ULTRA;i++){
    s += String(i+1)+":"+String(ultras[i].trigPin)+":"+String(ultras[i].echoPin);
    if(i<MAX_ULTRA-1) s += ",";
  }
  replyPort->println(s);
}
void sendSeqGet(){
  for(int i=0;i<MAX_SEQ;i++){
    if(sequences[i].numSteps == 0) continue;
    String s = "SEQ ";
    s += String(i)+" "+String(sequences[i].trigSrc)+" "+String(sequences[i].trigCh)+" "+String(sequences[i].trigCond)+" "
       + String(sequences[i].trigDistMode)+" "+String(sequences[i].trigDistCm)+" "+String(sequences[i].numSteps)+" ";
    String nm = String(sequences[i].name);
    if(nm.length()==0) nm = "seq"+String(i+1);
    s += nm + " ";
    for(int st=0; st<sequences[i].numSteps; st++){
      s += String(sequences[i].steps[st].type)+","+String(sequences[i].steps[st].target)+","
         + String(sequences[i].steps[st].state)+","+String(sequences[i].steps[st].delayMs)+","
         + String(sequences[i].steps[st].steps)+","+String(sequences[i].steps[st].speedPps);
      if(st < sequences[i].numSteps-1) s += ";";
    }
    replyPort->println(s);
    if(sequences[i].condOn != 0 || sequences[i].condOff != 0){
      String c = "SEQCOND ";
      c += String(i)+" "+String((unsigned long)sequences[i].condOn)+" "+String((unsigned long)sequences[i].condOff);
      replyPort->println(c);
    }
  }
  replyPort->println("SEQGETDONE");
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
  if(strncmp(buf,"RAWPIN ",7)==0){
    // 아직 입력 채널로 등록하지 않은 임의의 핀을 즉석에서 확인하기 위한 명령.
    // (웹 화면 "11. 테스트"의 "핀 원시 상태 확인" 기능에서 사용)
    // 해당 핀을 INPUT_PULLUP으로 설정한 뒤 즉시 읽어서 결과만 보내고,
    // 다른 용도(릴레이 출력 등)로 이미 쓰이는 핀 배열은 전혀 건드리지 않는다.
    int pin;
    if(sscanf(buf+7,"%d",&pin)==1){
      pinMode(pin, INPUT_PULLUP);
      delayMicroseconds(50);
      int raw = digitalRead(pin)==LOW ? 1 : 0;
      replyPort->print("RAWPIN ");
      replyPort->print(pin);
      replyPort->print(' ');
      replyPort->println(raw);
    }
    return;
  }
  if(strcmp(buf,"RAWSCAN?")==0){
    // 핀 번호를 몰라도 어떤 핀이 눌렸는지 찾기 위한 "자동 감지" 기능용.
    // 릴레이 출력으로 이미 쓰는 핀, (사용 중이면) 키패드 행/열 핀, I2C(20,21)는
    // 절대 건드리면 안 되므로 스캔 대상에서 자동으로 제외한다.
    bool first=true;
    replyPort->print("RAWSCAN ");
    for(int pin=2; pin<=69; pin++){
      if(pin==20 || pin==21) continue; // I2C SDA/SCL (OLED·외장 EEPROM 전용)
      bool used=false;
      for(int i=0;i<TOTAL_RELAYS;i++){ if(outPins[i]==pin){ used=true; break; } }
      if(!used && kpEnabled){
        for(int i=0;i<4;i++){ if(kpRow[i]==pin || kpCol[i]==pin){ used=true; break; } }
      }
      if(!used){
        for(int i=0;i<MAX_STEPPERS;i++){
          if(steppers[i].stepPin==pin || steppers[i].dirPin==pin || steppers[i].enPin==pin){ used=true; break; }
        }
      }
      if(!used){
        for(int i=0;i<MAX_ULTRA;i++){
          if(ultras[i].trigPin==pin || ultras[i].echoPin==pin){ used=true; break; }
        }
      }
      if(!used){
        for(int i=0;i<MAX_LOAD;i++){
          if(loads[i].doutPin==pin || loads[i].sckPin==pin){ used=true; break; }
        }
      }
      if(used) continue;
      pinMode(pin, INPUT_PULLUP);
      int raw = digitalRead(pin)==LOW ? 1 : 0;
      if(!first) replyPort->print(',');
      replyPort->print(pin); replyPort->print(':'); replyPort->print(raw);
      first=false;
    }
    replyPort->println();
    return;
  }
  if(strncmp(buf,"SETOUT ",7)==0){
    int ch,pin;
    if(sscanf(buf+7,"%d %d",&ch,&pin)==2 && ch>=1 && ch<=TOTAL_RELAYS){
      outPins[ch-1]=pin; pinMode(pin, OUTPUT); digitalWrite(pin, relayPhysLevel(relayState[ch-1]==1));
      EEPROM.update(EE_OUT_PINS+(ch-1), pin);
      replyPort->println("OK");
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
      replyPort->println("OK");
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
      replyPort->println("OK");
    }
    return;
  }
  if(strncmp(buf,"SETKPCOL ",9)==0){
    int idx,pin;
    if(sscanf(buf+9,"%d %d",&idx,&pin)==2 && idx>=0 && idx<4){
      kpCol[idx]=pin;
      if(kpEnabled) pinMode(pin,INPUT_PULLUP);
      EEPROM.update(EE_KP_COL+idx,pin);
      replyPort->println("OK");
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
      replyPort->println("OK");
    }
    return;
  }
  if(strcmp(buf,"GETKPCONFIG?")==0){ sendKpConfig(); return; }
  if(strncmp(buf,"SETLED ",7)==0){
    strncpy(ledText, buf+7, LED_TEXT_MAX);
    ledText[LED_TEXT_MAX] = 0;
    saveLedTextToEEPROM();
    drawLedText();
    replyPort->println("OK");
    return;
  }
  if(strcmp(buf,"GETLED?")==0){
    replyPort->print("LEDTEXT ");
    replyPort->println(ledText);
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
      replyPort->println("OK");
    }
    return;
  }
  if(strcmp(buf,"GETLEDTRIG?")==0){
    replyPort->print("LEDTRIG ");
    replyPort->print(ledTrigCh); replyPort->print(" ");
    replyPort->print(ledTrigCond); replyPort->print(" ");
    replyPort->print(ledTrigSec); replyPort->print(" ");
    replyPort->println(ledTrigText);
    return;
  }
  if(strcmp(buf,"SEQCLR")==0){
    for(int i=0;i<MAX_SEQ;i++){
      sequences[i].numSteps=0; sequences[i].trigSrc=0; sequences[i].trigCh=0; sequences[i].trigCond=1;
      sequences[i].trigDistMode=0; sequences[i].trigDistCm=10; sequences[i].name[0]=0;
      sequences[i].condOn=0; sequences[i].condOff=0;
      saveSeqToEEPROM(i); saveCondToEEPROM(i); seqLastInputState[i]=255; // 추가 조건도 함께 지움(항상 기록: 손상된 기록이 남지 않게)
    }
    replyPort->println("OK");
    return;
  }
  if(strncmp(buf,"SEQTRIG ",8)==0){
    // SEQTRIG <idx> <trigSrc:0=입력채널|1=초음파거리|2=로드셀무게> <trigCh> <trigCond:0=OFF|1=ON|2=BOTH> <trigDistMode:0=이하|1=이상> <trigDistCm(g 또는 cm)>
    int idx,trigSrc,trigCh,trigCond,trigDistMode,trigDistCm;
    if(sscanf(buf+8,"%d %d %d %d %d %d",&idx,&trigSrc,&trigCh,&trigCond,&trigDistMode,&trigDistCm)==6 && idx>=0 && idx<MAX_SEQ){
      sequences[idx].trigSrc=trigSrc; sequences[idx].trigCh=trigCh; sequences[idx].trigCond=trigCond;
      sequences[idx].trigDistMode=trigDistMode; sequences[idx].trigDistCm=trigDistCm;
      // 트리거를 새로 설정(또는 재설정)하는 이 순간의 실제 조건 충족 상태로 "이전 상태"를 맞춰둔다.
      // (setup()의 부팅 시 처리와 동일한 이유: 무조건 255로 두면, 마침 그 순간 조건이 트리거
      //  조건과 우연히 같을 경우 버튼/센서 동작이 없었는데도 "전체 시퀀스를 보드에 저장" 등으로
      //  SEQTRIG가 실행되자마자 시퀀스가 즉시 오발동하는 문제가 있었음)
      if(trigSrc==0 && trigCh>=1 && trigCh<=inCount) seqLastInputState[idx] = inStableState[trigCh-1];
      else seqLastInputState[idx] = 255; // 초음파/로드셀은 아직 측정 전이라 255(미초기화)로 시작 — 첫 측정 때 자동으로 맞춰짐
      saveSeqToEEPROM(idx);
      replyPort->println("OK");
    }
    return;
  }
  if(strncmp(buf,"SEQSTEP ",8)==0){
    // SEQSTEP <idx> <si> <type:0=릴레이|1=스텝모터> <target> <state> <delayMs> <steps> <speedPps>
    int idx,si,type,target,state,delayMs,steps,speedPps;
    if(sscanf(buf+8,"%d %d %d %d %d %d %d %d",&idx,&si,&type,&target,&state,&delayMs,&steps,&speedPps)==8
       && idx>=0 && idx<MAX_SEQ && si>=0 && si<MAX_STEPS){
      sequences[idx].steps[si].type=(byte)type;
      sequences[idx].steps[si].target=(byte)target;
      sequences[idx].steps[si].state=(byte)state;
      sequences[idx].steps[si].delayMs=(unsigned int)delayMs;
      sequences[idx].steps[si].steps=steps;
      sequences[idx].steps[si].speedPps=(unsigned int)speedPps;
      saveSeqToEEPROM(idx);
      replyPort->println("OK");
    }
    return;
  }
  if(strncmp(buf,"SEQNAME ",8)==0){
    // nm은 넉넉하게(최대 39바이트) 받은 뒤, sequences[].name(12바이트, 11+널)에 옮겨 담을 때
    // utf8SafeCopy()로 한글 등 멀티바이트 문자 중간이 잘리지 않게 안전하게 자른다.
    int idx; char nm[40];
    if(sscanf(buf+8,"%d %39s",&idx,nm)==2 && idx>=0 && idx<MAX_SEQ){
      utf8SafeCopy(sequences[idx].name, nm, 11);
      saveSeqToEEPROM(idx);
      replyPort->println("OK");
    }
    return;
  }
  if(strncmp(buf,"SEQCOUNT ",9)==0){
    int idx,count;
    if(sscanf(buf+9,"%d %d",&idx,&count)==2 && idx>=0 && idx<MAX_SEQ){
      sequences[idx].numSteps = count>MAX_STEPS?MAX_STEPS:(count<0?0:count);
      saveSeqToEEPROM(idx);
      replyPort->println("OK");
    }
    return;
  }
  if(strcmp(buf,"SEQCOND?")==0){
    // 펌웨어가 SEQCOND를 지원하는지 확인만 한다(아무것도 바꾸지 않음)
    replyPort->println("SEQCOND OK MAXCH 32");
    return;
  }
  if(strncmp(buf,"SEQCOND ",8)==0){
    // SEQCOND <idx> <ON마스크> <OFF마스크>  (bit0=입력CH1 … bit31=입력CH32, 부호 없는 10진수)
    // sscanf 대신 strtol/strtoul을 쓰는 이유: 32비트 부호 없는 값이라 %lu 지원 여부에 의존하지 않기 위함
    char *p = buf+8, *e;
    long idx = strtol(p,&e,10); if(e==p) return; p = e;
    unsigned long on  = strtoul(p,&e,10); if(e==p) return; p = e;
    unsigned long off = strtoul(p,&e,10); if(e==p) return;
    if(idx>=0 && idx<MAX_SEQ){
      sequences[idx].condOn  = (uint32_t)on;
      sequences[idx].condOff = (uint32_t)off;
      saveCondToEEPROM((int)idx);
      replyPort->println("OK");
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
      replyPort->println("OK");
    }
    return;
  }
  if(strcmp(buf,"GETRMODE?")==0){
    replyPort->print("RMODE ");
    replyPort->println(relayActiveLow ? 1 : 0);
    return;
  }
  if(strncmp(buf,"SETSTEPPIN ",11)==0){
    int ch,stepPin,dirPin,enPin;
    if(sscanf(buf+11,"%d %d %d %d",&ch,&stepPin,&dirPin,&enPin)==4 && ch>=1 && ch<=MAX_STEPPERS){
      int i=ch-1;
      steppers[i].stepPin=stepPin; steppers[i].dirPin=dirPin; steppers[i].enPin=enPin;
      stepMotion[i].active=false; stepMotion[i].position=0; stepMotion[i].stepsRemaining=0;
      if(stepPin>0){ pinMode(stepPin, OUTPUT); digitalWrite(stepPin, LOW); }
      if(dirPin>0){ pinMode(dirPin, OUTPUT); digitalWrite(dirPin, LOW); }
      if(enPin>0){ pinMode(enPin, OUTPUT); digitalWrite(enPin, HIGH); } // 기본 비활성화(안전)
      EEPROM.update(EE_STEP_PINS+i*3+0, stepPin);
      EEPROM.update(EE_STEP_PINS+i*3+1, dirPin);
      EEPROM.update(EE_STEP_PINS+i*3+2, enPin);
      replyPort->println("OK");
    }
    return;
  }
  if(strcmp(buf,"GETSTEPCONFIG?")==0){ sendStepConfig(); return; }
  if(strncmp(buf,"STEPEN ",7)==0){
    int ch,en;
    if(sscanf(buf+7,"%d %d",&ch,&en)==2 && ch>=1 && ch<=MAX_STEPPERS){
      stepperSetEnable(ch, en!=0);
      replyPort->println("OK");
    }
    return;
  }
  if(strncmp(buf,"STEPMOVE ",9)==0){
    int ch,speed; long steps;
    if(sscanf(buf+9,"%d %ld %d",&ch,&steps,&speed)==3 && ch>=1 && ch<=MAX_STEPPERS){
      stepperStartMove(ch, steps, speed);
      replyPort->println("OK");
    }
    return;
  }
  if(strncmp(buf,"STEPSTOP ",9)==0){
    int ch;
    if(sscanf(buf+9,"%d",&ch)==1 && ch>=1 && ch<=MAX_STEPPERS){
      stepperStop(ch);
      replyPort->println("OK");
    }
    return;
  }
  if(strcmp(buf,"STEPSTOPALL")==0){
    for(int i=0;i<MAX_STEPPERS;i++) stepMotion[i].active=false;
    replyPort->println("OK");
    return;
  }
  if(strncmp(buf,"STEPZERO ",9)==0){
    int ch;
    if(sscanf(buf+9,"%d",&ch)==1 && ch>=1 && ch<=MAX_STEPPERS){
      stepMotion[ch-1].position = 0;
      replyPort->println("OK");
    }
    return;
  }
  if(strcmp(buf,"STEPSTATUS?")==0){ sendStepStatus(); return; }
  if(strncmp(buf,"SETULTRAPIN ",12)==0){
    int ch,trigPin,echoPin;
    if(sscanf(buf+12,"%d %d %d",&ch,&trigPin,&echoPin)==3 && ch>=1 && ch<=MAX_ULTRA){
      int i=ch-1;
      ultras[i].trigPin=trigPin; ultras[i].echoPin=echoPin;
      if(trigPin>0 && trigPin!=echoPin){ pinMode(trigPin, OUTPUT); digitalWrite(trigPin, LOW); }
      EEPROM.update(EE_ULTRA_PINS+i*2+0, trigPin);
      EEPROM.update(EE_ULTRA_PINS+i*2+1, echoPin);
      replyPort->println("OK");
    }
    return;
  }
  if(strcmp(buf,"GETULTRACONFIG?")==0){ sendUltraConfig(); return; }
  if(strcmp(buf,"ULTRA?")==0){ sendUltraReadings(); return; }
  if(strncmp(buf,"ULTRAMM ",8)==0){ sendUltraMm(atoi(buf+8)); return; } // ULTRAMM <채널> -> ULTRAMM <채널>:<mm>  (스텝모터 자동 보정용 고정밀 단일채널 측정)
  if(strncmp(buf,"SETLOADPIN ",11)==0){
    int ch,dout,sck;
    if(sscanf(buf+11,"%d %d %d",&ch,&dout,&sck)==3 && ch>=1 && ch<=MAX_LOAD){
      int i=ch-1;
      loads[i].doutPin=dout; loads[i].sckPin=sck;
      if(dout>0) pinMode(dout, INPUT);
      if(sck>0){ pinMode(sck, OUTPUT); digitalWrite(sck, LOW); }
      EEPROM.update(EE_LOAD_PINS+i*2+0, dout);
      EEPROM.update(EE_LOAD_PINS+i*2+1, sck);
      replyPort->println("OK");
    }
    return;
  }
  if(strcmp(buf,"GETLOADCONFIG?")==0){ sendLoadConfig(); return; }
  if(strcmp(buf,"LOAD?")==0){ sendLoadReadings(); return; }
  if(strncmp(buf,"LOADTARE ",9)==0){
    int ch = atoi(buf+9);
    if(ch>=1 && ch<=MAX_LOAD){
      long raw = readLoadRawAvg(ch, 10);
      if(raw==LONG_MIN){ replyPort->println(String("LOADTAREFAIL ")+ch); return; }
      loads[ch-1].offset = raw;
      EEPROM.put(EE_LOAD_OFFSET+(ch-1)*4, loads[ch-1].offset);
      replyPort->println(String("LOADTAREOK ")+ch+" "+raw);
    }
    return;
  }
  if(strncmp(buf,"LOADCAL ",8)==0){
    int ch; float knownG;
    if(sscanf(buf+8,"%d %f",&ch,&knownG)==2 && ch>=1 && ch<=MAX_LOAD && knownG!=0){
      long raw = readLoadRawAvg(ch, 10);
      if(raw==LONG_MIN){ replyPort->println(String("LOADCALFAIL ")+ch); return; }
      float scale = (float)(raw - loads[ch-1].offset) / knownG; // 그램당 원시값 카운트
      loads[ch-1].scale = scale;
      EEPROM.put(EE_LOAD_SCALE+(ch-1)*4, loads[ch-1].scale);
      replyPort->println(String("LOADCALOK ")+ch+" "+String(scale,4));
    }
    return;
  }
  if(strcmp(buf,"GETLOADCAL?")==0){ sendLoadCal(); return; }
  if(strcmp(buf,"EXTMEM?")==0){
    // 웹 화면이 "외장 시퀀스 메모리(AT24C256)" 상태를 확인할 때 씀
    replyPort->print("EXTMEM ");
    replyPort->print(extEepromOK ? "CONNECTED" : "NOTFOUND"); replyPort->print(" ");
    replyPort->print(extBootWarning ? "BOOTWARN" : "NOWARN"); replyPort->print(" ");
    replyPort->println(extLastWriteOK ? "WRITEOK" : "WRITEFAIL");
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
        broadcastLine("KEY " + String(kpDebounced)); // 비동기 알림이므로 USB·블루투스 양쪽에 모두 전송
        kpSentKey = kpDebounced;
      } else if(kpDebounced == 0){
        kpSentKey = 0;
      }
    }
  }
}

/* ================= 입력 디바운스 + 보드 독립 시퀀스 실행 ================= */
/* 지금 어떤 활성 슬롯이든 relayCh 릴레이를 쓰고 있는 시퀀스가 있는지 확인.
   같은 릴레이를 서로 다른 시퀀스가 동시에 건드리는 것만 막기 위한 용도(스텝모터 타입 단계는 무시). */
bool relayBusyInActiveSlots(int relayCh){
  for(int i=0;i<MAX_ACTIVE_SEQ;i++){
    if(activeSeqs[i].idx==-1) continue;
    Seq &sq = sequences[activeSeqs[i].idx];
    for(int s=0;s<sq.numSteps;s++){
      if(sq.steps[s].type==0 && sq.steps[s].target==relayCh) return true;
    }
  }
  return false;
}
/* 위와 동일한 이유로, 같은 스텝모터 채널을 서로 다른 시퀀스가 동시에 움직이려 하는 것만 막는다
   (릴레이 타입 단계는 무시). 겹치면 모터가 엉뚱한 위치로 움직일 수 있어 위험하다. */
bool stepperBusyInActiveSlots(int motorCh){
  for(int i=0;i<MAX_ACTIVE_SEQ;i++){
    if(activeSeqs[i].idx==-1) continue;
    Seq &sq = sequences[activeSeqs[i].idx];
    for(int s=0;s<sq.numSteps;s++){
      if(sq.steps[s].type==1 && sq.steps[s].target==motorCh) return true;
    }
  }
  return false;
}
/* 시퀀스를 "시작"만 한다 — 실제 단계 진행은 매 loop()마다 updateRunningSequences()가 담당.
   서로 다른 릴레이/스텝모터를 쓰는 시퀀스는 최대 MAX_ACTIVE_SEQ개까지 동시에 진행할 수 있지만,
   지금 도는 시퀀스와 릴레이 또는 스텝모터 채널이 하나라도 겹치면(같은 릴레이/모터를 동시에
   두 곳에서 건드리면 오동작할 수 있어 위험) 이번 트리거는 건너뛴다. */
void startSequence(int idx){
  if(sequences[idx].numSteps <= 0) return;
  for(int i=0;i<MAX_ACTIVE_SEQ;i++) if(activeSeqs[i].idx==idx) return; // 같은 시퀀스가 이미 실행 중
  for(int s=0;s<sequences[idx].numSteps;s++){
    SeqStep &sst = sequences[idx].steps[s];
    if(sst.type==0){ if(relayBusyInActiveSlots(sst.target)) return; } // 릴레이 충돌 → 건너뜀
    else { if(stepperBusyInActiveSlots(sst.target)) return; }         // 스텝모터 충돌 → 건너뜀
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
    if(st.type==0){
      setRelay(st.target, st.state==1);
    } else {
      stepperStartMove(st.target, (long)st.steps, (int)st.speedPps); // 이동은 "시작"만 하고 곧장 다음 단계로(완료 대기는 delayMs로 직접 맞춤)
    }
    activeSeqs[i].dueMs = now + st.delayMs; // delayMs가 0이면 바로 다음 loop()에서 이 슬롯의 다음 단계 진행
    activeSeqs[i].step++;
  }
}

/* 초음파 거리를 "트리거"로 쓰는 시퀀스가 있을 때만, 그 채널들을 주기적으로 재측정해서 캐시해둔다.
   초음파 측정 1회는 응답이 없으면 최대 25ms까지 loop()를 멈추게 하므로(pulseIn 타임아웃),
   매 loop()마다 재지 않고 이 간격으로만 갱신하며, 실제로 트리거로 쓰이는 채널만 골라서 잰다. */
int ultraSeqLastCm[MAX_ULTRA] = {-1,-1,-1,-1};
unsigned long lastUltraSeqSampleMs = 0;
const unsigned long ULTRA_SEQ_SAMPLE_INTERVAL_MS = 400;
void sampleUltraForSequenceTriggers(){
  unsigned long now = millis();
  if(now - lastUltraSeqSampleMs < ULTRA_SEQ_SAMPLE_INTERVAL_MS) return;
  lastUltraSeqSampleMs = now;
  bool needed[MAX_ULTRA] = {false,false,false,false};
  for(int s=0;s<MAX_SEQ;s++){
    if(sequences[s].numSteps==0 || sequences[s].trigSrc!=1) continue;
    int ch = sequences[s].trigCh;
    if(ch>=1 && ch<=MAX_ULTRA) needed[ch-1]=true;
  }
  for(int c=0;c<MAX_ULTRA;c++){
    if(needed[c]) ultraSeqLastCm[c] = readUltraCm(c+1); // -1=응답없음/미설정
  }
}

/* 로드셀 무게를 "트리거"로 쓰는 시퀀스가 있을 때만, 그 채널들을 주기적으로 재측정해서 캐시해둔다.
   HX711 1회 읽기는 준비될 때까지 최대 150ms(hx711ReadRawOnce 타임아웃)까지 기다릴 수 있으므로,
   초음파와 동일하게 매 loop()마다 재지 않고 이 간격으로만 갱신하며, 실제로 트리거로 쓰이는
   채널만 골라서 잰다(평균 3샘플, 보정 전 채널은 -9999로 표시되어 조건 판정을 건너뛰게 됨). */
long loadSeqLastG[MAX_LOAD] = {-9999,-9999,-9999,-9999};
unsigned long lastLoadSeqSampleMs = 0;
const unsigned long LOAD_SEQ_SAMPLE_INTERVAL_MS = 500;
void sampleLoadForSequenceTriggers(){
  unsigned long now = millis();
  if(now - lastLoadSeqSampleMs < LOAD_SEQ_SAMPLE_INTERVAL_MS) return;
  lastLoadSeqSampleMs = now;
  bool needed[MAX_LOAD] = {false,false,false,false};
  for(int s=0;s<MAX_SEQ;s++){
    if(sequences[s].numSteps==0 || sequences[s].trigSrc!=2) continue;
    int ch = sequences[s].trigCh;
    if(ch>=1 && ch<=MAX_LOAD) needed[ch-1]=true;
  }
  for(int c=0;c<MAX_LOAD;c++){
    if(needed[c]) loadSeqLastG[c] = readLoadGrams(c+1, 3); // -9999=응답없음/미설정/미보정
  }
}
/* 추가 조건(인터록) 판정: 트리거가 일어난 "그 순간"의 디바운스된 입력 상태(inStableState)로 검사한다.
   조건 채널은 "상태(레벨)"만 보므로 버튼을 계속 눌러둔 채여도 되고, 조건이 하나라도 안 맞으면 false.
   등록되지 않은 채널(번호>inCount)이 조건에 있으면 판정할 수 없으므로 안전하게 false(실행 안 함). */
bool seqCondsOk(int s){
  uint32_t need = sequences[s].condOn | sequences[s].condOff;
  if(need == 0) return true;
  for(byte i=0;i<32;i++){
    uint32_t bit = 1UL << i;
    if(!(need & bit)) continue;
    if(i >= inCount) return false;
    byte st = inStableState[i];
    if((sequences[s].condOn  & bit) && st != 1) return false;
    if((sequences[s].condOff & bit) && st != 0) return false;
  }
  return true;
}
void updateInputsAndSequences(){
  unsigned long now = millis();
  for(int i=0;i<inCount;i++){
    int raw = digitalRead(inPins[i]) == LOW ? 1 : 0;
    if(raw != inRawLast[i]){ inRawLast[i] = raw; inLastChangeMs[i] = now; }
    if(now - inLastChangeMs[i] >= DEBOUNCE_MS){ inStableState[i] = raw; }
  }
  sampleUltraForSequenceTriggers();
  sampleLoadForSequenceTriggers();
  bool coolingDown = (now - lastSeqRunMs < SEQ_COOLDOWN_MS); // 방금 실행 직후 노이즈 안정 대기
  for(int s=0;s<MAX_SEQ;s++){
    if(sequences[s].numSteps==0 || sequences[s].trigCh==0) continue;
    int cur;
    if(sequences[s].trigSrc==1){
      // 초음파 거리 트리거: 목표 거리 조건을 만족하면 "1(켜짐)"인 가상 채널처럼 취급해서,
      // 아래 trigCond(OFF/ON/BOTH) 판정 로직을 입력채널 트리거와 완전히 동일하게 재사용한다.
      int ch = sequences[s].trigCh;
      if(ch < 1 || ch > MAX_ULTRA) continue;
      int cm = ultraSeqLastCm[ch-1];
      if(cm < 0) continue; // 아직 측정 전이거나 응답 없음 — 이번 판정은 건너뛰고 이전 상태 유지
      bool hit = (sequences[s].trigDistMode==1) ? (cm >= (int)sequences[s].trigDistCm) : (cm <= (int)sequences[s].trigDistCm);
      cur = hit ? 1 : 0;
    } else if(sequences[s].trigSrc==2){
      // 로드셀 무게 트리거: 초음파와 동일한 방식으로 trigDistMode/trigDistCm(그램)을 재사용
      int ch = sequences[s].trigCh;
      if(ch < 1 || ch > MAX_LOAD) continue;
      long g = loadSeqLastG[ch-1];
      if(g <= -9999) continue; // 아직 측정 전이거나 미설정/미보정 — 이번 판정은 건너뛰고 이전 상태 유지
      bool hit = (sequences[s].trigDistMode==1) ? (g >= (long)sequences[s].trigDistCm) : (g <= (long)sequences[s].trigDistCm);
      cur = hit ? 1 : 0;
    } else {
      int ch = sequences[s].trigCh;
      if(ch < 1 || ch > inCount) continue;
      cur = inStableState[ch-1];
    }
    int target = sequences[s].trigCond; // 0=OFF, 1=ON, 2=BOTH(양쪽 다)
    int prev = seqLastInputState[s];
    if(!coolingDown){
      if(target==2){
        // BOTH: 방향 상관없이 상태가 바뀌는 순간마다 실행(부팅 직후 prev==255일 때는 무시)
        if(prev!=255 && cur!=prev && seqCondsOk(s)) startSequence(s);
      } else if(cur==target && prev!=target){
        if(seqCondsOk(s)) startSequence(s); // 조건이 안 맞으면 이번 트리거는 무시(조건을 맞춘 뒤 다시 눌러야 실행)
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
  Serial.begin(115200); // 웹 프로그램 Baud rate 115200과 일치(USB 유선 연결용)
  Serial1.begin(9600);  // 블루투스 모듈(HC-05/HC-06 등, 하드웨어 핀 18=TX1/19=RX1)용.
                         // 대부분 모듈의 공장 기본 보드레이트가 9600이라 이렇게 맞춤 —
                         // AT 명령으로 모듈 보드레이트를 바꿨다면 이 숫자도 같이 바꿀 것.
                         // 웹 페이지에서 이 포트로 연결할 때도 "통신 속도"를 9600으로 선택해야 함.
  for(int i=0;i<MAX_ACTIVE_SEQ;i++) activeSeqs[i].idx=-1; // 실행 중 슬롯 전부 비움
  Wire.begin(); // 외장 I2C EEPROM(AT24C256) 통신을 위해 loadFromEEPROM()보다 먼저 초기화해야 함
  loadFromEEPROM();
  applyOutPinModes();
  applyInPinModes();
  applyKpPinModes();
  applyStepperPinModes();
  applyUltraPinModes();
  applyLoadPinModes();
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
    if(sequences[i].trigSrc==0 && sequences[i].trigCh>=1 && sequences[i].trigCh<=inCount){
      seqLastInputState[i] = inStableState[sequences[i].trigCh-1];
    } else {
      // 초음파 거리/로드셀 무게 트리거(trigSrc==1/2)는 아직 측정 전이라 255(미초기화)로 시작 —
      // updateInputsAndSequences()의 첫 측정 때 자동으로 채워짐.
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

String rxLine = "";   // USB(Serial)로 들어오는 줄 버퍼
String rxLine1 = "";  // 블루투스(Serial1)로 들어오는 줄 버퍼

void loop(){
  // 1) 시리얼 명령 처리(비블로킹, 한 줄씩) — USB(Serial)와 블루투스(Serial1) 둘 다 감시.
  //    어느 쪽에서 들어온 명령이든 replyPort를 그 포트로 맞춰준 뒤 처리해서, 응답이
  //    명령을 보낸 쪽으로 그대로 돌아가게 한다(엉뚱한 포트로 응답이 새는 것을 방지).
  while(Serial.available() > 0){
    char c = Serial.read();
    if(c == '\n'){
      rxLine.trim();
      if(rxLine.length() > 0){ replyPort = &Serial; handleCommand(rxLine); }
      rxLine = "";
    } else if(c != '\r'){
      rxLine += c;
      if(rxLine.length() > 90) rxLine = ""; // 이상 입력 방지
    }
  }
  while(Serial1.available() > 0){
    char c = Serial1.read();
    if(c == '\n'){
      rxLine1.trim();
      if(rxLine1.length() > 0){ replyPort = &Serial1; handleCommand(rxLine1); }
      rxLine1 = "";
    } else if(c != '\r'){
      rxLine1 += c;
      if(rxLine1.length() > 90) rxLine1 = ""; // 이상 입력 방지
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

  // 5) 스텝모터 펄스 진행(활성 채널만, delay() 없이 한 스텝씩)
  updateSteppers();
}
