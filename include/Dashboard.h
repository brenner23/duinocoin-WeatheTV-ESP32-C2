#ifndef DASHBOARD_H
#define DASHBOARD_H

const char WEBSITE[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="de">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Duino-Coin @@DEVICE@@</title>
<style>
  *{box-sizing:border-box}
  body{
    margin:0;background:#0d0f0f;color:#f3f3f3;
    font-family:Arial,Helvetica,sans-serif;
  }
  .wrap{max-width:760px;margin:0 auto;padding:28px 18px 48px}
  h1{margin:0 0 6px;color:#00ff2a;font-size:30px}
  .sub{color:#8d9696;margin:0 0 22px;font-size:14px}
  .card{
    background:#181b1b;border-radius:10px;padding:18px;margin:0 0 14px;
    box-shadow:0 0 0 1px rgba(255,255,255,.02) inset;
  }
  .title{
    color:#00eaff;font-weight:700;letter-spacing:2px;font-size:13px;
    margin-bottom:14px;
  }
  .grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:10px}
  .stat{
    background:#101212;border:1px solid #2a2f2f;border-radius:7px;padding:12px;
  }
  .value{font-size:25px;font-weight:700;color:#fff}
  .label{font-size:11px;color:#7f8a8a;letter-spacing:1.2px;margin-top:4px}
  .range-head{display:flex;justify-content:space-between;align-items:center;margin-bottom:7px}
  .percent{color:#00ff2a;font-weight:700}
  input[type=range]{width:100%;accent-color:#00ff2a}
  input[type=number]{width:100%;background:#101212;color:#fff;border:1px solid #2a2f2f;border-radius:6px;padding:12px;font-size:18px}
  .wallet-balance{font-size:28px;font-weight:700;color:#00ff2a;margin-bottom:5px;overflow-wrap:anywhere}
  .wallet-usd{font-size:16px;color:#00eaff;margin:0 0 7px}
  .wallet-meta{font-size:12px;color:#8d9696;margin-bottom:14px}
  .btnrow{display:grid;grid-template-columns:1fr 1fr;gap:10px}
  .miner-summary{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:8px;margin:14px 0}
  .mini{background:#101212;border:1px solid #2a2f2f;border-radius:7px;padding:10px;text-align:center}
  .mini b{display:block;font-size:18px;color:#fff}.mini span{font-size:10px;color:#7f8a8a;letter-spacing:.8px}
  .tablewrap{overflow-x:auto;margin-top:12px}
  table{width:100%;border-collapse:collapse;font-size:12px;white-space:nowrap}
  th,td{padding:8px 7px;border-bottom:1px solid #2a2f2f;text-align:left}
  th{color:#8d9696;font-size:10px;letter-spacing:.7px}
  .refresh{background:#00eaff;color:#001417}
  .hint{font-size:11px;color:#6d7474;margin-top:8px}
  .btn{
    width:100%;border:0;border-radius:6px;padding:13px 14px;
    font-weight:700;font-size:15px;cursor:pointer;margin-top:10px;
  }
  .save{background:#00ff19;color:#001500}
  .restart{background:#232727;color:#ff6262;border:1px solid #4b2a2a}
  .status{min-height:18px;text-align:center;margin-top:10px;color:#00eaff;font-size:13px}
  .crash-entry{background:#101212;border:1px solid #4b2a2a;border-radius:7px;padding:12px;margin-top:10px}
  .crash-head{color:#ff6262;font-weight:700;letter-spacing:1px;margin-bottom:9px}
  .crash-grid{display:grid;grid-template-columns:130px 1fr;gap:5px 10px;font-size:12px}
  .crash-grid span{color:#7f8a8a}.crash-grid b{color:#f3f3f3;overflow-wrap:anywhere}
  .crash-empty{color:#8d9696;font-size:13px;padding:4px 0}
  .clearlog{background:#232727;color:#ffb762;border:1px solid #51402a}
  .footer{text-align:center;color:#596161;font-size:11px;margin-top:20px}
  @media(max-width:560px){.grid{grid-template-columns:1fr}.wrap{padding:18px 12px}}
</style>
</head>
<body>
<div class="wrap">
  <h1>⚙ Duino-Coin Miner</h1>
  <p class="sub">@@DEVICE@@ · @@ID@@ · <span id="ip">@@IP_ADDR@@</span></p>
  <div class="card" style="padding:12px 18px"><div style="display:flex;justify-content:space-between;gap:12px;align-items:center;flex-wrap:wrap"><div><span id="onlineLed" style="display:inline-block;width:10px;height:10px;border-radius:50%;background:#00ff2a;margin-right:7px"></span><b id="onlineText">ONLINE</b></div><div><span class="label">MINING TIME</span> <b id="miningTime">0Y 0M 0W 0D 00:00:00</b></div><div class="label">LAST UPDATE <span id="lastUpdate">--:--:--</span></div></div></div>

  <div class="card">
    <div class="title">MINING</div>
    <div class="grid">
      <div class="stat"><div class="value"><span id="liveHashrate">@@HASHRATE@@</span> <small>kH/s</small></div><div class="label">HASHRATE</div></div>
      <div class="stat"><div class="value" id="liveDifficulty">@@DIFF@@</div><div class="label">DIFFICULTY</div></div>
      <div class="stat"><div class="value" id="liveShares">@@SHARES@@</div><div class="label">SHARES</div></div>
      <div class="stat"><div class="value" id="liveNode">@@NODE@@</div><div class="label">NODE</div></div>
    </div>
  </div>

  <div class="card">
    <div class="title">GERÄT</div>
    <div class="grid">
      <div class="stat"><div class="value" id="liveHeap">@@MEMORY@@</div><div class="label">FREIER SPEICHER</div></div>
      <div class="stat"><div class="value">@@VERSION@@</div><div class="label">MINER VERSION</div></div>
      @@CPU_TEMP_FIELDS@@
    </div>
  </div>

  <div class="card">
    <div class="title">WALLET</div>
    <div class="wallet-balance"><span id="walletBalance">@@WALLET_BALANCE@@</span> DUCO</div>
    <div class="wallet-usd">≈ $<span id="walletUsd">@@WALLET_USD@@</span> USD</div>
    <div class="wallet-meta">Letztes Update: <span id="walletLastUpdate">@@WALLET_LAST_UPDATE@@</span></div>
    <div class="miner-summary">
      <div class="mini"><b>@@ACTIVE_DEVICES@@</b><span>GERÄTE</span></div>
      <div class="mini"><b>@@ACTIVE_THREADS@@</b><span>THREADS</span></div>
      <div class="mini"><b>@@TOTAL_HASHRATE@@</b><span>GESAMT</span></div>
    </div>
    <div class="wallet-meta">Gesamt: @@TOTAL_ACCEPTED@@ Accepted · @@TOTAL_REJECTED@@ Rejected</div>
    <div class="tablewrap"><table>
      <thead><tr><th>MINER</th><th>IP</th><th>HASHRATE</th><th>DIFF</th><th>A / R</th><th>SHARETIME</th><th>POOL</th></tr></thead>
      <tbody>@@MINER_ROWS@@</tbody>
    </table></div>
    <div class="label" style="margin-bottom:6px">AUTOMATISCHE AKTUALISIERUNG IN SEKUNDEN</div>
    <input id="walletInterval" type="number" min="60" max="86400" step="1" value="@@WALLET_INTERVAL@@">
    <div class="hint">Standard: 300 Sekunden = 5 Minuten. Erlaubt sind 60 bis 86400 Sekunden.</div>
    <div class="btnrow">
      <button class="btn save" onclick="saveWalletInterval()">SPEICHERN</button>
      <button class="btn refresh" onclick="refreshWallet()">JETZT AKTUALISIEREN</button>
    </div>
    <div id="walletStatus" class="status"></div>
  </div>

  <div class="card">
    <div class="title">HELLIGKEIT</div>
    <div class="range-head">
      <span>Display-Backlight</span>
      <span class="percent"><span id="brightnessValue">@@BRIGHTNESS@@</span>%</span>
    </div>
    <input id="brightness" type="range" min="0" max="100" step="1" value="@@BRIGHTNESS@@">
    <div class="hint">Die Helligkeit wird beim Schieben sofort live übernommen.</div>
    <button class="btn save" onclick="saveBrightness()">SPEICHERN</button>
    <div id="status" class="status"></div>
  </div>


  @@CRASH_CARD@@


  <div class="card">
    <div class="title">SYSTEM</div>
    <button class="btn restart" onclick="restartMiner()">ESP32 NEU STARTEN</button>
  </div>

  <div class="footer">Duino-Coin · lokales Miner-Dashboard</div>
</div>

<script>
const slider = document.getElementById('brightness');
const value = document.getElementById('brightnessValue');
const statusBox = document.getElementById('status');
let liveTimer = 0;
let brightnessRequestRunning = false;
let pendingBrightness = null;

function sendBrightness(v) {
  if (brightnessRequestRunning) {
    pendingBrightness = v;
    return;
  }

  brightnessRequestRunning = true;
  fetch('/brightness?value=' + v)
    .then(r => r.text())
    .then(() => { statusBox.textContent = 'Live: ' + v + '%'; })
    .catch(() => { statusBox.textContent = 'Verbindung fehlgeschlagen'; })
    .finally(() => {
      brightnessRequestRunning = false;
      if (pendingBrightness !== null) {
        const next = pendingBrightness;
        pendingBrightness = null;
        sendBrightness(next);
      }
    });
}

slider.addEventListener('input', () => {
  value.textContent = slider.value;
  clearTimeout(liveTimer);
  liveTimer = setTimeout(() => sendBrightness(slider.value), 300);
});

function saveBrightness(){
  fetch('/brightness/save?value=' + slider.value)
    .then(r => r.text())
    .then(t => { statusBox.textContent = t; })
    .catch(() => { statusBox.textContent = 'Speichern fehlgeschlagen'; });
}

function saveWalletInterval(){
  const box = document.getElementById('walletInterval');
  const status = document.getElementById('walletStatus');
  const v = parseInt(box.value, 10);
  if (!Number.isFinite(v) || v < 60 || v > 86400) {
    status.textContent = 'Bitte 60 bis 86400 Sekunden eingeben';
    return;
  }
  fetch('/wallet/interval?value=' + v)
    .then(r => r.text().then(t => { if(!r.ok) throw new Error(t); return t; }))
    .then(t => { status.textContent = t; })
    .catch(e => { status.textContent = e.message || 'Speichern fehlgeschlagen'; });
}

function refreshWallet(){
  const status = document.getElementById('walletStatus');
  status.textContent = 'Wallet wird aktualisiert...';
  fetch('/wallet/refresh', {method:'POST'})
    .then(r => r.text().then(t => { if(!r.ok) throw new Error(t); return t; }))
    .then(() => {
      status.textContent = 'Wallet aktualisiert';
      fetchLive();
    })
    .catch(e => { status.textContent = e.message || 'Aktualisierung fehlgeschlagen'; });
}

function clearCrashlog(){
  if(!confirm('Alle gespeicherten Fehlerberichte löschen?')) return;
  const box = document.getElementById('crashStatus');
  fetch('/crashlog/clear', {method:'POST'})
    .then(r => r.text().then(t => { if(!r.ok) throw new Error(t); return t; }))
    .then(t => { box.textContent = t; fetchDiag(); })
    .catch(e => { box.textContent = e.message || 'Löschen fehlgeschlagen'; });
}


let miningSeconds = 0;
let failedLiveRequests = 0;
function fmtMiningTime(sec){
  sec=Math.max(0,Math.floor(sec)); const Y=365*86400,M=30*86400,W=7*86400,D=86400;
  const y=Math.floor(sec/Y); sec%=Y; const m=Math.floor(sec/M); sec%=M; const w=Math.floor(sec/W); sec%=W; const d=Math.floor(sec/D); sec%=D;
  const h=Math.floor(sec/3600); sec%=3600; const mi=Math.floor(sec/60), ss=sec%60;
  return y+'Y '+m+'M '+w+'W '+d+'D '+String(h).padStart(2,'0')+':'+String(mi).padStart(2,'0')+':'+String(ss).padStart(2,'0');
}
function setOnline(ok){
  if(ok){failedLiveRequests=0;document.getElementById('onlineLed').style.background='#00ff2a';document.getElementById('onlineText').textContent='ONLINE';}
  else if(++failedLiveRequests>=2){document.getElementById('onlineLed').style.background='#ff3b3b';document.getElementById('onlineText').textContent='OFFLINE';}
}
function put(id,v){const e=document.getElementById(id);if(e&&v!==undefined)e.textContent=v;}
function fetchLive(){
  fetch('/api/status?t='+Date.now(),{cache:'no-store'}).then(r=>{if(!r.ok)throw 0;return r.json()}).then(d=>{
    setOnline(true); miningSeconds=Number(d.uptime_sec||0); put('liveHashrate',Number(d.hashrate||0).toFixed(1)); put('liveDifficulty',d.difficulty); put('liveShares',d.rejected+'/'+d.accepted); put('liveNode',d.node); put('liveHeap',d.free_heap);
    if(d.cpu_temp!==undefined){put('cpuTemp',Number(d.cpu_temp).toFixed(1)+' °C');put('cpuTempMax',Number(d.cpu_temp_max).toFixed(1)+' °C');}
    put('walletBalance',d.wallet_balance);put('walletUsd',d.wallet_usd);put('walletLastUpdate',d.wallet_last_update);
    put('lastUpdate',new Date().toLocaleTimeString()); fetchDiag();
  }).catch(()=>setOnline(false));
}
function fetchDiag(){
  fetch('/api/diag?t='+Date.now(),{cache:'no-store'}).then(r=>r.json()).then(d=>{
    if(d.crashlog_html!==undefined){const e=document.getElementById('crashlog');if(e)e.innerHTML=d.crashlog_html;}
  }).catch(()=>{});
}
setInterval(()=>{miningSeconds++;put('miningTime',fmtMiningTime(miningSeconds));},1000);
setInterval(fetchLive,60000);
setTimeout(fetchLive,500);

function restartMiner(){
  if(!confirm('ESP32 wirklich neu starten?')) return;
  statusBox.textContent = 'Neustart...';
  fetch('/restart', {method:'POST'}).catch(()=>{});
}
</script>
</body>
</html>
)=====";

#endif
