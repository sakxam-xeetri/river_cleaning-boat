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
#include <ArduinoOTA.h>
#include <ESP8266HTTPUpdateServer.h>

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
ESP8266HTTPUpdateServer httpUpdater;
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
    @import url('https://fonts.googleapis.com/css2?family=Plus+Jakarta+Sans:wght@500;600;700;800&family=Space+Grotesk:wght@600;700&display=swap');
    :root {
      --bg-main: #f4f5f8; --bg-card: #ffffff; --bg-inset: #f8f9fa; --bg-hover: #f1f5f9;
      --text-main: #0f172a; --text-secondary: #475569; --text-muted: #64748b; --text-light: #94a3b8;
      --border-color: #e2e8f0; --border-dark: #0f172a; --brand-black: #111111;
      --accent-green: #10b981; --accent-green-bg: #ecfdf5; --accent-green-border: #a7f3d0; --accent-red: #ef4444;
      --font-heading: 'Space Grotesk', sans-serif; --font-body: 'Plus Jakarta Sans', sans-serif;
    }
    *, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; user-select: none; touch-action: manipulation; }
    html, body { width: 100vw; min-height: 100vh; background-color: var(--bg-main); color: var(--text-main); font-family: var(--font-body); font-size: 15px; }
    .app-container { width: 100%; max-width: 440px; margin: 0 auto; min-height: 100vh; padding: 16px 14px; display: flex; flex-direction: column; gap: 16px; }
    .top-header { background-color: var(--bg-card); border: 1px solid var(--border-color); border-radius: 16px; padding: 16px 20px; box-shadow: 0 1px 3px rgba(0,0,0,0.05); display: flex; flex-direction: column; gap: 12px; }
    .header-top-row { display: flex; justify-content: space-between; align-items: center; }
    .pill-badge { display: inline-flex; align-items: center; gap: 6px; background-color: var(--bg-inset); border: 1px solid var(--border-color); border-radius: 20px; padding: 4px 10px; font-size: 0.7rem; font-weight: 700; letter-spacing: 1px; color: var(--text-secondary); text-transform: uppercase; }
    .hud-status-badge { display: inline-flex; align-items: center; gap: 6px; background-color: var(--bg-inset); border: 1px solid var(--border-color); border-radius: 20px; padding: 4px 10px; font-size: 0.7rem; font-family: monospace; font-weight: 600; color: var(--text-secondary); }
    .status-dot { width: 8px; height: 8px; border-radius: 50%; background-color: var(--accent-green); box-shadow: 0 0 6px var(--accent-green); }
    .brand-title-box h1 { font-family: var(--font-heading); font-size: 1.5rem; font-weight: 700; color: var(--text-main); }
    .brand-sub { font-size: 0.85rem; font-weight: 500; color: var(--text-muted); margin-top: 2px; }
    .portrait-dashboard { display: flex; flex-direction: column; gap: 16px; flex: 1; }
    .panel-card { background-color: var(--bg-card); border: 1px solid var(--border-color); border-radius: 16px; padding: 18px; box-shadow: 0 4px 12px rgba(0,0,0,0.04); display: flex; flex-direction: column; gap: 16px; }
    .panel-header { display: flex; justify-content: space-between; align-items: center; border-bottom: 1px solid var(--border-color); padding-bottom: 12px; }
    .panel-title { display: flex; align-items: center; gap: 8px; font-family: var(--font-heading); font-size: 0.95rem; font-weight: 700; color: var(--text-main); }
    .step-num { width: 22px; height: 22px; border-radius: 50%; border: 1.5px solid var(--border-dark); display: flex; align-items: center; justify-content: center; font-family: monospace; font-size: 0.75rem; font-weight: 700; }
    .panel-tag { font-family: monospace; font-size: 0.65rem; font-weight: 600; color: var(--text-muted); background-color: var(--bg-inset); border: 1px solid var(--border-color); border-radius: 6px; padding: 2px 6px; }
    .controller-wrapper { display: flex; justify-content: center; align-items: center; padding: 10px 0; }
    .dpad-cross { display: grid; grid-template-columns: repeat(3, 82px); grid-template-rows: repeat(3, 82px); gap: 10px; }
    .btn-control { background-color: var(--bg-inset); border: 1.5px solid var(--border-color); border-radius: 12px; color: var(--text-main); font-family: var(--font-heading); cursor: pointer; display: flex; flex-direction: column; justify-content: center; align-items: center; gap: 2px; transition: all 0.15s ease; outline: none; }
    .btn-control .arrow-icon { font-size: 1.3rem; }
    .btn-control .key-hint { font-size: 0.6rem; font-family: monospace; color: var(--text-muted); }
    .btn-control:hover { background-color: var(--bg-hover); border-color: var(--border-dark); }
    .btn-control:active, .btn-control.active { background-color: var(--brand-black); border-color: var(--brand-black); color: #fff; transform: scale(0.95); }
    .btn-control:active .key-hint, .btn-control.active .key-hint { color: #a1a1aa; }
    .btn-control.btn-center-stop { background-color: #fff1f2; border-color: #fecdd3; color: var(--accent-red); }
    .btn-control.btn-center-stop:active, .btn-control.btn-center-stop.active { background-color: var(--accent-red); border-color: var(--accent-red); color: #fff; }
    .cleaner-panel-content { display: flex; flex-direction: column; gap: 14px; align-items: center; }
    .cleaner-status-box { width: 100%; background-color: var(--bg-inset); border: 1px solid var(--border-color); border-radius: 10px; padding: 12px 16px; display: flex; justify-content: space-between; align-items: center; }
    .cleaner-status-label { font-size: 0.75rem; font-weight: 600; color: var(--text-secondary); }
    .cleaner-status-state { font-family: var(--font-heading); font-size: 0.95rem; font-weight: 700; color: var(--text-muted); }
    .cleaner-status-state.active { color: var(--accent-green); background-color: var(--accent-green-bg); border: 1px solid var(--accent-green-border); padding: 2px 10px; border-radius: 20px; }
    .btn-cleaner-main { width: 100%; background-color: var(--bg-card); border: 1.5px solid var(--border-dark); border-radius: 30px; color: var(--text-main); font-family: var(--font-heading); font-size: 0.95rem; font-weight: 700; padding: 14px 24px; cursor: pointer; display: flex; align-items: center; justify-content: center; gap: 8px; transition: all 0.2s ease; outline: none; }
    .btn-cleaner-main:hover, .btn-cleaner-main.active { background-color: var(--brand-black); border-color: var(--brand-black); color: #fff; }
    .speed-readout { font-family: monospace; font-size: 1.1rem; font-weight: 700; color: var(--brand-black); }
    .speed-slider-wrapper { display: flex; flex-direction: column; gap: 14px; }
    input[type="range"].speed-slider { -webkit-appearance: none; width: 100%; height: 10px; background: var(--bg-inset); border: 1px solid var(--border-color); border-radius: 6px; outline: none; }
    input[type="range"].speed-slider::-webkit-slider-thumb { -webkit-appearance: none; width: 24px; height: 24px; border-radius: 50%; background: var(--brand-black); border: 2px solid #fff; cursor: pointer; }
    .speed-presets-group { display: grid; grid-template-columns: repeat(4, 1fr); gap: 8px; }
    .btn-speed-step { background-color: var(--bg-inset); border: 1px solid var(--border-color); border-radius: 8px; color: var(--text-secondary); font-family: monospace; font-size: 0.8rem; font-weight: 600; padding: 8px 4px; cursor: pointer; text-align: center; }
    .btn-speed-step:hover, .btn-speed-step.active { background-color: var(--brand-black); border-color: var(--brand-black); color: #fff; }
    .footer-bar { background-color: var(--bg-card); border: 1px solid var(--border-color); border-radius: 12px; padding: 10px 14px; display: flex; flex-direction: column; gap: 6px; }
    .log-msg { font-family: monospace; font-size: 0.72rem; color: var(--text-secondary); white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
    .hotkeys { display: flex; justify-content: space-between; font-family: monospace; font-size: 0.68rem; color: var(--text-muted); }
    .kbd { background-color: var(--bg-inset); border: 1px solid var(--border-color); border-radius: 4px; padding: 1px 4px; font-weight: 600; color: var(--text-main); }
  </style>
</head>
<body>
  <div class="app-container">
    <header class="top-header">
      <div class="header-top-row">
        <div class="pill-badge">HIMALIX PLATFORM</div>
        <div style="display:flex;align-items:center;gap:8px;">
          <a href="/update" target="_blank" style="background:#fef3c7;border:1px solid #fde68a;border-radius:20px;padding:3px 8px;font-family:monospace;font-size:0.68rem;font-weight:700;color:#b45309;text-decoration:none;">OTA ⚡</a>
          <div class="hud-status-badge">
            <span class="status-dot"></span>
            <span>ONLINE AP</span>
          </div>
        </div>
      </div>
      <div class="brand-title-box">
        <h1>River Cleaning <span>Boat</span></h1>
        <div class="brand-sub">by HimalixLabs</div>
      </div>
    </header>
    <main class="portrait-dashboard">
      <section class="panel-card">
        <div class="panel-header">
          <div class="panel-title"><span class="step-num">1</span><span>CONTROLLER</span></div>
          <span class="panel-tag">STEERING</span>
        </div>
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
        <div class="panel-header">
          <div class="panel-title"><span class="step-num">2</span><span>CLEANER ON OFF</span></div>
          <span class="panel-tag">RELAY SWITCH</span>
        </div>
        <div class="cleaner-panel-content">
          <div class="cleaner-status-box">
            <span class="cleaner-status-label">MOTOR RELAY STATE</span>
            <span class="cleaner-status-state" id="cleaner-state-text">OFF</span>
          </div>
          <button class="btn-cleaner-main" id="cleaner-toggle-btn">CLEANER ON / OFF →</button>
        </div>
      </section>
      <section class="panel-card">
        <div class="panel-header">
          <div class="panel-title"><span class="step-num">3</span><span>SPEED CONTROLLER</span></div>
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
    </main>
    <footer class="footer-bar">
      <div style="display:flex;justify-content:space-between;align-items:center;">
        <div class="log-msg" id="log-output">HimalixLabs River Cleaner AP Connected (192.168.4.1)</div>
        <a href="/update" target="_blank" style="font-family:monospace;font-size:0.68rem;font-weight:700;color:#b45309;text-decoration:none;background:#fef3c7;border:1px solid #fde68a;border-radius:4px;padding:1px 6px;">OTA ⚡</a>
      </div>
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
// OVER-THE-AIR (OTA) UPDATER SETUP
// ----------------------------------------------------------------------------------
void setupOTA() {
  // Bind Web Browser Firmware Uploader to /update
  httpUpdater.setup(&server, "/update");

  // Configure ArduinoOTA for IDE network flashing
  ArduinoOTA.setHostname("RIVER_CLEANER_BOT");

  ArduinoOTA.onStart([]() {
    stopMotors();
    setCleanerState(false);
    Serial.println("[OTA] Wireless firmware update started. Motors stopped.");
  });

  ArduinoOTA.onEnd([]() {
    Serial.println("\n[OTA] Firmware upload complete! Rebooting ESP8266...");
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("[OTA] Flashing Progress: %u%%\r", (progress / (total / 100)));
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("[OTA] Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });

  ArduinoOTA.begin();
  Serial.println("[OTA] Wireless OTA Services Active (Web /update & ArduinoOTA)");
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

  setupOTA();
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
  ArduinoOTA.handle();

  if (currentDirection != "STOP" && (millis() - lastCommandTime > FAILSAFE_TIMEOUT_MS)) {
    Serial.println("[SAFETY] Fail-safe timeout triggered! Auto-stopping motors.");
    stopMotors();
  }
}
