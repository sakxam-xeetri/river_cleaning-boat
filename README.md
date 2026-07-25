# 🚤 ESP8266 River Cleaning Boat Controller (HimalixLabs)

An advanced, web-controlled autonomous & manual **River Cleaning Boat** system powered by an **ESP8266**, **L298N Motor Driver** (differential dual-motor steering), and a **Relay Module** (controlling the river trash collecting/cleaner motor).

Features an **Open Wi-Fi Access Point** (`RIVER_CLEANER_BOT`), **Captive Portal auto-popup**, and a high-performance **Landscape Web Interface** with a dark matte & neon crimson red cyber HUD aesthetic, **100% sharp edges** (0px border radius), and direct support for **PlatformIO IDE** and **Arduino IDE**.

---

## ⚡ Serial Pinout Connections & Schematic

All motor and relay control pins are mapped serially across pins **D1 through D7** on the ESP8266 (NodeMCU / WeMos D1 mini):

| ESP8266 Pin | GPIO Pin | Component Pin | Function Description |
| :--- | :--- | :--- | :--- |
| **D1** | GPIO5 | **ENA** | Motor Driver ENA (PWM Speed Motor A / Left) |
| **D2** | GPIO4 | **IN1** | Motor Driver IN1 (Direction Motor A) |
| **D3** | GPIO0 | **IN2** | Motor Driver IN2 (Direction Motor A) |
| **D4** | GPIO2 | **IN3** | Motor Driver IN3 (Direction Motor B / Right) |
| **D5** | GPIO14 | **IN4** | Motor Driver IN4 (Direction Motor B) |
| **D6** | GPIO12 | **ENB** | Motor Driver ENB (PWM Speed Motor B / Right) |
| **D7** | GPIO13 | **RELAY IN** | Cleaner Motor Relay Switch Control Signal |

### 🔌 Power Supply Wiring Rules
1. **L298N Motor Driver Power**:
   - `12V` Terminal → Motor Battery (+7.4V to +12V LiPo/LiFePO4)
   - `GND` Terminal → Motor Battery (-) **AND** ESP8266 `GND` *(Common Ground is mandatory!)*
   - `5V` Terminal → Powers ESP8266 `VIN` or `5V` pin (if L298N 5V jumper is engaged).
2. **Relay Module Wiring**:
   - `VCC` → 5V (from L298N 5V or ESP8266 5V pin)
   - `GND` → Common Ground
   - `IN` → Pin **D7**
   - `COM` & `NO` → Connected in series with Cleaner Motor and Cleaner Battery.

---

## 🚀 Uploading via PlatformIO IDE (Recommended)

PlatformIO is fully configured for single-click compilation and uploading!

1. Install **PlatformIO IDE** extension in VS Code / Cursor / Antigravity IDE.
2. Open this project folder (`d:\river_cleaning boat`).
3. Connect your ESP8266 board via USB cable.
4. Click the PlatformIO Alien Icon in the sidebar or use keyboard shortcuts:
   - **Build**: `Ctrl + Alt + B` (or `Cmd + Option + B` on Mac)
   - **Upload**: `Ctrl + Alt + U` (or `Cmd + Option + U` on Mac)
   - **Serial Monitor**: `Ctrl + Alt + S` (or click `PlatformIO: Serial Monitor` at the bottom status bar)

### PlatformIO Configuration File: [`platformio.ini`](file:///d:/river_cleaning%20boat/platformio.ini)
```ini
[platformio]
default_envs = nodemcuv2
src_dir = src

[env:nodemcuv2]
platform = espressif8266
board = nodemcuv2
framework = arduino
monitor_speed = 115200
upload_speed = 921600
```

---

## 🛠️ Uploading via Arduino IDE (Alternative)

1. Open **Arduino IDE**.
2. Go to **File > Preferences** and add the ESP8266 board manager URL:
   `http://arduino.esp8266.com/stable/package_esp8266com_index.json`
3. Open **Tools > Board > Boards Manager...**, search for `esp8266` and install it.
4. Select board: **Tools > Board > ESP8266 Boards > NodeMCU 1.0 (ESP-12E Module)**.
5. Open [`river_cleaner_bot.ino`](file:///d:/river_cleaning%20boat/river_cleaner_bot.ino).
6. Connect your ESP8266 via USB cable and select the COM port.
7. Click **Upload** (➜).

---

## 📶 Automatic Open Wi-Fi Connection

1. Power on the ESP8266 boat.
2. The ESP8266 automatically starts an open Wi-Fi Access Point:
   - **Wi-Fi Network Name (SSID)**: `RIVER_CLEANER_BOT`
   - **Password**: *None (Open Wi-Fi)*
   - **IP Address**: `192.168.4.1`
3. Connect your smartphone, tablet, or laptop to `RIVER_CLEANER_BOT`.
4. A captive portal prompt will automatically launch. If it doesn't open automatically, navigate to:
   ```text
   http://192.168.4.1
   ```

---

## 📁 Workspace Files

- [platformio.ini](file:///d:/river_cleaning%20boat/platformio.ini) – PlatformIO IDE build configuration.
- [src/main.cpp](file:///d:/river_cleaning%20boat/src/main.cpp) – PlatformIO C++ main source code.
- [river_cleaner_bot.ino](file:///d:/river_cleaning%20boat/river_cleaner_bot.ino) – Arduino IDE C++ firmware.
- [index.html](file:///d:/river_cleaning%20boat/index.html) – Landscape web interface dashboard HTML.
- [styles.css](file:///d:/river_cleaning%20boat/styles.css) – Dark & Crimson Red sharp HUD stylesheet.
- [app.js](file:///d:/river_cleaning%20boat/app.js) – Real-time event handling & mock browser testing engine.
- [README.md](file:///d:/river_cleaning%20boat/README.md) – Project setup & wiring documentation.
