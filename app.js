/**
 * River Cleaning Boat Controller by HimalixLabs
 * Professional event engine for touch, mouse, keyboard hotkeys, and HTTP/mock API endpoints.
 */

(function () {
  'use strict';

  // Global State
  const state = {
    direction: 'STOP',
    speed: 200,          // PWM scale 0 - 255
    cleanerOn: false,
    isConnected: false,
    isMockMode: false,
    lastSentTime: 0,
    throttleDelay: 100   // Throttling for ESP8266 HTTP stability
  };

  // DOM Elements Selection
  const elements = {
    statusIndicator: document.getElementById('status-indicator'),
    statusText: document.getElementById('status-text'),
    speedValDisplay: document.getElementById('speed-val-display'),
    speedSlider: document.getElementById('speed-slider'),
    cleanerToggleBtn: document.getElementById('cleaner-toggle-btn'),
    cleanerStateText: document.getElementById('cleaner-state-text'),
    logOutput: document.getElementById('log-output'),
    controlButtons: document.querySelectorAll('.btn-control'),
    speedPresets: document.querySelectorAll('.btn-speed-step')
  };

  // App Initialization
  function init() {
    setupEventListeners();
    setupKeyboardListeners();
    checkConnection();
    updateUI();
    log('HimalixLabs Boat Controller Online. Ready for command.');
  }

  // Update UI Visuals
  function updateUI() {
    // Speed Percentage Calculation
    const speedPercent = Math.round((state.speed / 255) * 100);
    if (elements.speedValDisplay) {
      elements.speedValDisplay.textContent = `${speedPercent}%`;
    }
    if (elements.speedSlider) {
      elements.speedSlider.value = state.speed;
    }

    // Preset Speed Buttons Highlight
    elements.speedPresets.forEach(btn => {
      const val = parseInt(btn.dataset.speed, 10);
      btn.classList.toggle('active', val === state.speed);
    });

    // Cleaner Relay State Highlight
    if (elements.cleanerToggleBtn && elements.cleanerStateText) {
      if (state.cleanerOn) {
        elements.cleanerToggleBtn.classList.add('active');
        elements.cleanerStateText.classList.add('active');
        elements.cleanerStateText.textContent = 'ACTIVE (ON)';
      } else {
        elements.cleanerToggleBtn.classList.remove('active');
        elements.cleanerStateText.classList.remove('active');
        elements.cleanerStateText.textContent = 'OFF';
      }
    }

    // D-Pad Direction Buttons Highlight
    elements.controlButtons.forEach(btn => {
      const dir = btn.dataset.dir;
      btn.classList.toggle('active', dir === state.direction);
    });
  }

  // Logging telemetry utility
  function log(msg) {
    if (elements.logOutput) {
      const timestamp = new Date().toLocaleTimeString('en-US', { hour12: false });
      elements.logOutput.textContent = `[${timestamp}] ${msg}`;
    }
  }

  // Send Motor Movement Command
  function sendCommand(dir, speedOverride) {
    const now = Date.now();
    const currentSpeed = speedOverride !== undefined ? speedOverride : state.speed;

    if (dir !== undefined) {
      state.direction = dir;
    }

    updateUI();

    // Throttling fast repeated clicks unless it is a STOP command
    if (dir !== 'STOP' && now - state.lastSentTime < state.throttleDelay) {
      return;
    }
    state.lastSentTime = now;

    const url = `/move?dir=${state.direction}&speed=${currentSpeed}`;

    if (state.isMockMode) {
      log(`[MOCK] DIR: ${state.direction} | PWM: ${currentSpeed}`);
      return;
    }

    fetch(url, { method: 'GET', cache: 'no-cache' })
      .then(res => {
        if (res.ok) {
          setConnectedStatus(true);
          log(`COMMAND: ${state.direction} @ ${Math.round((currentSpeed / 255) * 100)}%`);
        } else {
          log(`HTTP ERROR: ${res.status}`);
        }
      })
      .catch(() => {
        setConnectedStatus(false);
        log(`CONNECTION ERROR - CHECK ESP8266 AP`);
      });
  }

  // Send Cleaner Toggle Command
  function sendCleanerToggle(targetState) {
    state.cleanerOn = targetState !== undefined ? targetState : !state.cleanerOn;
    updateUI();

    const stateStr = state.cleanerOn ? 'on' : 'off';
    const url = `/cleaner?state=${stateStr}`;

    if (state.isMockMode) {
      log(`[MOCK] CLEANER RELAY: ${stateStr.toUpperCase()}`);
      return;
    }

    fetch(url, { method: 'GET', cache: 'no-cache' })
      .then(res => {
        if (res.ok) {
          setConnectedStatus(true);
          log(`CLEANER RELAY: ${stateStr.toUpperCase()}`);
        }
      })
      .catch(() => {
        setConnectedStatus(false);
        log(`CLEANER COMMAND FAILED`);
      });
  }

  // Server Ping & Status Sync
  function checkConnection() {
    fetch('/status', { method: 'GET', cache: 'no-cache' })
      .then(res => res.json())
      .then(data => {
        setConnectedStatus(true);
        if (data.speed !== undefined) state.speed = data.speed;
        if (data.cleaner !== undefined) state.cleanerOn = data.cleaner;
        if (data.direction !== undefined) state.direction = data.direction;
        updateUI();
        log('CONNECTED TO HIMALIXLABS ESP8266 BOAT');
      })
      .catch(() => {
        // Fallback to local browser preview mode
        state.isMockMode = true;
        setConnectedStatus(true, true);
        log('MOCK MODE ACTIVE (Browser Local Preview)');
      });
  }

  function setConnectedStatus(online, isMock = false) {
    state.isConnected = online;
    state.isMockMode = isMock;

    if (elements.statusIndicator && elements.statusText) {
      if (online) {
        elements.statusIndicator.classList.remove('offline');
        elements.statusText.textContent = isMock ? 'MOCK MODE' : 'ONLINE (AP)';
        elements.statusText.style.color = '#ff2a55';
      } else {
        elements.statusIndicator.classList.add('offline');
        elements.statusText.textContent = 'DISCONNECTED';
        elements.statusText.style.color = '#777777';
      }
    }
  }

  // Mouse & Touch Listeners
  function setupEventListeners() {
    // D-Pad Touch/Click Events
    elements.controlButtons.forEach(btn => {
      const dir = btn.dataset.dir;

      const triggerHandler = (e) => {
        e.preventDefault();
        sendCommand(dir);
      };

      btn.addEventListener('mousedown', triggerHandler);
      btn.addEventListener('touchstart', triggerHandler);
    });

    // Auto-Stop when releasing mouse or touch (if moving)
    const releaseStop = () => {
      if (state.direction !== 'STOP') {
        sendCommand('STOP');
      }
    };
    window.addEventListener('mouseup', releaseStop);
    window.addEventListener('touchend', releaseStop);

    // Speed Slider Input Event
    if (elements.speedSlider) {
      elements.speedSlider.addEventListener('input', (e) => {
        const val = parseInt(e.target.value, 10);
        state.speed = val;
        updateUI();
        if (state.direction !== 'STOP') {
          sendCommand(state.direction, val);
        }
      });
    }

    // Speed Presets
    elements.speedPresets.forEach(btn => {
      btn.addEventListener('click', () => {
        const pwm = parseInt(btn.dataset.speed, 10);
        state.speed = pwm;
        updateUI();
        if (state.direction !== 'STOP') {
          sendCommand(state.direction, pwm);
        }
      });
    });

    // Cleaner Toggle Button
    if (elements.cleanerToggleBtn) {
      elements.cleanerToggleBtn.addEventListener('click', () => {
        sendCleanerToggle();
      });
    }
  }

  // Keyboard Hotkey Support (WASD, Arrows, Space, C, 1-4)
  function setupKeyboardListeners() {
    const keyMap = {
      'KeyW': 'FORWARD',
      'ArrowUp': 'FORWARD',
      'KeyS': 'BACKWARD',
      'ArrowDown': 'BACKWARD',
      'KeyA': 'LEFT',
      'ArrowLeft': 'LEFT',
      'KeyD': 'RIGHT',
      'ArrowRight': 'RIGHT',
      'Space': 'STOP'
    };

    let activeKey = null;

    window.addEventListener('keydown', (e) => {
      if (e.repeat) return;

      if (keyMap[e.code]) {
        e.preventDefault();
        activeKey = e.code;
        sendCommand(keyMap[e.code]);
      } else if (e.code === 'KeyC') {
        e.preventDefault();
        sendCleanerToggle();
      } else if (e.code === 'Digit1') {
        state.speed = 64;
        updateUI();
      } else if (e.code === 'Digit2') {
        state.speed = 128;
        updateUI();
      } else if (e.code === 'Digit3') {
        state.speed = 192;
        updateUI();
      } else if (e.code === 'Digit4') {
        state.speed = 255;
        updateUI();
      }
    });

    window.addEventListener('keyup', (e) => {
      if (keyMap[e.code] && activeKey === e.code) {
        e.preventDefault();
        activeKey = null;
        sendCommand('STOP');
      }
    });
  }

  // Start when DOM ready
  document.addEventListener('DOMContentLoaded', init);

})();
