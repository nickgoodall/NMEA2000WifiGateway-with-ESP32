// Network settings page — served at GET /settings
// Self-contained, no external resources, mobile-friendly dark theme.

const char settingsHTML[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Network Settings</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{background:#111722;color:#c8d6e5;font-family:'Segoe UI',sans-serif;padding:16px;min-height:100vh}
h1{font-size:1rem;font-weight:600;color:#4db6ff;letter-spacing:.08em;text-transform:uppercase;margin-bottom:16px}
.card{background:#1e2633;border:1px solid #2a3444;border-radius:6px;padding:16px;margin-bottom:12px}
.card h2{font-size:.72rem;color:#607080;letter-spacing:.12em;text-transform:uppercase;margin-bottom:12px}
label{display:block;font-size:.82rem;color:#8ba0b5;margin:10px 0 3px}
input[type=text],input[type=password],select{width:100%;background:#111722;border:1px solid #2a3444;color:#c8d6e5;padding:8px 10px;border-radius:4px;font-size:.88rem;outline:none}
input:focus,select:focus{border-color:#4db6ff}
.mg{display:flex;gap:8px;flex-wrap:wrap}
.mo{flex:1;min-width:110px;background:#111722;border:2px solid #2a3444;border-radius:5px;padding:10px 8px;cursor:pointer;text-align:center;user-select:none}
.mo input{display:none}
.mo .t{font-size:.85rem;font-weight:600;display:block}
.mo .s{font-size:.72rem;color:#5a6a7a;display:block;margin-top:3px}
.mo.sel{border-color:#4db6ff;color:#4db6ff}
.mo.sel .s{color:#3a7eaa}
.row{display:flex;gap:8px;align-items:flex-end}
.row select{flex:1}
.btn{padding:10px 18px;border:none;border-radius:4px;cursor:pointer;font-size:.88rem;font-weight:600;letter-spacing:.04em}
.btn-p{background:#1a6fa8;color:#fff}.btn-p:hover{background:#2080c0}
.btn-d{background:#6a1010;color:#ffbbbb}.btn-d:hover{background:#8a1818}
.btn-sm{background:#1e2633;border:1px solid #2a3444;color:#8ba0b5;padding:7px 12px;border-radius:4px;cursor:pointer;font-size:.8rem;white-space:nowrap}
.sr{display:flex;justify-content:space-between;padding:5px 0;border-bottom:1px solid #1a2230;font-size:.83rem}
.sr:last-child{border:none}
.ok{color:#4caf50}.err{color:#f44336}
#msg{margin-top:10px;padding:9px 12px;border-radius:4px;font-size:.83rem;display:none}
#msg.ok{background:#1a3220;color:#66bb6a;display:block}
#msg.err{background:#321a1a;color:#ef5350;display:block}
.sect{display:none}.vis{display:block}
a{color:#4db6ff;font-size:.83rem;text-decoration:none}
</style>
</head>
<body>
<h1>&#9881; Network Settings</h1>

<div class="card">
  <h2>Current Status</h2>
  <div id="st">Loading...</div>
</div>

<div class="card">
  <h2>WiFi Mode</h2>
  <div class="mg" id="mg">
    <label class="mo" id="mo0"><input type="radio" name="mode" value="0">
      <span class="t">AP Only</span><span class="s">Broadcasts its own network</span></label>
    <label class="mo" id="mo1"><input type="radio" name="mode" value="1">
      <span class="t">Join Network</span><span class="s">Connects to your router</span></label>
    <label class="mo" id="mo2"><input type="radio" name="mode" value="2">
      <span class="t">Both</span><span class="s">Router + AP fallback</span></label>
  </div>
</div>

<div id="apSect" class="card sect">
  <h2>Access Point Settings</h2>
  <label>AP Network Name (SSID)</label>
  <input type="text" id="apSsid" maxlength="32" placeholder="NMEA2000-GW">
  <label>AP Password <span style="color:#5a6a7a">(min 8 chars &mdash; blank = keep existing)</span></label>
  <input type="password" id="apPass" maxlength="64" placeholder="leave blank to keep current password">
</div>

<div id="staSect" class="card sect">
  <h2>Join Network (Station) Settings</h2>
  <label>Available Networks</label>
  <div class="row">
    <select id="scanSel" onchange="pickNet()">
      <option value="">-- press Scan to find networks --</option>
    </select>
    <button class="btn-sm" onclick="doScan()">&#8635; Scan</button>
  </div>
  <label>SSID <span style="color:#5a6a7a">(or type manually above)</span></label>
  <input type="text" id="staSsid" maxlength="32" placeholder="YourBoatWiFi">
  <label>Password <span style="color:#5a6a7a">(blank = keep existing)</span></label>
  <input type="password" id="staPass" maxlength="64" placeholder="leave blank to keep current password">
</div>

<div class="card">
  <button class="btn btn-p" onclick="doSave()">&#10003; Save &amp; Reboot</button>
  <div id="msg"></div>
</div>

<div class="card">
  <h2>MQTT Remote Monitoring</h2>
  <div class="sr" id="mqtt-status-row" style="margin-bottom:10px">
    <span style="font-size:.8rem;color:#607080">Status</span>
    <span id="mqtt-st" style="font-size:.8rem">Loading…</span>
  </div>
  <label>Broker Host <span style="color:#5a6a7a">(e.g. abc123.s1.eu.hivemq.cloud)</span></label>
  <input type="text" id="mqttHost" maxlength="128" placeholder="xxxxxxx.s1.eu.hivemq.cloud">
  <label>Port</label>
  <input type="text" id="mqttPort" maxlength="5" placeholder="8883" value="8883">
  <label>Username</label>
  <input type="text" id="mqttUser" maxlength="64" placeholder="hivemq-username">
  <label>Password <span style="color:#5a6a7a">(blank = keep existing)</span></label>
  <input type="password" id="mqttPass" maxlength="128" placeholder="leave blank to keep current">
  <label>Topic prefix <span style="color:#5a6a7a">(data published to prefix/telemetry)</span></label>
  <input type="text" id="mqttTopic" maxlength="64" placeholder="boat/myyacht">
  <div style="margin-top:12px;display:flex;align-items:center;gap:10px">
    <button class="btn btn-p" onclick="saveMqtt()">&#10003; Save &amp; Apply</button>
    <label style="display:flex;align-items:center;gap:6px;margin:0;font-size:.84rem;color:#8ba0b5;cursor:pointer">
      <input type="checkbox" id="mqttEnabled"> Enable MQTT
    </label>
  </div>
  <div id="mqtt-msg" style="display:none;margin-top:8px;padding:8px 10px;border-radius:4px;font-size:.82rem"></div>
</div>

<div class="card">
  <h2>Ruuvi BLE Sensors</h2>
  <div id="ruuvi-list"><p style="font-size:.8rem;color:#607080">Loading…</p></div>
  <button class="btn btn-p" style="margin-top:10px" onclick="saveRuuvi()">&#10003; Save Labels</button>
  <div id="ruuvi-msg" style="display:none;margin-top:8px;padding:8px 10px;border-radius:4px;font-size:.82rem"></div>
</div>

<div class="card" style="border-color:#4a1515">
  <h2 style="color:#b04040">Factory Reset</h2>
  <p style="font-size:.8rem;color:#607080;margin-bottom:10px">
    Clears all saved network settings and reboots in AP-only mode with default credentials (SSID: NMEA2000-GW, password: nmea2000).<br>
    You can also hold the physical button for 5 seconds.
  </p>
  <button class="btn btn-d" onclick="doReset()">&#9888; Factory Reset</button>
</div>

<p style="margin-top:12px"><a href="/">&#8592; Back to Dashboard</a></p>

<script>
var cur={};
function ld(){
  fetch('/api/status').then(function(r){return r.json();}).then(function(d){
    cur=d;
    var mn=['AP Only','Join Network','Both'],h='';
    h+='<div class="sr"><span>Mode</span><span>'+mn[d.mode]+'</span></div>';
    if(d.ap_ip) h+='<div class="sr"><span>AP IP</span><span>'+d.ap_ip+'</span></div>';
    if(d.ap_ssid) h+='<div class="sr"><span>AP SSID</span><span>'+d.ap_ssid+'</span></div>';
    if(d.sta_ssid) h+='<div class="sr"><span>Station SSID</span><span>'+d.sta_ssid+'</span></div>';
    if(d.sta_ip && d.sta_ip!='0.0.0.0')
      h+='<div class="sr"><span>Station IP</span><span class="'+(d.sta_connected?'ok':'err')+'">'+d.sta_ip+'</span></div>';
    h+='<div class="sr"><span>mDNS</span><span class="ok">nmea2000.local</span></div>';
    document.getElementById('st').innerHTML=h;
    setMode(d.mode);
    document.querySelector('input[name=mode][value="'+d.mode+'"]').checked=true;
    document.getElementById('apSsid').value=d.ap_ssid||'NMEA2000-GW';
    document.getElementById('staSsid').value=d.sta_ssid||'';
  }).catch(function(){document.getElementById('st').innerHTML='Could not load status.';});
}
function setMode(m){
  m=parseInt(m);
  [0,1,2].forEach(function(i){
    document.getElementById('mo'+i).className='mo'+(i===m?' sel':'');
  });
  document.getElementById('apSect').className='card sect'+(m===0||m===2?' vis':'');
  document.getElementById('staSect').className='card sect'+(m===1||m===2?' vis':'');
}
document.getElementById('mg').addEventListener('change',function(e){setMode(e.target.value);});
function doScan(){
  var s=document.getElementById('scanSel');
  s.innerHTML='<option>Scanning...</option>';
  fetch('/api/wifi/scan').then(function(r){return r.json();}).then(function(d){
    s.innerHTML='<option value="">-- select network --</option>';
    d.networks.forEach(function(n){
      var o=document.createElement('option');
      o.value=n.ssid;
      o.textContent=n.ssid+' ('+n.rssi+'dBm'+(n.secure?' [enc]':'')+')';
      s.appendChild(o);
    });
  }).catch(function(){s.innerHTML='<option>Scan failed</option>';});
}
function pickNet(){var v=document.getElementById('scanSel').value;if(v)document.getElementById('staSsid').value=v;}
function doSave(){
  var m=parseInt(document.querySelector('input[name=mode]:checked').value);
  var body={mode:m,
    ap_ssid:document.getElementById('apSsid').value,
    ap_pass:document.getElementById('apPass').value,
    sta_ssid:document.getElementById('staSsid').value,
    sta_pass:document.getElementById('staPass').value};
  var msg=document.getElementById('msg');
  fetch('/api/wifi/save',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)})
    .then(function(r){return r.json();}).then(function(d){
      if(d.ok){msg.className='ok';msg.textContent='Saved! Rebooting in 3s...';setTimeout(function(){window.location.href='/';},6000);}
      else{msg.className='err';msg.textContent='Error: '+(d.error||'unknown');}
    }).catch(function(){msg.className='err';msg.textContent='Request failed.';});
}
function doReset(){
  if(!confirm('Clear all network settings and reboot to default AP mode?'))return;
  fetch('/api/wifi/reset',{method:'POST'}).then(function(){
    var msg=document.getElementById('msg');
    msg.className='ok';msg.textContent='Factory reset! Rebooting...';
  }).catch(function(){});
}
ld();

function loadRuuvi() {
  fetch('/api/ruuvi/labels').then(function(r){return r.json();}).then(function(d){
    var list = document.getElementById('ruuvi-list');
    var sensors = d.sensors || [];
    var active = sensors.filter(function(s){return s.active;});
    if (active.length === 0) {
      list.innerHTML = '<p style="font-size:.8rem;color:#607080">No Ruuvi sensors detected yet.<br>Bring a Ruuvi tag near the gateway — it will appear here automatically.</p>';
      return;
    }
    var h = '<table style="width:100%;border-collapse:collapse">';
    h += '<tr style="font-size:.72rem;color:#607080"><th style="text-align:left;padding:4px 0">Slot</th><th style="text-align:left;padding:4px 6px">MAC</th><th style="text-align:left;padding:4px 0">Label</th><th style="text-align:left;padding:4px 6px">Status</th></tr>';
    sensors.forEach(function(s) {
      if (!s.active) return;
      var status = s.stale ? '<span style="color:#f44336">stale</span>' : '<span style="color:#4caf50">live</span>';
      h += '<tr style="border-top:1px solid #1a2230">';
      h += '<td style="padding:6px 0;font-size:.78rem;color:#607080">'+(s.slot+1)+'</td>';
      h += '<td style="padding:6px;font-size:.72rem;color:#4db6ff;font-family:monospace">'+s.mac+'</td>';
      h += '<td style="padding:6px 0"><input type="text" id="rlbl'+s.slot+'" data-slot="'+s.slot+'" value="'+s.label+'" maxlength="31" style="width:120px"></td>';
      h += '<td style="padding:6px;font-size:.78rem">'+status+'</td>';
      h += '</tr>';
    });
    h += '</table>';
    list.innerHTML = h;
  }).catch(function(){
    document.getElementById('ruuvi-list').innerHTML='<p style="font-size:.8rem;color:#f44336">Failed to load sensor list.</p>';
  });
}
function saveRuuvi() {
  var inputs = document.querySelectorAll('#ruuvi-list input[data-slot]');
  var sensors = [];
  inputs.forEach(function(el){ sensors.push({slot:parseInt(el.dataset.slot), label:el.value}); });
  var msg = document.getElementById('ruuvi-msg');
  fetch('/api/ruuvi/labels',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({sensors:sensors})})
    .then(function(r){return r.json();}).then(function(d){
      msg.style.display='block';
      if(d.ok){msg.style.background='#1a3220';msg.style.color='#66bb6a';msg.textContent='Labels saved.';}
      else{msg.style.background='#321a1a';msg.style.color='#ef5350';msg.textContent='Error saving labels.';}
      setTimeout(function(){msg.style.display='none';},3000);
    }).catch(function(){
      msg.style.display='block';msg.style.background='#321a1a';msg.style.color='#ef5350';msg.textContent='Request failed.';
    });
}
function loadMqtt() {
  fetch('/api/mqtt/config').then(function(r){return r.json();}).then(function(d){
    document.getElementById('mqttHost').value  = d.host  || '';
    document.getElementById('mqttPort').value  = d.port  || '8883';
    document.getElementById('mqttUser').value  = d.user  || '';
    document.getElementById('mqttTopic').value = d.topic || 'boat/myyacht';
    document.getElementById('mqttEnabled').checked = !!d.enabled;
    var st = document.getElementById('mqtt-st');
    if (!d.enabled) { st.textContent='Disabled'; st.style.color='#607080'; }
    else if (d.connected) { st.textContent='Connected'; st.style.color='#4caf50'; }
    else { st.textContent='Disconnected'; st.style.color='#f44336'; }
  }).catch(function(){
    document.getElementById('mqtt-st').textContent='Could not load';
  });
}
function saveMqtt() {
  var body = {
    host:    document.getElementById('mqttHost').value,
    port:    parseInt(document.getElementById('mqttPort').value) || 8883,
    user:    document.getElementById('mqttUser').value,
    pass:    document.getElementById('mqttPass').value,
    topic:   document.getElementById('mqttTopic').value,
    enabled: document.getElementById('mqttEnabled').checked
  };
  var msg = document.getElementById('mqtt-msg');
  fetch('/api/mqtt/save',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)})
    .then(function(r){return r.json();}).then(function(d){
      msg.style.display='block';
      if(d.ok){msg.style.background='#1a3220';msg.style.color='#66bb6a';msg.textContent='Saved! Reconnecting…';loadMqtt();}
      else{msg.style.background='#321a1a';msg.style.color='#ef5350';msg.textContent='Error: '+(d.error||'unknown');}
      setTimeout(function(){msg.style.display='none';},4000);
    }).catch(function(){
      msg.style.display='block';msg.style.background='#321a1a';msg.style.color='#ef5350';msg.textContent='Request failed.';
    });
}
loadMqtt();
loadRuuvi();
setInterval(loadMqtt, 5000);
</script>
</body>
</html>
)=====";
