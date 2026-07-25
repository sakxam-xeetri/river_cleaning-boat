/*
 * ==================================================================================
 * ESP8266 RIVER CLEANING BOAT FIRMWARE - BY HIMALIXLABS
 * ----------------------------------------------------------------------------------
 * Component Connections:
 * - L298N Motor Driver (Propulsion & Differential Steering):
 *     ENA   -> Pin D1 (GPIO5)  [PWM Motor A Speed]
 *     IN1   -> Pin D2 (GPIO4)  [Motor A Dir 1]
 *     IN2   -> Pin D3 (GPIO0)  [Motor A Dir 2]
 *     IN3   -> Pin D4 (GPIO2)  [Motor B Dir 1]
 *     IN4   -> Pin D5 (GPIO14) [Motor B Dir 2]
 *     ENB   -> Pin D6 (GPIO12) [PWM Motor B Speed]
 *
 * - 1-Channel Relay Module (River Cleaner Motor Switch):
 *     RELAY -> Pin D7 (GPIO13) [Relay Control IN]
 *
 * Features:
 * - Automatically starts an Open Access Point: SSID "RIVER_CLEANER_BOT"
 * - Serves responsive landscape dark/red web controller interface at 192.168.4.1
 * - Includes Captive Portal support for instant auto-popup on mobile devices
 * - Safety Auto-Stop fail-safe timer (stops motors if connection drops >3 seconds)
 * ==================================================================================
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>

// ----------------------------------------------------------------------------------
// PIN DEFINITIONS (SERIAL D1 TO D7 MAPPING)
// ----------------------------------------------------------------------------------
const int PIN_ENA   = D1; // GPIO5  - Motor A PWM Speed
const int PIN_IN1   = D2; // GPIO4  - Motor A Direction 1
const int PIN_IN2   = D3; // GPIO0  - Motor A Direction 2
const int PIN_IN3   = D4; // GPIO2  - Motor B Direction 1
const int PIN_IN4   = D5; // GPIO14 - Motor B Direction 2
const int PIN_ENB   = D6; // GPIO12 - Motor B PWM Speed
const int PIN_RELAY = D7; // GPIO13 - Cleaner Motor Relay

// Relay logic: Active LOW for most relay modules
#define RELAY_ACTIVE_STATE LOW
#define RELAY_OFF_STATE    HIGH

// ----------------------------------------------------------------------------------
// GLOBAL STATE & OBJECTS
// ----------------------------------------------------------------------------------
ESP8266WebServer server(80);
DNSServer dnsServer;

String currentDirection = "STOP";
int currentSpeed        = 200; // Default PWM (0 - 255)
bool cleanerState       = false;

unsigned long lastCommandTime = 0;
const unsigned long FAILSAFE_TIMEOUT_MS = 3000; // 3 Seconds Auto-Stop Safety

// SoftAP Config
const char* AP_SSID = "RIVER_CLEANER_BOT";

// ----------------------------------------------------------------------------------
// EMBEDDED WEB CONTROLLER HTML (MATCHING REFERENCE DIAGRAM & HIMALIXLABS BRANDING)
// ----------------------------------------------------------------------------------
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <title>River Cleaning Boat by HimalixLabs</title>
  <style>
    @import url('https://fonts.googleapis.com/css2?family=Orbitron:wght@600;800;900&family=Rajdhani:wght@600;700&display=swap');
    :root {
      --bg-main: #050507; --bg-card: #0c0c12; --bg-card-hover: #13131c; --bg-inset: #030305;
      --red-primary: #ff003c; --red-bright: #ff2a55; --red-dim: #33000b; --red-glow: rgba(255,0,60,0.45);
      --text-main: #f0f0f5; --text-muted: #8c8ca3; --text-dim: #505066; --border-color: #222230;
      --font-heading: 'Orbitron', sans-serif; --font-body: 'Rajdhani', sans-serif;
    }
    *, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; border-radius: 0 !important; user-select: none; touch-action: manipulation; }
    html, body { width: 100vw; height: 100vh; overflow: hidden; background-color: var(--bg-main); color: var(--text-main); font-family: var(--font-body); font-size: 16px; background-image: radial-gradient(circle at 50% 10%, rgba(255, 0, 60, 0.08) 0%, transparent 60%), linear-gradient(rgba(255, 0, 60, 0.025) 1px, transparent 1px), linear-gradient(90deg, rgba(255, 0, 60, 0.025) 1px, transparent 1px); background-size: 100% 100%, 24px 24px, 24px 24px; }
    .app-container { display: grid; grid-template-rows: auto 1fr auto auto; width: 100vw; height: 100vh; padding: 12px 16px; gap: 12px; }
    .top-header-wrapper { display: flex; justify-content: center; align-items: center; position: relative; }
    .brand-box { background-color: var(--bg-card); border: 2px solid var(--red-primary); box-shadow: 0 0 20px var(--red-glow); padding: 10px 36px; text-align: center; position: relative; }
    .brand-box::before, .brand-box::after { content: ''; position: absolute; width: 8px; height: 8px; border-style: solid; border-color: #fff; }
    .brand-box::before { top: -2px; left: -2px; border-width: 2px 0 0 2px; }
    .brand-box::after { bottom: -2px; right: -2px; border-width: 0 2px 2px 0; }
    .brand-box h1 { font-family: var(--font-heading); font-size: 1.3rem; font-weight: 900; letter-spacing: 3px; text-transform: uppercase; }
    .brand-box h1 span { color: var(--red-primary); }
    .brand-sub { font-family: var(--font-heading); font-size: 0.75rem; letter-spacing: 4px; color: var(--red-bright); text-transform: uppercase; margin-top: 2px; }
    .hud-status-badge { position: absolute; right: 0; top: 50%; transform: translateY(-50%); display: flex; align-items: center; gap: 8px; background-color: var(--bg-card); border: 1px solid var(--border-color); padding: 6px 12px; font-size: 0.75rem; font-family: monospace; }
    .status-dot { width: 8px; height: 8px; background-color: var(--red-primary); box-shadow: 0 0 8px var(--red-primary); }
    .main-dashboard { display: grid; grid-template-columns: 1fr 1fr; gap: 16px; height: 100%; }
    .panel-card { background-color: var(--bg-card); border: 1px solid var(--border-color); padding: 16px; display: flex; flex-direction: column; position: relative; }
    .panel-card::before { content: ''; position: absolute; top: 0; left: 0; width: 4px; height: 100%; background-color: var(--red-primary); }
    .panel-header-label { font-family: var(--font-heading); font-size: 0.9rem; font-weight: 800; letter-spacing: 2px; color: var(--red-primary); text-transform: uppercase; margin-bottom: 12px; display: flex; align-items: center; justify-content: space-between; border-bottom: 1px solid var(--border-color); padding-bottom: 6px; }
    .panel-tag { font-size: 0.65rem; font-family: monospace; color: var(--text-dim); }
    .controller-wrapper { flex: 1; display: flex; justify-content: center; align-items: center; }
    .dpad-cross { display: grid; grid-template-columns: repeat(3, 76px); grid-template-rows: repeat(3, 76px); gap: 8px; }
    .btn-control { background-color: var(--bg-inset); border: 1px solid var(--border-color); color: var(--text-main); font-family: var(--font-heading); font-size: 1.1rem; font-weight: 700; cursor: pointer; display: flex; flex-direction: column; justify-content: center; align-items: center; transition: all 0.1s ease; }
    .btn-control .key-hint { font-size: 0.6rem; font-family: monospace; color: var(--text-dim); margin-top: 2px; }
    .btn-control:hover { background-color: var(--bg-card-hover); border-color: var(--red-primary); color: var(--red-bright); }
    .btn-control:active, .btn-control.active { background-color: var(--red-primary); border-color: var(--red-bright); color: #fff; box-shadow: 0 0 18px var(--red-glow); transform: scale(0.96); }
    .btn-control.btn-center-stop { background-color: var(--red-dim); border: 1px solid var(--red-primary); color: var(--red-bright); }
    .cleaner-panel-content { flex: 1; display: flex; flex-direction: column; justify-content: center; align-items: center; gap: 16px; }
    .cleaner-status-box { text-align: center; background-color: var(--bg-inset); border: 1px solid var(--border-color); padding: 16px; width: 100%; max-width: 280px; }
    .cleaner-status-title { font-family: var(--font-heading); font-size: 0.8rem; color: var(--text-muted); letter-spacing: 1.5px; }
    .cleaner-status-state { font-family: var(--font-heading); font-size: 1.3rem; font-weight: 900; color: var(--text-dim); margin-top: 4px; letter-spacing: 2px; }
    .cleaner-status-state.active { color: var(--red-bright); text-shadow: 0 0 10px var(--red-glow); }
    .btn-cleaner-main { background-color: var(--bg-card); border: 2px solid var(--border-color); color: var(--text-main); font-family: var(--font-heading); font-size: 1.1rem; font-weight: 900; letter-spacing: 2px; padding: 20px 32px; width: 100%; max-width: 280px; cursor: pointer; text-transform: uppercase; transition: all 0.15s ease; }
    .btn-cleaner-main:hover { border-color: var(--red-primary); color: var(--red-bright); background-color: var(--red-dim); }
    .btn-cleaner-main.active { background-color: var(--red-primary); border-color: var(--red-bright); color: #fff; box-shadow: 0 0 25px var(--red-glow); }
    .speed-panel-card { background-color: var(--bg-card); border: 1px solid var(--border-color); padding: 14px 20px; display: flex; flex-direction: column; gap: 8px; position: relative; }
    .speed-panel-card::before { content: ''; position: absolute; bottom: 0; left: 0; width: 100%; height: 3px; background-color: var(--red-primary); }
    .speed-header-row { display: flex; justify-content: space-between; align-items: center; }
    .speed-label { font-family: var(--font-heading); font-size: 0.85rem; font-weight: 800; letter-spacing: 2px; color: var(--red-primary); text-transform: uppercase; }
    .speed-readout { font-family: monospace; font-size: 1.2rem; font-weight: 700; color: var(--red-bright); }
    .speed-slider-wrapper { display: flex; align-items: center; gap: 16px; }
    input[type="range"].speed-slider { -webkit-appearance: none; flex: 1; height: 20px; background: var(--bg-inset); border: 1px solid var(--border-color); outline: none; }
    input[type="range"].speed-slider::-webkit-slider-thumb { -webkit-appearance: none; width: 28px; height: 32px; background: var(--red-primary); border: 1px solid var(--red-bright); cursor: pointer; }
    .speed-presets-group { display: flex; gap: 8px; }
    .btn-speed-step { background-color: var(--bg-inset); border: 1px solid var(--border-color); color: var(--text-muted); font-family: monospace; font-size: 0.75rem; padding: 6px 12px; cursor: pointer; }
    .btn-speed-step:hover, .btn-speed-step.active { border-color: var(--red-primary); color: var(--red-bright); background-color: var(--red-dim); }
    .footer-bar { display: flex; justify-content: space-between; align-items: center; background-color: var(--bg-card); border: 1px solid var(--border-color); padding: 4px 12px; font-family: monospace; font-size: 0.75rem; color: var(--text-muted); }
    .log-msg { color: var(--red-bright); }
    .hotkeys { display: flex; gap: 12px; color: var(--text-dim); }
    .kbd { color: var(--text-main); background-color: var(--bg-inset); border: 1px solid var(--border-color); padding: 0 4px; }
  </style>
</head>
<body>
  <div class="app-container">
    <header class="top-header-wrapper">
      <div class="brand-box">
        <h1>River Cleaning <span>Boat</span></h1>
        <div class="brand-sub">by HimalixLabs</div>
      </div>
      <div class="hud-status-badge">
        <span class="status-dot"></span>
        <span>ONLINE AP</span>
      </div>
    </header>
    <main class="main-dashboard">
      <section class="panel-card">
        <div class="panel-header-label"><span>CONTROLLER</span><span class="panel-tag">STEERING</span></div>
        <div class="controller-wrapper">
          <div class="dpad-cross">
            <div style="visibility:hidden;"></div>
            <button class="btn-control" data-dir="FORWARD"><span>▲</span><span class="key-hint">FWD [W]</span></button>
            <div style="visibility:hidden;"></div>
            <button class="btn-control" data-dir="LEFT"><span>◀</span><span class="key-hint">LEFT [A]</span></button>
            <button class="btn-control btn-center-stop" data-dir="STOP"><span>■</span><span class="key-hint">STOP</span></button>
            <button class="btn-control" data-dir="RIGHT"><span>▶</span><span class="key-hint">RIGHT [D]</span></button>
            <div style="visibility:hidden;"></div>
            <button class="btn-control" data-dir="BACKWARD"><span>▼</span><span class="key-hint">REV [S]</span></button>
            <div style="visibility:hidden;"></div>
          </div>
        </div>
      </section>
      <section class="panel-card">
        <div class="panel-header-label"><span>CLEANER ON / OFF</span><span class="panel-tag">RELAY MODULE</span></div>
        <div class="cleaner-panel-content">
          <div class="cleaner-status-box">
            <div class="cleaner-status-title">MOTOR RELAY STATE</div>
            <div class="cleaner-status-state" id="cleaner-state-text">OFF</div>
          </div>
          <button class="btn-cleaner-main" id="cleaner-toggle-btn">CLEANER ON / OFF</button>
        </div>
      </section>
    </main>
    <section class="speed-panel-card">
      <div class="speed-header-row">
        <span class="speed-label">SPEED CONTROLLER</span>
        <span class="speed-readout" id="speed-val-display">78%</span>
      </div>
      <div class="speed-slider-wrapper">
        <input type="range" min="0" max="255" value="200" class="speed-slider" id="speed-slider">
        <div class="speed-presets-group">
          <button class="btn-speed-step" data-speed="64">25%</button>
          <button class="btn-speed-step" data-speed="128">50%</button>
          <button class="btn-speed-step active" data-speed="192">75%</button>
          <button class="btn-speed-step" data-speed="255">100%</button>
        </div>
      </div>
    </section>
    <footer class="footer-bar">
      <div class="log-msg" id="log-output">HimalixLabs River Cleaner AP Connected (192.168.4.1)</div>
      <div class="hotkeys"><span><span class="kbd">WASD</span> Steer</span><span><span class="kbd">SPACE</span> Stop</span><span><span class="kbd">C</span> Cleaner</span></div>
    </footer>
  </div>
  <script>
    (function(){
      let state = { direction: 'STOP', speed: 200, cleanerOn: false };
      const el = {
        speedVal: document.getElementById('speed-val-display'),
        speedSlider: document.getElementById('speed-slider'),
        cleanerBtn: document.getElementById('cleaner-toggle-btn'),
        cleanerText: document.getElementById('cleaner-state-text'),
        log: document.getElementById('log-output'),
        controls: document.querySelectorAll('.btn-control'),
        presets: document.querySelectorAll('.btn-speed-step')
      };
      function updateUI(){
        el.speedVal.textContent = Math.round((state.speed/255)*100) + '%';
        el.speedSlider.value = state.speed;
        if(state.cleanerOn){ el.cleanerBtn.classList.add('active'); el.cleanerText.classList.add('active'); el.cleanerText.textContent = 'ACTIVE (ON)'; }
        else { el.cleanerBtn.classList.remove('active'); el.cleanerText.classList.remove('active'); el.cleanerText.textContent = 'OFF'; }
        el.controls.forEach(b => b.classList.toggle('active', b.dataset.dir === state.direction));
        el.presets.forEach(b => b.classList.toggle('active', parseInt(b.dataset.speed) === state.speed));
      }
      function sendMove(dir, spd){
        state.direction = dir || state.direction;
        if(spd !== undefined) state.speed = spd;
        updateUI();
        fetch('/move?dir=' + state.direction + '&speed=' + state.speed).then(r=>r.text()).catch(e=>{});
        el.log.textContent = 'COMMAND: ' + state.direction + ' @ ' + Math.round((state.speed/255)*100) + '%';
      }
      function sendCleaner(){
        state.cleanerOn = !state.cleanerOn;
        updateUI();
        fetch('/cleaner?state=' + (state.cleanerOn ? 'on' : 'off')).then(r=>r.text()).catch(e=>{});
        el.log.textContent = 'CLEANER RELAY: ' + (state.cleanerOn ? 'ON' : 'OFF');
      }
      el.controls.forEach(b => {
        const handler = (e) => { e.preventDefault(); sendMove(b.dataset.dir); };
        b.addEventListener('mousedown', handler);
        b.addEventListener('touchstart', handler);
      });
      const stopHandler = () => { if(state.direction !== 'STOP') sendMove('STOP'); };
      window.addEventListener('mouseup', stopHandler);
      window.addEventListener('touchend', stopHandler);
      el.speedSlider.addEventListener('input', (e) => sendMove(state.direction, parseInt(e.target.value)));
      el.presets.forEach(b => b.addEventListener('click', () => sendMove(state.direction, parseInt(b.dataset.speed))));
      el.cleanerBtn.addEventListener('click', sendCleaner);
      window.addEventListener('keydown', (e) => {
        if(e.repeat) return;
        if(e.code === 'KeyW' || e.code === 'ArrowUp') sendMove('FORWARD');
        else if(e.code === 'KeyS' || e.code === 'ArrowDown') sendMove('BACKWARD');
        else if(e.code === 'KeyA' || e.code === 'ArrowLeft') sendMove('LEFT');
        else if(e.code === 'KeyD' || e.code === 'ArrowRight') sendMove('RIGHT');
        else if(e.code === 'Space') sendMove('STOP');
        else if(e.code === 'KeyC') sendCleaner();
      });
      window.addEventListener('keyup', (e) => {
        if(['KeyW','KeyS','KeyA','KeyD','ArrowUp','ArrowDown','ArrowLeft','ArrowRight'].includes(e.code)){
          sendMove('STOP');
        }
      });
    })();
  </script>
</body>
</html>
)rawliteral";

// ----------------------------------------------------------------------------------
// MOTOR DRIVER HELPER FUNCTIONS
// ----------------------------------------------------------------------------------

void stopMotors() {
  analogWrite(PIN_ENA, 0);
  analogWrite(PIN_ENB, 0);
  digitalWrite(PIN_IN1, LOW);
  digitalWrite(PIN_IN2, LOW);
  digitalWrite(PIN_IN3, LOW);
  digitalWrite(PIN_IN4, LOW);
  currentDirection = "STOP";
}

void moveForward(int speed) {
  digitalWrite(PIN_IN1, HIGH);
  digitalWrite(PIN_IN2, LOW);
  digitalWrite(PIN_IN3, HIGH);
  digitalWrite(PIN_IN4, LOW);
  analogWrite(PIN_ENA, speed);
  analogWrite(PIN_ENB, speed);
  currentDirection = "FORWARD";
}

void moveBackward(int speed) {
  digitalWrite(PIN_IN1, LOW);
  digitalWrite(PIN_IN2, HIGH);
  digitalWrite(PIN_IN3, LOW);
  digitalWrite(PIN_IN4, HIGH);
  analogWrite(PIN_ENA, speed);
  analogWrite(PIN_ENB, speed);
  currentDirection = "BACKWARD";
}

void turnLeft(int speed) {
  int slowSpeed = speed * 0.3;
  digitalWrite(PIN_IN1, HIGH);
  digitalWrite(PIN_IN2, LOW);
  digitalWrite(PIN_IN3, HIGH);
  digitalWrite(PIN_IN4, LOW);
  analogWrite(PIN_ENA, slowSpeed);
  analogWrite(PIN_ENB, speed);
  currentDirection = "LEFT";
}

void turnRight(int speed) {
  int slowSpeed = speed * 0.3;
  digitalWrite(PIN_IN1, HIGH);
  digitalWrite(PIN_IN2, LOW);
  digitalWrite(PIN_IN3, HIGH);
  digitalWrite(PIN_IN4, LOW);
  analogWrite(PIN_ENA, speed);
  analogWrite(PIN_ENB, slowSpeed);
  currentDirection = "RIGHT";
}

void setCleanerState(bool turnOn) {
  cleanerState = turnOn;
  digitalWrite(PIN_RELAY, turnOn ? RELAY_ACTIVE_STATE : RELAY_OFF_STATE);
}

// ----------------------------------------------------------------------------------
// WEB SERVER ROUTE HANDLERS
// ----------------------------------------------------------------------------------

void handleRoot() {
  server.send(200, "text/html", INDEX_HTML);
}

void handleMove() {
  lastCommandTime = millis();
  
  if (server.hasArg("dir")) {
    String dir = server.arg("dir");
    if (server.hasArg("speed")) {
      currentSpeed = constrain(server.arg("speed").toInt(), 0, 255);
    }
    
    if (dir == "FORWARD") moveForward(currentSpeed);
    else if (dir == "BACKWARD") moveBackward(currentSpeed);
    else if (dir == "LEFT") turnLeft(currentSpeed);
    else if (dir == "RIGHT") turnRight(currentSpeed);
    else stopMotors();
    
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Missing Parameters");
  }
}

void handleCleaner() {
  if (server.hasArg("state")) {
    String st = server.arg("state");
    if (st == "on" || st == "1" || st == "true") {
      setCleanerState(true);
    } else {
      setCleanerState(false);
    }
    server.send(200, "text/plain", cleanerState ? "ON" : "OFF");
  } else {
    setCleanerState(!cleanerState);
    server.send(200, "text/plain", cleanerState ? "ON" : "OFF");
  }
}

void handleStatus() {
  String json = "{";
  json += "\"direction\":\"" + currentDirection + "\",";
  json += "\"speed\":" + String(currentSpeed) + ",";
  json += "\"cleaner\":" + String(cleanerState ? "true" : "false");
  json += "}";
  server.send(200, "application/json", json);
}

void handleNotFound() {
  server.sendHeader("Location", String("http://192.168.4.1/"), true);
  server.send(302, "text/plain", "");
}

// ----------------------------------------------------------------------------------
// SETUP & MAIN LOOP
// ----------------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  Serial.println("\n[INIT] Starting HimalixLabs River Cleaner Firmware...");

  pinMode(PIN_ENA, OUTPUT);
  pinMode(PIN_IN1, OUTPUT);
  pinMode(PIN_IN2, OUTPUT);
  pinMode(PIN_IN3, OUTPUT);
  pinMode(PIN_IN4, OUTPUT);
  pinMode(PIN_ENB, OUTPUT);
  pinMode(PIN_RELAY, OUTPUT);

  stopMotors();
  setCleanerState(false);

  WiFi.mode(WIFI_AP);
  IPAddress local_ip(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);
  
  WiFi.softAPConfig(local_ip, gateway, subnet);
  
  if (WiFi.softAP(AP_SSID)) {
    Serial.print("[WiFi] Open Access Point Started: ");
    Serial.println(AP_SSID);
    Serial.print("[WiFi] AP IP Address: ");
    Serial.println(WiFi.softAPIP());
  }

  dnsServer.start(53, "*", local_ip);

  server.on("/", handleRoot);
  server.on("/move", handleMove);
  server.on("/cleaner", handleCleaner);
  server.on("/status", handleStatus);
  server.on("/generate_204", handleRoot);
  server.on("/fwlink", handleRoot);
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("[HTTP] Server listening on port 80");
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();

  if (currentDirection != "STOP" && (millis() - lastCommandTime > FAILSAFE_TIMEOUT_MS)) {
    Serial.println("[SAFETY] Fail-safe timeout triggered! Auto-stopping motors.");
    stopMotors();
  }
}
