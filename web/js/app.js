// Mosquito Trap Dashboard — JavaScript
// Real-time WebSocket communication with ESP32 #2

const CONFIG = {
  WS_PORT: 81,
  RECONNECT_DELAY: 3000,
  CLOCK_INTERVAL: 1000,
  MAX_LOG_ENTRIES: 50,
};

let ws = null;
let state = {
  trap_online: false,
  led_state: false,
  led_brightness: 128,
  buzz_state: false,
  ir_detected: false,
  distance_cm: 0,
  mode: 0,
  detection_count: 0,
  error_flags: 0,
  emotion: 0,
  is_lifted: false,
  uptime: 0,
};

const EMOTIONS = ['Happy', 'Neutral', 'Angry', 'Sleeping', 'Scared'];
const MODES = ['Automatic', 'Manual', 'Schedule', 'Silent'];

// ===== WebSocket Connection =====
function connectWebSocket() {
  const host = window.location.hostname;
  ws = new WebSocket(`ws://${host}:${CONFIG.WS_PORT}/`);

  ws.onopen = () => {
    console.log('WebSocket connected');
    updateConnectionStatus(true);
  };

  ws.onclose = () => {
    console.log('WebSocket disconnected');
    updateConnectionStatus(false);
    setTimeout(connectWebSocket, CONFIG.RECONNECT_DELAY);
  };

  ws.onerror = (err) => {
    console.error('WebSocket error:', err);
  };

  ws.onmessage = (event) => {
    try {
      const data = JSON.parse(event.data);
      if (data.type === 'update' || data.type === 'status') {
        updateState(data);
      }
    } catch (e) {
      console.error('Parse error:', e);
    }
  };
}

// ===== State Updates =====
function updateState(data) {
  const prevState = { ...state };
  Object.assign(state, data);

  // Update UI elements
  updateStatusCards();
  updateSensors();
  updateFace(data.emotion);
  updateCounter();
  updateErrors(data.error_flags);
  updateModeButtons(data.mode);
  updateBrightnessSlider(data.led_brightness);
  updateLiftStatus(data.is_lifted);

  // Log detections
  if (data.detection_count > prevState.detection_count) {
    addDetectionEntry();
  }

  // Log lift events
  if (data.is_lifted && !prevState.is_lifted) {
    addLiftEntry();
  }
}

function updateConnectionStatus(connected) {
  const el = document.getElementById('connStatus');
  if (connected) {
    el.className = 'status-badge status-online';
    el.textContent = 'Connected';
  } else {
    el.className = 'status-badge status-offline';
    el.textContent = 'Disconnected';
  }
}

function updateStatusCards() {
  // Trap status
  const trapEl = document.getElementById('trapStatus');
  trapEl.textContent = state.trap_online ? 'ONLINE' : 'OFFLINE';
  trapEl.className = state.trap_online ? 'status-on' : 'status-off';

  // LED status
  const ledEl = document.getElementById('ledStatus');
  ledEl.textContent = state.led_state ? 'ON' : 'OFF';
  ledEl.className = state.led_state ? 'status-on' : 'status-off';

  // Buzz status
  const buzzEl = document.getElementById('buzzStatus');
  buzzEl.textContent = state.buzz_state ? 'ON' : 'OFF';
  buzzEl.className = state.buzz_state ? 'status-active' : 'status-off';

  // IR status
  const irEl = document.getElementById('irStatus');
  irEl.textContent = state.ir_detected ? 'DETECTED' : 'Clear';
  irEl.className = state.ir_detected ? 'status-active' : 'status-on';

  // Update status card icons
  document.getElementById('trapIcon').style.color = state.trap_online ? '#34d399' : '#fca5a5';
  document.getElementById('ledIcon').style.color = state.led_state ? '#38bdf8' : '#64748b';
  document.getElementById('buzzIcon').style.color = state.buzz_state ? '#fbbf24' : '#64748b';
  document.getElementById('irIcon').style.color = state.ir_detected ? '#fbbf24' : '#64748b';
}

function updateSensors() {
  document.getElementById('distance').textContent = `${state.distance_cm} cm`;
  document.getElementById('modeStatus').textContent = MODES[state.mode] || 'Unknown';
  document.getElementById('uptime').textContent = formatTime(state.uptime);
  document.getElementById('freeHeap').textContent = state.free_heap
    ? `${(state.free_heap / 1024).toFixed(1)} KB`
    : '--';
  
  // Update lift status
  const liftEl = document.getElementById('liftStatus');
  if (liftEl) {
    liftEl.textContent = state.is_lifted ? 'LIFTED!' : 'Normal';
    liftEl.className = state.is_lifted ? 'sensor-value status-active' : 'sensor-value';
  }
}

function updateCounter() {
  document.getElementById('detectionCount').textContent = state.detection_count;
  // Update bar (max 50 detections for full bar)
  const pct = Math.min((state.detection_count / 50) * 100, 100);
  document.getElementById('counterBar').style.width = `${pct}%`;
}

function updateModeButtons(mode) {
  for (let i = 0; i < 4; i++) {
    const btn = document.getElementById(`modeBtn${i}`);
    if (btn) {
      btn.className = i === mode ? 'btn btn-primary active' : 'btn btn-secondary';
    }
  }
}

function updateBrightnessSlider(val) {
  document.getElementById('brightnessSlider').value = val;
  document.getElementById('brightnessVal').textContent = val;
}

// ===== Face Drawing =====
function updateFace(emotion) {
  const canvas = document.getElementById('faceCanvas');
  if (!canvas) return;
  const ctx = canvas.getContext('2d');
  const w = canvas.width;
  const h = canvas.height;
  const cx = w / 2;
  const cy = h / 2;
  const r = w * 0.38;

  ctx.clearRect(0, 0, w, h);

  // Face outline
  ctx.strokeStyle = '#f8fafc';
  ctx.lineWidth = 2.5;
  ctx.beginPath();
  ctx.arc(cx, cy, r, 0, Math.PI * 2);
  ctx.stroke();

  // Inner glow
  const gradient = ctx.createRadialGradient(cx, cy, 0, cx, cy, r);
  gradient.addColorStop(0, 'rgba(56, 189, 248, 0.05)');
  gradient.addColorStop(1, 'rgba(56, 189, 248, 0)');
  ctx.fillStyle = gradient;
  ctx.fill();

  ctx.fillStyle = '#f8fafc';
  ctx.strokeStyle = '#f8fafc';
  ctx.lineWidth = 2;

  const eyeY = cy - r * 0.15;
  const mouthY = cy + r * 0.35;

  switch (emotion) {
    case 0: // Happy
      // Eyes
      ctx.beginPath();
      ctx.arc(cx - r * 0.3, eyeY, r * 0.12, 0, Math.PI * 2);
      ctx.fill();
      ctx.beginPath();
      ctx.arc(cx + r * 0.3, eyeY, r * 0.12, 0, Math.PI * 2);
      ctx.fill();
      // Smile
      ctx.beginPath();
      ctx.arc(cx, mouthY - r * 0.1, r * 0.35, 0.15 * Math.PI, 0.85 * Math.PI);
      ctx.stroke();
      break;

    case 1: // Neutral
      // Eyes
      ctx.fillRect(cx - r * 0.38, eyeY - 2, r * 0.22, 4);
      ctx.fillRect(cx + r * 0.16, eyeY - 2, r * 0.22, 4);
      // Mouth
      ctx.beginPath();
      ctx.moveTo(cx - r * 0.25, mouthY);
      ctx.lineTo(cx + r * 0.25, mouthY);
      ctx.stroke();
      break;

    case 2: // Angry
      // Eyes
      ctx.beginPath();
      ctx.arc(cx - r * 0.3, eyeY, r * 0.12, 0, Math.PI * 2);
      ctx.fill();
      ctx.beginPath();
      ctx.arc(cx + r * 0.3, eyeY, r * 0.12, 0, Math.PI * 2);
      ctx.fill();
      // Eyebrows
      ctx.lineWidth = 3;
      ctx.beginPath();
      ctx.moveTo(cx - r * 0.5, eyeY - r * 0.25);
      ctx.lineTo(cx - r * 0.15, eyeY - r * 0.12);
      ctx.stroke();
      ctx.beginPath();
      ctx.moveTo(cx + r * 0.5, eyeY - r * 0.25);
      ctx.lineTo(cx + r * 0.15, eyeY - r * 0.12);
      ctx.stroke();
      // Frown
      ctx.lineWidth = 2;
      ctx.beginPath();
      ctx.arc(cx, mouthY + r * 0.2, r * 0.3, 1.15 * Math.PI, 1.85 * Math.PI);
      ctx.stroke();
      break;

    case 3: // Sleeping
      // Closed eyes
      ctx.lineWidth = 2;
      ctx.beginPath();
      ctx.moveTo(cx - r * 0.4, eyeY);
      ctx.lineTo(cx - r * 0.15, eyeY);
      ctx.stroke();
      ctx.beginPath();
      ctx.moveTo(cx + r * 0.15, eyeY);
      ctx.lineTo(cx + r * 0.4, eyeY);
      ctx.stroke();
      // Small O mouth
      ctx.beginPath();
      ctx.arc(cx, mouthY, r * 0.1, 0, Math.PI * 2);
      ctx.stroke();
      // Zzz
      ctx.font = `${r * 0.2}px sans-serif`;
      ctx.fillStyle = '#38bdf8';
      ctx.fillText('z', cx + r * 0.5, eyeY - r * 0.3);
      ctx.font = `${r * 0.25}px sans-serif`;
      ctx.fillText('Z', cx + r * 0.6, eyeY - r * 0.55);
      break;

    case 4: // Scared
      // Wide open eyes
      ctx.beginPath();
      ctx.arc(cx - r * 0.3, eyeY, r * 0.18, 0, Math.PI * 2);
      ctx.fill();
      ctx.beginPath();
      ctx.arc(cx + r * 0.3, eyeY, r * 0.18, 0, Math.PI * 2);
      ctx.fill();
      // Pupils looking up
      ctx.fillStyle = '#0f172a';
      ctx.beginPath();
      ctx.arc(cx - r * 0.3, eyeY - r * 0.05, r * 0.06, 0, Math.PI * 2);
      ctx.fill();
      ctx.beginPath();
      ctx.arc(cx + r * 0.3, eyeY - r * 0.05, r * 0.06, 0, Math.PI * 2);
      ctx.fill();
      // Raised eyebrows
      ctx.strokeStyle = '#f8fafc';
      ctx.lineWidth = 3;
      ctx.beginPath();
      ctx.moveTo(cx - r * 0.45, eyeY - r * 0.35);
      ctx.lineTo(cx - r * 0.15, eyeY - r * 0.3);
      ctx.stroke();
      ctx.beginPath();
      ctx.moveTo(cx + r * 0.45, eyeY - r * 0.35);
      ctx.lineTo(cx + r * 0.15, eyeY - r * 0.3);
      ctx.stroke();
      // Open screaming mouth
      ctx.lineWidth = 2;
      ctx.beginPath();
      ctx.ellipse(cx, mouthY + r * 0.1, r * 0.15, r * 0.2, 0, 0, Math.PI * 2);
      ctx.stroke();
      break;
  }

  document.getElementById('emotionLabel').textContent = EMOTIONS[emotion] || 'Unknown';
}

// ===== Error Display =====
function updateErrors(flags) {
  const container = document.getElementById('errorBadges');
  const errorDefs = [
    ['IR Sensor Fault', 0x01],
    ['HC-SR04 Timeout', 0x02],
    ['Zapper Overcurrent', 0x04],
    ['LED Fault', 0x08],
    ['Buzzer Fault', 0x10],
    ['Over Temperature', 0x20],
    ['Trap Lifted', 0x40],
    ['Communication Error', 0x80],
  ];

  container.innerHTML = '';
  let hasError = false;

  errorDefs.forEach(([name, bit]) => {
    if (flags & bit) {
      hasError = true;
      const badge = document.createElement('span');
      badge.className = 'error-badge';
      badge.textContent = name;
      container.appendChild(badge);
    }
  });

  if (!hasError) {
    container.innerHTML = '<span class="ok-badge">All systems OK</span>';
  }
}

// ===== Lift Status =====
function updateLiftStatus(isLifted) {
  const liftEl = document.getElementById('liftStatus');
  if (liftEl) {
    liftEl.textContent = isLifted ? 'LIFTED!' : 'Normal';
    liftEl.className = isLifted ? 'sensor-value status-active' : 'sensor-value';
  }
  
  // Show/hide lift alert banner
  const alertBanner = document.getElementById('liftAlert');
  if (alertBanner) {
    alertBanner.style.display = isLifted ? 'flex' : 'none';
  }
}

function addLiftEntry() {
  const log = document.getElementById('detectionLog');
  const empty = log.querySelector('.detection-empty');
  if (empty) empty.remove();

  const entry = document.createElement('div');
  entry.className = 'detection-entry';
  entry.style.borderLeft = '3px solid #dc2626';
  entry.innerHTML = `
    <span class="detection-text" style="color:#fca5a5">TRAP LIFTED!</span>
    <span class="detection-time">${new Date().toLocaleTimeString()}</span>
  `;

  log.insertBefore(entry, log.firstChild);

  while (log.children.length > CONFIG.MAX_LOG_ENTRIES) {
    log.removeChild(log.lastChild);
  }
}

// ===== Detection Log =====
function addDetectionEntry() {
  const log = document.getElementById('detectionLog');

  // Remove empty message
  const empty = log.querySelector('.detection-empty');
  if (empty) empty.remove();

  const entry = document.createElement('div');
  entry.className = 'detection-entry';
  entry.innerHTML = `
    <span class="detection-text">Mosquito detected!</span>
    <span class="detection-time">${new Date().toLocaleTimeString()}</span>
  `;

  log.insertBefore(entry, log.firstChild);

  // Limit entries
  while (log.children.length > CONFIG.MAX_LOG_ENTRIES) {
    log.removeChild(log.lastChild);
  }
}

// ===== Commands =====
function sendCmd(cmd, param = 0) {
  if (ws && ws.readyState === WebSocket.OPEN) {
    ws.send(JSON.stringify({ cmd, param }));
  } else {
    fetch(`/api/command?cmd=${cmd}&param=${param}`, { method: 'POST' });
  }
}

function updateBrightness(val) {
  document.getElementById('brightnessVal').textContent = val;
  sendCmd(3, parseInt(val));
}

function updateVolume(val) {
  document.getElementById('volumeVal').textContent = val;
  sendCmd(11, parseInt(val)); // CMD_SET_VOLUME = 0x0B = 11
}

function setMode(mode) {
  sendCmd(8, mode);
}

// ===== Utilities =====
function formatTime(seconds) {
  if (!seconds && seconds !== 0) return '--';
  const h = Math.floor(seconds / 3600);
  const m = Math.floor((seconds % 3600) / 60);
  const s = seconds % 60;
  return `${h ? h + 'h ' : ''}${m}m ${s}s`;
}

function updateClock() {
  const el = document.getElementById('clock');
  if (el) {
    el.textContent = new Date().toLocaleTimeString();
  }
}

// ===== Init =====
document.addEventListener('DOMContentLoaded', () => {
  connectWebSocket();
  updateClock();
  setInterval(updateClock, CONFIG.CLOCK_INTERVAL);
  updateFace(0);
});
