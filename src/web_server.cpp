#include "web_server.h"
#include "config.h"
#include "channels.h"
#include "failsafe.h"
#include "udp_control.h"
#include <WebServer.h>

static WebServer server(80);

// Served straight from flash (PROGMEM) - built once at compile time
// instead of re-concatenated into a String on every request.
static const char INDEX_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html><html><head><title>ESP Drone Modes</title><style>
body{font-family:Arial;text-align:center;background:#1a1a1a;color:white;}
h1{margin-top:20px;}
.buttons{display:grid;grid-template-columns:repeat(2,150px);grid-gap:20px;justify-content:center;margin-top:50px;}
.switch{position:relative;display:inline-block;width:60px;height:34px;}
.switch input{display:none;}
.slider{position:absolute;cursor:pointer;top:0;left:0;right:0;bottom:0;background:#ccc;transition:.4s;border-radius:34px;}
.slider:before{position:absolute;content:'';height:26px;width:26px;left:4px;bottom:4px;background:white;transition:.4s;border-radius:50%;}
input:checked + .slider{background:#4CAF50;}
input:checked + .slider:before{transform:translateX(26px);}
.status{margin-top:30px;font-size:18px;}
.linkbad{color:#ff5555;font-weight:bold;}
.estop{margin-top:40px;}
.estop button{font-size:22px;font-weight:bold;padding:18px 36px;border-radius:10px;border:none;cursor:pointer;}
.estop-on{background:#cc2222;color:white;}
.estop-off{background:#444;color:#888;}
.estop-active-banner{color:#ff5555;font-weight:bold;font-size:20px;margin-top:15px;}
</style></head><body>
<h1>ESP Drone Modes</h1>
<div class='buttons'>
<label class='switch'><input type='checkbox' id='aux1' onchange="toggle(1)"><span class='slider'></span></label> Mode 1
<label class='switch'><input type='checkbox' id='aux2' onchange="toggle(2)"><span class='slider'></span></label> Mode 2
<label class='switch'><input type='checkbox' id='aux3' onchange="toggle(3)"><span class='slider'></span></label> Mode 3
<label class='switch'><input type='checkbox' id='aux4' onchange="toggle(4)"><span class='slider'></span></label> Mode 4
</div>
<div class='status' id='status'>Loading...</div>
<div class='estop'>
  <button id='estopBtn' onclick='toggleEstop()'>E-STOP</button>
  <div id='estopBanner'></div>
</div>
<script>
let estopState = false;
function toggle(ch){ fetch('/aux?ch=' + ch); }
function toggleEstop(){
  fetch('/estop?set=' + (estopState ? '0' : '1'));
}
function renderEstop(active){
  estopState = active;
  let btn = document.getElementById('estopBtn');
  btn.textContent = active ? 'CLEAR E-STOP' : 'E-STOP';
  btn.className = active ? 'estop-on' : 'estop-off';
  document.getElementById('estopBanner').innerHTML =
    active ? "<span class='estop-active-banner'>E-STOP LATCHED - throttle forced to 0</span>" : '';
}
async function update(){
  try {
    let r = await fetch('/status');
    let j = await r.json();
    let linkClass = j.linkOk ? '' : 'linkbad';
    document.getElementById('status').innerHTML =
      "<span class='" + linkClass + "'>Link: " + (j.linkOk ? 'OK' : 'LOST') + "</span><br>" +
      'Throttle: ' + j.throttle + '%<br>Roll: ' + j.roll + '%<br>Pitch: ' + j.pitch + '%<br>Yaw: ' + j.yaw + '%<br>' +
      'Packets lost: ' + j.lost + '<br>Packets rejected (bad auth/replay): ' + j.rejected;
    for(let i=1;i<=4;i++){ document.getElementById('aux'+i).checked = j['aux'+i]; }
    renderEstop(j.estop);
  } catch(e) {
    document.getElementById('status').innerHTML = "<span class='linkbad'>No response from ESP32</span>";
  }
}
setInterval(update, 500);
update();
</script></body></html>
)HTML";

static void handleRoot() {
  server.send_P(200, "text/html", INDEX_HTML);
}

static void handleAux() {
  if (!server.hasArg("ch")) {
    server.send(400, "text/plain", "Bad Request");
    return;
  }
  int ch = server.arg("ch").toInt();
  if (ch < 1 || ch > 4) {
    server.send(400, "text/plain", "Bad Request");
    return;
  }
  channelsToggleAux(ch);
  server.send(200, "text/plain", "OK");
}

static void handleStatus() {
  uint16_t snap[IBUS_CHANNELS];
  channelsGetSnapshot(snap, IBUS_CHANNELS);
  bool linkOk = failsafeIsLinkOk();

  char json[320];
  snprintf(json, sizeof(json),
    "{\"linkOk\":%s,\"throttle\":%d,\"roll\":%d,\"pitch\":%d,\"yaw\":%d,"
    "\"aux1\":%s,\"aux2\":%s,\"aux3\":%s,\"aux4\":%s,\"lost\":%lu,"
    "\"rejected\":%lu,\"estop\":%s}",
    linkOk ? "true" : "false",
    (snap[CH_THROTTLE] - PWM_MIN) * 100 / (PWM_MAX - PWM_MIN),
    (snap[CH_ROLL]     - PWM_MIN) * 100 / (PWM_MAX - PWM_MIN),
    (snap[CH_PITCH]    - PWM_MIN) * 100 / (PWM_MAX - PWM_MIN),
    (snap[CH_YAW]      - PWM_MIN) * 100 / (PWM_MAX - PWM_MIN),
    channelsGetAux(1) ? "true" : "false",
    channelsGetAux(2) ? "true" : "false",
    channelsGetAux(3) ? "true" : "false",
    channelsGetAux(4) ? "true" : "false",
    (unsigned long)udpControlPacketLossCount(),
    (unsigned long)udpControlRejectedCount(),
    channelsIsEstop() ? "true" : "false");

  server.send(200, "application/json", json);
}

static void handleEstop() {
  if (!server.hasArg("set")) {
    server.send(400, "text/plain", "Bad Request");
    return;
  }
  bool latch = server.arg("set").toInt() != 0;
  channelsSetEstop(latch);
  server.send(200, "text/plain", "OK");
}

void webServerInit() {
  server.on("/", handleRoot);
  server.on("/aux", handleAux);
  server.on("/status", handleStatus);
  server.on("/estop", handleEstop);
  server.begin();
  Serial.println("[Web] Server started");
}

void webServerPoll() {
  server.handleClient();
}
