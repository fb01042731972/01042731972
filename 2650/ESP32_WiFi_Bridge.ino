/* =========================================================
   ESP32_WiFi_Bridge.ino
   Mega2560_16Relay_Firmware_keypad.ino 전용 "와이파이 ↔ 시리얼" 브리지

   역할: 이 스케치는 릴레이/시퀀스 로직을 전혀 모른다. 그냥
     "웹 프로그램(WebSocket)에서 받은 글자를 그대로 메가2560의
      Serial2로 전달"하고, "메가2560 Serial2에서 온 글자를 그대로
      웹 프로그램(WebSocket)에 돌려주는" 양방향 통로 역할만 한다.
     즉 메가2560 입장에서는 USB로 연결했을 때와 완전히 동일하게 보인다.

   준비물: ESP32 개발보드 1개(ESP32 DevKit / NodeMCU-32S 등 아무 것이나 OK.
     ESP8266은 하드웨어 UART가 1개뿐이라 USB 디버그와 겹치므로 권장하지 않음 —
     ESP32는 UART가 3개라 Serial(USB, 디버그용)과 Serial2(메가2560 연결용)를
     동시에 따로 쓸 수 있어서 이 용도에 적합함)

   ★배선 — 반드시 레벨 변환(메가2560은 5V 로직, ESP32는 3.3V 로직):
     메가2560 GND        -- ESP32 GND                (공통 접지, 필수)
     메가2560 TX2(핀 16) -- [저항 분배기로 3.3V로 낮춰서] -- ESP32 RX2(GPIO16)
        예: 메가 TX2 -- 2kΩ -- ESP32 RX2 -- 1kΩ -- GND  (5V*1k/(2k+1k)≈1.7V, 3.3V 로직 HIGH 인식 범위)
     메가2560 RX2(핀 17) -- ESP32 TX2(GPIO17)          (ESP32의 3.3V 출력은 메가2560이
                                                          HIGH로 인식하므로 직결 가능)
   ※ 저항 분배기 없이 직결하면 ESP32의 RX2 핀이 5V를 받아 손상될 수 있음(전용 레벨
     시프터 모듈이 있다면 그걸 써도 됨 — 저항 분배기는 가장 저렴한 임시 방편).

   ★보안/네트워크 주의:
     - 이 브리지는 같은 와이파이(같은 공유기) 안에서만 접속되도록 설계되어 있고,
       WebSocket 통신 자체에는 비밀번호·암호화가 없다(누구든 그 IP·포트를 알면
       릴레이를 조작할 수 있음). 절대로 공유기 포트포워딩 등으로 인터넷에 직접
       노출하지 말 것 — 정말 외부(멀리)에서 쓰고 싶다면 회사/집 네트워크로 들어오는
       VPN을 통해 접속하는 방식을 권장한다(직접 노출보다 훨씬 안전).
     - 따라서 기본적으로는 "같은 와이파이 안에서의 무선 조작"용이며, 그 자체로는
       인터넷 너머 먼 곳에서의 접속을 지원하지 않는다.

   ★관리자 셀프 설정(오작동/와이파이 변경 대응): 와이파이 이름·비밀번호가 바뀌었거나
     잘못 입력해서 연결이 안 될 때, 코드를 다시 업로드할 필요 없이 브라우저에서
     http://ESP32의IP주소/wifi 로 들어가면 새 와이파이 정보를 입력해 저장할 수 있다
     (저장하면 자동 재시작). 접속할 IP를 모르면(네트워크가 바뀌어 예전 IP를 못 찾을 때),
     아래 WIFI_CONNECT_TIMEOUT_MS 안에 접속을 못 하는 경우 자동으로 만들어지는 임시
     핫스팟(AP_SSID/AP_PASSWORD)에 휴대폰으로 먼저 접속한 뒤 http://192.168.4.1/wifi 로
     들어가서 새 와이파이 정보를 넣고 저장하면 된다. 저장된 값은 ESP32 내부 저장소
     (Preferences/NVS)에 남아있어서 전원을 껐다 켜도 유지된다.

   라이브러리: Arduino IDE 라이브러리 매니저에서 "WebSockets" (Markus Sattler,
     links2004/arduinoWebSockets) 설치 필요. WiFi/WebServer/Preferences는 ESP32
     보드 패키지에 내장되어 있어 별도 설치가 필요 없음.

   사용법:
     1) 아래 WIFI_SSID_DEFAULT / WIFI_PASSWORD_DEFAULT를 집/공장 와이파이 정보로 1차 수정
        (이후에는 코드를 고치지 않고도 /wifi 페이지에서 바꿀 수 있음)
     2) 이 스케치를 ESP32에 업로드 (USB로, 메가2560과는 별개로 컴퓨터에 연결)
     3) 시리얼 모니터(115200)로 접속하면 ESP32가 할당받은 IP 주소를 출력함
        (와이파이 접속에 실패하면 AP_FALLBACK 모드로 자동 전환 — 이땐 휴대폰/PC로
         ESP32가 직접 만든 와이파이(AP_SSID)에 접속해서 확인)
     4) ESP32에 업로드가 끝나면 USB를 뽑고 배선대로 메가2560과 연결, 외부 전원 공급
     5) 웹 프로그램의 "1. 연결" 화면에서 이 IP 주소를 입력하고 "와이파이 연결" 클릭
   =========================================================*/

#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <Preferences.h>

/* ---------------- 와이파이 접속 정보 ----------------
   최초 1회(=Preferences에 아직 저장된 값이 없을 때)만 쓰이는 "공장 기본값".
   그 이후로는 /wifi 설정 페이지에서 저장한 값이 우선 적용되며, 코드를 고치지
   않아도 계속 유지된다(전원을 꺼도 안 지워짐). */
const char* WIFI_SSID_DEFAULT     = "여기에_와이파이_이름";
const char* WIFI_PASSWORD_DEFAULT = "여기에_와이파이_비밀번호";
const unsigned long WIFI_CONNECT_TIMEOUT_MS = 15000; // 이 시간 안에 접속 안 되면 AP 모드로 전환

/* 와이파이 접속 실패 시 대신 켜지는 "직접 접속용" AP(핫스팟) 정보.
   공유기 없이도, 또는 공유기 정보를 몰라서 재설정이 필요할 때도 휴대폰/PC를
   이 AP에 직접 붙여서 /wifi 설정 페이지로 들어갈 수 있음. */
const char* AP_SSID     = "MegaRelay-WiFi";
const char* AP_PASSWORD = "12345678"; // 8자 이상 필요(WPA2 최소 길이)

/* ---------------- 통신 포트 ---------------- */
// Serial  (USB, GPIO1=TX0/GPIO3=RX0) : 디버그 로그 전용 (컴퓨터로 상태 확인할 때만 사용)
// Serial2 (GPIO17=TX2/GPIO16=RX2)    : 메가2560과의 연결 전용 (위 배선 참고)
WebSocketsServer webSocket(81); // 웹 프로그램(제어 화면)이 접속할 WebSocket 포트(기본 81)
WebServer configServer(80);     // 관리자가 /wifi로 들어와 와이파이를 재설정하는 설정용 웹서버(포트 80)
Preferences prefs;              // 와이파이 SSID/비밀번호를 전원 꺼도 유지되게 저장(NVS)

bool apMode = false;            // 지금 AP(핫스팟) 모드로 떠 있는지
String currentSsid, currentPass;

/* 메가2560 → (Serial2로 들어온 바이트) → 모든 WebSocket 클라이언트에게 그대로 전달 */
void pumpMegaToWebSocket(){
  static uint8_t buf[256];
  int n = 0;
  while(Serial2.available() > 0 && n < (int)sizeof(buf)){
    buf[n++] = (uint8_t)Serial2.read();
  }
  if(n > 0){
    webSocket.broadcastTXT(buf, n);
  }
}

/* WebSocket에서 들어온 바이트(웹 프로그램이 보낸 명령) → 그대로 메가2560의 Serial2로 전달 */
void webSocketEvent(uint8_t clientNum, WStype_t type, uint8_t* payload, size_t length){
  switch(type){
    case WStype_CONNECTED:
      Serial.printf("[WS] 클라이언트 #%u 연결됨\n", clientNum);
      break;
    case WStype_DISCONNECTED:
      Serial.printf("[WS] 클라이언트 #%u 연결 해제\n", clientNum);
      break;
    case WStype_TEXT:
    case WStype_BIN:
      Serial2.write(payload, length); // 받은 그대로(줄바꿈 포함) 메가2560에 전달
      break;
    default:
      break;
  }
}

/* HTML 속성 값에 안전하게 넣기 위한 아주 단순한 이스케이프(따옴표/꺾쇠만 처리) */
String htmlEscape(const String &s){
  String out; out.reserve(s.length());
  for(size_t i=0;i<s.length();i++){
    char c=s[i];
    if(c=='&') out+="&amp;";
    else if(c=='"') out+="&quot;";
    else if(c=='<') out+="&lt;";
    else if(c=='>') out+="&gt;";
    else out+=c;
  }
  return out;
}

/* ---------------- 관리자용 와이파이 설정 페이지 (http://IP/ 또는 http://IP/wifi) ----------------
   오작동/네트워크 변경 시 ESP32를 다시 프로그래밍하지 않고도 여기서 바로 고칠 수 있게 하는 페이지. */
void handleWifiPage(){
  String html;
  html += "<!DOCTYPE html><html lang='ko'><head><meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
  html += "<title>와이파이 브리지 설정</title><style>";
  html += "body{font-family:Arial,sans-serif;background:#f3f4f6;margin:0;padding:20px}";
  html += ".card{background:#fff;border-radius:10px;padding:18px;max-width:420px;margin:0 auto;box-shadow:0 2px 8px #0001}";
  html += "h2{margin-top:0;font-size:18px}";
  html += "label{font-weight:bold;display:block;margin-top:12px;font-size:14px}";
  html += "input{width:100%;box-sizing:border-box;padding:9px;margin-top:4px;border:1px solid #cbd5e1;border-radius:7px;font-size:15px}";
  html += "button{margin-top:16px;width:100%;padding:10px;background:#2563eb;color:#fff;border:0;border-radius:7px;font-size:15px}";
  html += ".status{background:#eef2ff;border-radius:7px;padding:10px;margin-bottom:14px;font-size:13px;line-height:1.6}";
  html += ".warn{background:#fef2f2;border:1px solid #fecaca;color:#991b1b;border-radius:7px;padding:10px;margin-top:14px;font-size:12.5px;line-height:1.5}";
  html += "</style></head><body><div class='card'>";
  html += "<h2>ESP32 와이파이 브리지 설정</h2>";
  html += "<div class='status'>";
  html += String("현재 모드: ") + (apMode ? "직접 접속용 AP(핫스팟) 모드" : "와이파이(공유기) 접속 모드") + "<br>";
  html += String("접속된 네트워크: ") + (apMode ? String(AP_SSID) : WiFi.SSID()) + "<br>";
  html += String("IP 주소: ") + (apMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString()) + "<br>";
  html += String("현재 저장된 SSID: ") + htmlEscape(currentSsid);
  html += "</div>";
  html += "<form method='POST' action='/save'>";
  html += "<label>새 와이파이 이름(SSID)</label>";
  html += "<input name='ssid' value='" + htmlEscape(currentSsid) + "' placeholder='집/공장 와이파이 이름'>";
  html += "<label>새 와이파이 비밀번호</label>";
  html += "<input name='pass' type='password' placeholder='바꿀 때만 입력 (비워두면 기존 비밀번호 유지)'>";
  html += "<button type='submit'>저장하고 재시작</button>";
  html += "</form>";
  html += "<div class='warn'>저장하면 ESP32가 곧바로 재시작되어 새 와이파이로 접속을 시도합니다. ";
  html += String("접속에 실패하면 약 ") + String(WIFI_CONNECT_TIMEOUT_MS/1000) + "초 뒤 자동으로 AP 모드(";
  html += String(AP_SSID) + ", 비밀번호 " + String(AP_PASSWORD) + ")로 다시 전환되니, ";
  html += "그 핫스팟에 휴대폰으로 접속해서 http://192.168.4.1/wifi 로 다시 들어와 재설정하면 됩니다.</div>";
  html += "</div></body></html>";
  configServer.send(200, "text/html; charset=utf-8", html);
}

/* /save : 새 SSID/비밀번호를 저장(NVS)하고 재시작해서 새 설정으로 재접속 시도 */
void handleWifiSave(){
  if(!configServer.hasArg("ssid") || configServer.arg("ssid").length()==0){
    configServer.send(400, "text/plain; charset=utf-8", "SSID를 입력하세요");
    return;
  }
  String newSsid = configServer.arg("ssid");
  String newPass = configServer.hasArg("pass") ? configServer.arg("pass") : "";
  prefs.putString("ssid", newSsid);
  if(newPass.length() > 0) prefs.putString("pass", newPass); // 비워두면 기존 비밀번호 유지
  String html = "<!DOCTYPE html><html lang='ko'><head><meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'></head>"
    "<body style='font-family:Arial,sans-serif;padding:24px'>저장했습니다. 곧 재시작하여 "
    "새 와이파이로 접속을 시도합니다...</body></html>";
  configServer.send(200, "text/html; charset=utf-8", html);
  delay(1200);
  ESP.restart();
}

/* 저장된(또는 공장 기본) SSID/비밀번호로 와이파이 접속을 시도하고, 실패하면 AP 모드로 전환 */
void connectWifiOrFallbackToAP(){
  Serial.print("와이파이 접속 시도: "); Serial.println(currentSsid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(currentSsid.c_str(), currentPass.c_str());
  unsigned long t0 = millis();
  while(WiFi.status() != WL_CONNECTED && millis() - t0 < WIFI_CONNECT_TIMEOUT_MS){
    delay(300);
    Serial.print(".");
  }
  Serial.println();

  if(WiFi.status() == WL_CONNECTED){
    apMode = false;
    Serial.println("✅ 와이파이 접속 성공!");
    Serial.print("▶ 웹 프로그램에 입력할 IP 주소: "); Serial.println(WiFi.localIP());
    Serial.print("▶ 와이파이가 잘못되면 재설정: http://"); Serial.print(WiFi.localIP()); Serial.println("/wifi");
  } else {
    apMode = true;
    Serial.println("⚠ 와이파이 접속 실패 — 직접 접속용 AP 모드로 전환합니다.");
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASSWORD);
    Serial.print("▶ 이 와이파이(핫스팟)에 먼저 접속하세요: "); Serial.println(AP_SSID);
    Serial.print("▶ 비밀번호: "); Serial.println(AP_PASSWORD);
    Serial.print("▶ 재설정 페이지: http://"); Serial.print(WiFi.softAPIP()); Serial.println("/wifi");
  }
}

void setup(){
  Serial.begin(115200);      // USB 디버그 로그용
  Serial2.begin(115200, SERIAL_8N1, 16, 17); // GPIO16=RX2, GPIO17=TX2, 메가2560과 반드시 같은 속도(115200)
  delay(200);

  Serial.println();
  Serial.println("=== ESP32 와이파이 브리지 시작 ===");

  prefs.begin("wifi_cfg", false); // NVS에 저장된 이전 설정 불러오기(없으면 공장 기본값 사용)
  currentSsid = prefs.getString("ssid", WIFI_SSID_DEFAULT);
  currentPass = prefs.getString("pass", WIFI_PASSWORD_DEFAULT);

  connectWifiOrFallbackToAP();

  configServer.on("/", handleWifiPage);
  configServer.on("/wifi", handleWifiPage);
  configServer.on("/save", HTTP_POST, handleWifiSave);
  configServer.begin();
  Serial.println("▶ 관리자 설정 페이지 시작됨 (포트 80, /wifi)");

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  Serial.println("▶ WebSocket 서버 시작됨 (포트 81, 제어 화면용)");
  Serial.println("=== 준비 완료 — 이제 메가2560을 배선하고 웹 프로그램에서 연결하세요 ===");
}

void loop(){
  webSocket.loop();
  configServer.handleClient();
  pumpMegaToWebSocket();
}
