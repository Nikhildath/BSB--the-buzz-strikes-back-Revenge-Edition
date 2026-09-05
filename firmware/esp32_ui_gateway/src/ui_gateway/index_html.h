#ifndef INDEX_HTML_H
#define INDEX_HTML_H

#include <Arduino.h>

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Mosquito Trap Dashboard</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;background:#0f172a;color:#e2e8f0;min-height:100vh}
.header{background:linear-gradient(135deg,#1e293b,#0f172a);padding:16px 24px;border-bottom:1px solid #334155;display:flex;justify-content:space-between;align-items:center}
.header h1{font-size:1.3rem;color:#38bdf8}
.status-badge{padding:4px 12px;border-radius:20px;font-size:0.8rem;font-weight:600}
.status-online{background:#065f46;color:#34d399}
.status-offline{background:#7f1d1d;color:#fca5a5}
.container{max-width:1200px;margin:0 auto;padding:16px}
.lift-alert{background:rgba(220,38,38,0.15);border:2px solid rgba(252,165,165,0.5);border-radius:12px;padding:16px;margin-bottom:16px;display:none;align-items:center;gap:12px;animation:pulse 2s infinite}
@keyframes pulse{0%,100%{border-color:rgba(252,165,165,0.5)}50%{border-color:rgba(252,165,165,1)}}
.lift-alert-icon{width:36px;height:36px;border-radius:50%;background:#dc2626;color:white;display:flex;align-items:center;justify-content:center;font-weight:bold;font-size:1.2rem}
.lift-alert-text{color:#fca5a5;font-weight:600}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(280px,1fr));gap:16px;margin-bottom:16px}
.card{background:#1e293b;border-radius:12px;padding:20px;border:1px solid #334155}
.card h3{color:#94a3b8;font-size:0.85rem;text-transform:uppercase;letter-spacing:1px;margin-bottom:12px}
.face-container{text-align:center;padding:20px}
.face-container canvas{border-radius:50%;background:#1e293b}
.controls{display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:12px}
.btn{padding:12px 20px;border:none;border-radius:8px;font-size:0.95rem;font-weight:600;cursor:pointer;transition:all 0.2s}
.btn:hover{transform:translateY(-1px);box-shadow:0 4px 12px rgba(0,0,0,0.3)}
.btn-primary{background:#2563eb;color:white}.btn-success{background:#059669;color:white}
.btn-danger{background:#dc2626;color:white}.btn-warning{background:#d97706;color:white}
.btn-secondary{background:#475569;color:white}.btn:active{transform:translateY(0)}
.slider-container{margin:12px 0}
.slider-container label{display:block;color:#94a3b8;font-size:0.85rem;margin-bottom:8px}
.slider{width:100%;height:8px;border-radius:4px;outline:none;-webkit-appearance:none;background:#334155}
.slider::-webkit-slider-thumb{-webkit-appearance:none;width:20px;height:20px;border-radius:50%;background:#38bdf8;cursor:pointer}
.sensor-row{display:flex;justify-content:space-between;padding:8px 0;border-bottom:1px solid #334155}
.sensor-row:last-child{border-bottom:none}
.sensor-label{color:#94a3b8}.sensor-value{font-weight:600;color:#f8fafc}
.status-active{color:#fbbf24;font-weight:600}
.detection-log{max-height:200px;overflow-y:auto;padding:8px;background:#0f172a;border-radius:8px}
.detection-entry{padding:6px 0;border-bottom:1px solid #1e293b;font-size:0.85rem;color:#94a3b8}
.error-badge{display:inline-block;padding:2px 8px;border-radius:4px;font-size:0.75rem;background:#7f1d1d;color:#fca5a5;margin:2px}
.ok-badge{display:inline-block;padding:2px 8px;border-radius:4px;font-size:0.75rem;background:#065f46;color:#34d399;margin:2px}
@media(max-width:600px){.grid{grid-template-columns:1fr}.controls{grid-template-columns:1fr}}
</style>
</head>
<body>
<div class="header">
<h1>Mosquito Trap Dashboard</h1>
<span id="connStatus" class="status-badge status-online">Connected</span>
</div>
<div class="container">
<div class="lift-alert" id="liftAlert">
<span class="lift-alert-icon">!</span>
<span class="lift-alert-text">TRAP LIFTED - Tamper Alert Active</span>
</div>
<div class="grid">
<div class="card face-container">
<canvas id="faceCanvas" width="120" height="120"></canvas>
<div id="emotionLabel" style="margin-top:12px;color:#94a3b8;font-size:0.9rem">Happy</div>
</div>
<div class="card">
<h3>Detection Counter</h3>
<div style="font-size:2rem;font-weight:700;color:#f8fafc" id="detectionCount">0</div>
<div style="color:#64748b;font-size:0.8rem">mosquitoes detected</div>
</div>
<div class="card">
<h3>System Status</h3>
<div class="sensor-row"><span class="sensor-label">Trap</span><span class="sensor-value" id="trapStatus">--</span></div>
<div class="sensor-row"><span class="sensor-label">LED</span><span class="sensor-value" id="ledStatus">--</span></div>
<div class="sensor-row"><span class="sensor-label">Speaker</span><span class="sensor-value" id="buzzStatus">--</span></div>
<div class="sensor-row"><span class="sensor-label">IR Sensor</span><span class="sensor-value" id="irStatus">--</span></div>
<div class="sensor-row"><span class="sensor-label">Height</span><span class="sensor-value" id="distance">--</span></div>
<div class="sensor-row"><span class="sensor-label">Position</span><span class="sensor-value" id="liftStatus">Normal</span></div>
<div class="sensor-row"><span class="sensor-label">Mode</span><span class="sensor-value" id="modeStatus">--</span></div>
<div class="sensor-row"><span class="sensor-label">Uptime</span><span class="sensor-value" id="uptime">--</span></div>
</div>
</div>
<div class="card" style="margin-bottom:16px">
<h3>Controls</h3>
<div class="controls">
<button class="btn btn-success" onclick="sendCmd(1,0)">LED ON</button>
<button class="btn btn-danger" onclick="sendCmd(2,0)">LED OFF</button>
<button class="btn btn-success" onclick="sendCmd(4,0)">Speaker ON</button>
<button class="btn btn-danger" onclick="sendCmd(5,0)">Speaker OFF</button>
<button class="btn btn-warning" onclick="sendCmd(6,0)">Zapper ON</button>
<button class="btn btn-secondary" onclick="sendCmd(7,0)">Zapper OFF</button>
<button class="btn btn-primary" onclick="sendCmd(9,0)">Reset Counter</button>
<button class="btn btn-secondary" onclick="sendCmd(10,0)">Restart</button>
</div>
<div class="slider-container">
<label>LED Brightness: <span id="brightnessVal">128</span></label>
<input type="range" class="slider" min="0" max="255" value="128" id="brightnessSlider" oninput="updateBrightness(this.value)">
</div>
<div class="slider-container">
<label>Speaker Volume: <span id="volumeVal">128</span></label>
<input type="range" class="slider" min="0" max="255" value="128" id="volumeSlider" oninput="updateVolume(this.value)">
</div>
<div class="controls" style="margin-top:12px">
<button class="btn btn-primary" onclick="sendCmd(8,0)">Auto Mode</button>
<button class="btn btn-secondary" onclick="sendCmd(8,1)">Manual</button>
<button class="btn btn-warning" onclick="sendCmd(8,2)">Schedule</button>
<button class="btn btn-secondary" onclick="sendCmd(8,3)">Silent</button>
</div>
</div>
<div class="card">
<h3>System Health</h3>
<div id="errorBadges"><span class="ok-badge">All systems OK</span></div>
</div>
</div>
<script>
let ws;
const EMOTIONS=['Happy','Neutral','Angry','Sleeping','Scared'];
const MODES=['Automatic','Manual','Schedule','Silent'];
function connectWS(){
const host=window.location.hostname;
ws=new WebSocket('ws://'+host+':81/');
ws.onopen=()=>{document.getElementById('connStatus').className='status-badge status-online';document.getElementById('connStatus').textContent='Connected'};
ws.onclose=()=>{document.getElementById('connStatus').className='status-badge status-offline';document.getElementById('connStatus').textContent='Disconnected';setTimeout(connectWS,3000)};
ws.onmessage=(e)=>{const d=JSON.parse(e.data);if(d.type==='update'||d.type==='status')updateUI(d)};
}
function updateUI(d){
document.getElementById('trapStatus').textContent=d.trap_online?'ONLINE':'OFFLINE';
document.getElementById('ledStatus').textContent=d.led_state?'ON':'OFF';
document.getElementById('buzzStatus').textContent=d.buzz_state?'ON':'OFF';
document.getElementById('irStatus').textContent=d.ir_detected?'DETECTED':'Clear';
document.getElementById('distance').textContent=d.distance_cm+' cm';
document.getElementById('modeStatus').textContent=MODES[d.mode]||'Unknown';
document.getElementById('detectionCount').textContent=d.detection_count;
document.getElementById('uptime').textContent=formatTime(d.uptime);
document.getElementById('brightnessSlider').value=d.led_brightness;
document.getElementById('brightnessVal').textContent=d.led_brightness;
document.getElementById('emotionLabel').textContent=EMOTIONS[d.emotion]||'Unknown';
drawFace(d.emotion);
updateErrors(d.error_flags);
const liftEl=document.getElementById('liftStatus');
if(liftEl){liftEl.textContent=d.is_lifted?'LIFTED!':'Normal';liftEl.className=d.is_lifted?'sensor-value status-active':'sensor-value'}
document.getElementById('liftAlert').style.display=d.is_lifted?'flex':'none';
}
function drawFace(emotion){
const c=document.getElementById('faceCanvas'),ctx=c.getContext('2d');
ctx.clearRect(0,0,120,120);
ctx.strokeStyle='#f8fafc';ctx.lineWidth=2;ctx.beginPath();ctx.arc(60,60,45,0,Math.PI*2);ctx.stroke();
ctx.fillStyle='#f8fafc';
const eyeY=45,mouthY=72;
if(emotion===0){ctx.beginPath();ctx.arc(42,eyeY,6,0,Math.PI*2);ctx.fill();ctx.beginPath();ctx.arc(78,eyeY,6,0,Math.PI*2);ctx.fill();ctx.beginPath();ctx.arc(60,mouthY-5,15,0.15*Math.PI,0.85*Math.PI);ctx.stroke()}
else if(emotion===1){ctx.fillRect(33,eyeY-2,16,4);ctx.fillRect(71,eyeY-2,16,4);ctx.beginPath();ctx.moveTo(45,mouthY);ctx.lineTo(75,mouthY);ctx.stroke()}
else if(emotion===2){ctx.beginPath();ctx.arc(42,eyeY,6,0,Math.PI*2);ctx.fill();ctx.beginPath();ctx.arc(78,eyeY,6,0,Math.PI*2);ctx.fill();ctx.lineWidth=3;ctx.beginPath();ctx.moveTo(28,38);ctx.lineTo(52,44);ctx.stroke();ctx.beginPath();ctx.moveTo(92,38);ctx.lineTo(68,44);ctx.stroke();ctx.lineWidth=2;ctx.beginPath();ctx.arc(60,85,15,1.15*Math.PI,1.85*Math.PI);ctx.stroke()}
else if(emotion===3){ctx.fillRect(35,eyeY,12,3);ctx.fillRect(73,eyeY,12,3);ctx.beginPath();ctx.arc(60,mouthY,6,0,Math.PI*2);ctx.stroke();ctx.font='12px sans-serif';ctx.fillStyle='#38bdf8';ctx.fillText('z',90,35);ctx.font='14px sans-serif';ctx.fillText('Z',98,25)}
else if(emotion===4){ctx.beginPath();ctx.arc(42,eyeY,8,0,Math.PI*2);ctx.fill();ctx.beginPath();ctx.arc(78,eyeY,8,0,Math.PI*2);ctx.fill();ctx.fillStyle='#0f172a';ctx.beginPath();ctx.arc(42,eyeY-3,3,0,Math.PI*2);ctx.fill();ctx.beginPath();ctx.arc(78,eyeY-3,3,0,Math.PI*2);ctx.fill();ctx.strokeStyle='#f8fafc';ctx.lineWidth=3;ctx.beginPath();ctx.moveTo(30,32);ctx.lineTo(52,37);ctx.stroke();ctx.beginPath();ctx.moveTo(90,32);ctx.lineTo(68,37);ctx.stroke();ctx.lineWidth=2;ctx.beginPath();ctx.ellipse(60,mouthY+5,8,10,0,0,Math.PI*2);ctx.stroke()}
}
function updateErrors(f){
const el=document.getElementById('errorBadges');el.innerHTML='';
const flags=[['IR Fault',1],['HC-SR04',2],['Zapper',4],['LED',8],['Speaker',16],['Temp',32],['Lifted',64],['Comm',128]];
let any=false;
flags.forEach(([n,b])=>{if(f&b){el.innerHTML+='<span class="error-badge">'+n+'</span>';any=true}});
if(!any)el.innerHTML='<span class="ok-badge">All systems OK</span>';
}
function sendCmd(c,p){fetch('/api/command?cmd='+c+'&param='+p,{method:'POST'})}
function updateBrightness(v){document.getElementById('brightnessVal').textContent=v;sendCmd(3,v)}
function updateVolume(v){document.getElementById('volumeVal').textContent=v;sendCmd(11,v)}
function formatTime(s){const h=Math.floor(s/3600),m=Math.floor((s%3600)/60),sec=s%60;return(h?h+'h ':'')+m+'m '+sec+'s'}
connectWS();
</script>
</body>
</html>
)rawliteral";

#endif
