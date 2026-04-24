#include "web_handlers.h"

#include <LittleFS.h>
#include <ArduinoJson.h>

#include "app_state.h"
#include "storage.h"
#include "wifi_manager.h"
#include "mqtt_ha.h"

static String prettyDeviceNameForUi() {
  String s = deviceId;
  s.replace('_', ' ');
  s.trim();
  if (s.length() == 0) s = deviceId;
  return s;
}

void sendRebootingPage(const String& title, const String& detail, int countdownSec, uint32_t startPollDelayMs) {
  String html =
    F("<!doctype html><html><head><meta charset='utf-8'>"
      "<meta name='viewport' content='width=device-width,initial-scale=1'>"
      "<style>"
      "body{font-family:system-ui,-apple-system,Segoe UI,Roboto,Arial;padding:28px;line-height:1.5}"
      ".pill{display:inline-block;padding:6px 10px;border:1px solid #e5e5e5;border-radius:999px;margin:6px 0}"
      ".muted{color:#666;font-size:13px}"
      "</style></head><body><h2>");
  html += title;
  html += F(" &#10003;</h2><div class='pill'>");
  html += prettyDeviceNameForUi();
  html += F("</div><p>Rebooting the controller... <b><span id='s'>");
  html += String(countdownSec);
  html += F("</span>s</b></p>");
  if (detail.length()) { html += F("<p class='muted'>"); html += detail; html += F("</p>"); }

  html += F("<script>"
            "var s="); html += String(countdownSec); html += F(";"
            "function tick(){var el=document.getElementById('s'); if(el){el.textContent=s;} if(s>0){s--; setTimeout(tick,900);} }"
            "function poll(){fetch('/status',{cache:'no-store'})"
              ".then(r=>r.json()).then(()=>location.replace('/'))"
              ".catch(()=>setTimeout(poll,700));}"
            "tick();"
            "setTimeout(poll,"); html += String(startPollDelayMs); html += F(");"
            "</script></body></html>");

  server.sendHeader("Cache-Control","no-store");
  server.send(200, "text/html", html);
}

static inline void pageBegin(const String& title) {
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", "");
  server.sendContent(
    F("<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>"
      "<title>Somfy RTS - ")
  );
  server.sendContent(title);
  server.sendContent(
    F("</title>"
      "<style>"
      ":root{--bg:#fff;--fg:#111;--muted:#666;--card:#fff;--border:#e5e5e5}"
      "body.dark {--bg:#0f1216;--fg:#e5e7eb;--muted:#9ca3af;--card:#161a20;--border:#273245;}"
      "body{background:var(--bg);color:var(--fg);font-family:system-ui,-apple-system,Segoe UI,Roboto,Arial,sans-serif;margin:0;line-height:1.35}"
      ".nav{position:sticky;top:0;z-index:10;display:flex;gap:10px;align-items:center;padding:10px 14px;border-bottom:1px solid var(--border);background:var(--bg)}"
      ".nav .sp{flex:1}"
      ".btn{display:inline-block;padding:8px 12px;border:1px solid var(--border);border-radius:10px;background:var(--card);cursor:pointer}"
      "a{color:inherit;text-decoration:none}"
      "a.btn{text-decoration:none;color:inherit}"
      ".container{padding:16px}"
      ".card{border:1px solid var(--border);border-radius:12px;padding:12px;margin:10px 0;background:var(--card)}"
      ".muted{color:var(--muted);font-size:12px}"
      ".grid2{display:grid;grid-template-columns:repeat(2,minmax(280px,1fr));gap:12px}"
      "@media(max-width:900px){.grid2{grid-template-columns:1fr}}"
      ".cardb{border:1px solid var(--border);border-radius:12px;padding:10px;background:var(--card)}"
      ".title{font-weight:600;margin:0 0 6px}"
      ".group{display:flex;gap:6px;flex-wrap:wrap}"
      ".btn-prog{border-color:#f59e0b;background:rgba(245,158,11,.08)}"
      ".rowline{display:flex;justify-content:space-between;align-items:center;gap:12px;margin:6px 0}"
      ".in{flex:1;min-width:120px;padding:6px;border:1px solid var(--border);border-radius:8px;background:var(--card)}"
      ".status{display:flex;gap:12px;align-items:center;margin:10px 0}"
      ".badge{display:inline-flex;align-items:center;gap:6px;font-size:12px;padding:4px 8px;border-radius:999px;background:var(--card);border:1px solid var(--border)}"
      ".dot{width:8px;height:8px;border-radius:50%;background:#bbb}"
      ".ok .dot{background:#19c37d}.err .dot{background:#ef4444}"
      ".bar{display:flex;justify-content:space-between;align-items:center;gap:10px;flex-wrap:wrap}"
      ".btn-sm{padding:6px 10px;font-size:13px;border-radius:8px}"
      "input,select,button,textarea{background:var(--card);color:var(--fg);border:1px solid var(--border);border-radius:8px;padding:6px 8px}"
      "button.btn{cursor:pointer}"
      "button.btn:hover{filter:brightness(0.95)}"
      ".btn-warn{border-color:#e03131;background:#ffe3e3;color:#e03131}"
      "body.dark .btn-warn{background:rgba(224,49,49,.1);color:#ff6b6b}"
      ".btn-x{width:30px;height:30px;padding:0;line-height:1;font-weight:700;border-color:#e03131;background:#ffe3e3;color:#e03131}"
      "body.dark .btn-x{background:rgba(224,49,49,.1);color:#ff6b6b}"
      "</style>"
      "<script>"
      "function upd(b){"
        "const m=document.getElementById('mqttBadge');"
        "const mt=document.getElementById('mqttText');"
        "if(m){m.classList.toggle('ok', b.mqtt_connected); m.classList.toggle('err', !b.mqtt_connected);}"
        "if(mt) mt.textContent = b.mqtt_connected ? ('MQTT: ' + (b.mqtt_broker||'')) : 'MQTT disconnected';"
        "const sb=document.getElementById('staBadge');"
        "const st=document.getElementById('staText');"
        "const staUp = !!(b.sta_ip && b.sta_ip.length);"
        "if(sb){sb.classList.toggle('ok', staUp); sb.classList.toggle('err', !staUp);}"
        "if(st) st.textContent = staUp ? ('IP: ' + (b.sta_ip||'')) : 'No IP';"
        "const wb=document.getElementById('wifiBadge');"
        "const wt=document.getElementById('wifiText');"
        "if(wb){wb.classList.toggle('ok', staUp); wb.classList.toggle('err', !staUp);}"
        "if(wt) wt.textContent = staUp ? ('Wi-Fi: ' + (b.sta_ssid||'') + ' (' + (b.sta_rssi||0) + '%)') : 'Wi-Fi: Disconnected';"
        "const a=document.getElementById('apText');"
        "if(a) a.textContent = b.ap_active ? ('AP: ' + (b.ap_ssid||'')) : 'AP: Off';"
      "}"
      "function poll(){fetch('/status',{cache:'no-store'}).then(r=>r.json()).then(upd).catch(()=>{});}"
      "window.addEventListener('DOMContentLoaded',()=>{poll();setInterval(poll,2000);});"
      "function applyTheme(){document.body.classList.toggle('dark',localStorage.dark==='1')}"
      "function toggleDark(){localStorage.dark = (localStorage.dark==='1'?'0':'1');applyTheme()}"
      "window.addEventListener('DOMContentLoaded', ()=> {applyTheme();});"
      "</script>"
      "</head><body>"
    ));

  server.sendContent(F("<div class='nav'><strong>ESP Somfy RTS - "));
  server.sendContent(prettyDeviceNameForUi());
  server.sendContent(
    F("</strong><div class='sp'></div>"
      "<a class='btn' href='/'>Home</a>"
      "<a class='btn' href='/config'>Config</a>"
      "<form style='display:inline' method='GET' action='/reboot'>"
        "<button class='btn' type='submit'>Reboot</button>"
      "</form>"
      "<button class='btn' onclick='toggleDark()'>Light/Dark</button>"
    "</div><div class='container'>")
  );

  server.sendContent(F("<div class='status'>"
  "<div id='wifiBadge' class='badge'><span class='dot'></span><span id='wifiText'>Wi-Fi...</span></div>"
  "<div id='staBadge'  class='badge'><span class='dot'></span><span id='staText'>IP...</span></div>"
  "<div id='mqttBadge' class='badge'><span class='dot'></span><span id='mqttText'>MQTT...</span></div>"
  "<div class='badge'><span class='dot' style='background:#888'></span><span id='apText'>AP...</span></div>"
  "</div>"));
}

static inline void pageWrite(const __FlashStringHelper* s) { server.sendContent(s); }
static inline void pageWrite(const String& s) { server.sendContent(s); }
static inline void pageEnd() { server.sendContent(F("</div></body></html>")); }

void handleRootGet() {
  pageBegin("Home");

  pageWrite(F("<h1>Controls</h1>"));
  pageWrite(F("<div class='card'><div class='bar'>"
              "<div class='muted'>Active blinds: "));
  pageWrite(String(blindCount));
  pageWrite(F(" / "));
  pageWrite(String(MAX_BLINDS));
  pageWrite(F("</div>"
              "<form method='POST' action='/blind/add'>"));
  if (blindCount >= MAX_BLINDS) {
    pageWrite(F("<button class='btn btn-sm' type='submit' disabled>Max blinds reached</button>"));
  } else {
    pageWrite(F("<button class='btn btn-sm' type='submit'>Add blind</button>"));
  }
  pageWrite(F("</form></div></div>"));

  pageWrite(F("<div class='card'><div class='grid2'>"));

  for (int i = 1; i <= blindCount; i++) {
    pageWrite(F("<div class='cardb'>"));
    pageWrite(F("<div class='rowline' style='margin:0 0 6px 0'>"));
    pageWrite(F("<div class='title' style='margin:0'>"));
    pageWrite(blindName(i));
    pageWrite(F("</div><form method='POST' action='/blind/remove' style='margin:0' onsubmit=\"return confirm('Remove this blind?');\">"
                "<input type='hidden' name='b' value='"));
    pageWrite(String(i));
    pageWrite(F("'>"));
    if (blindCount <= MIN_BLINDS) {
      pageWrite(F("<button class='btn btn-sm btn-x' type='submit' disabled title='Minimum 1 blind'>X</button>"));
    } else {
      pageWrite(F("<button class='btn btn-sm btn-x' type='submit' title='Remove blind'>X</button>"));
    }
    pageWrite(F("</form></div>"));

    char hexbuf[16]; sprintf(hexbuf, "%06X", remoteId[i]);
    pageWrite(F("<div class='rowline'><div class='muted'>Remote ID</div><div class='muted'>0x"));
    pageWrite(String(hexbuf));
    pageWrite(F("</div></div>"));

    pageWrite(F("<div class='rowline'><div class='muted'>Rolling</div><div class='muted'>"));
    pageWrite(String(rollingCode[i]));
    pageWrite(F("</div></div>"));

    pageWrite(F("<div class='rowline' style='margin-top:8px;align-items:stretch'>"
                "<form method='POST' action='/rename' style='display:flex;gap:6px;align-items:center;flex:1;margin:0'>"
                "<input type='hidden' name='b' value='"));
    pageWrite(String(i));
    pageWrite(F("'><input class='in' name='name' type='text' style='flex:1;min-width:140px' value='"));
    pageWrite(blindName(i));
    pageWrite(F("'><button class='btn btn-sm' type='submit'>Save name</button></form>"
                "<form method='POST' action='/rename' style='display:flex;gap:6px;align-items:center;margin:0'>"
                "<input type='hidden' name='b' value='"));
    pageWrite(String(i));
    pageWrite(F("'><select name='type' style='min-width:150px' onchange='this.form.submit()'>"));
    pageWrite(F("<option value='0'"));
    if (blindTypes[i] == BLIND_TYPE_BLIND) pageWrite(F(" selected"));
    pageWrite(F(">Blind</option>"));
    pageWrite(F("<option value='1'"));
    if (blindTypes[i] == BLIND_TYPE_SHADE) pageWrite(F(" selected"));
    pageWrite(F(">Shade</option>"));
    pageWrite(F("<option value='2'"));
    if (blindTypes[i] == BLIND_TYPE_SHUTTER) pageWrite(F(" selected"));
    pageWrite(F(">Roller Shutter</option>"));
    pageWrite(F("<option value='3'"));
    if (blindTypes[i] == BLIND_TYPE_CURTAIN) pageWrite(F(" selected"));
    pageWrite(F(">Curtain</option>"));
    pageWrite(F("<option value='4'"));
    if (blindTypes[i] == BLIND_TYPE_AWNING) pageWrite(F(" selected"));
    pageWrite(F(">Awning</option>"));
    pageWrite(F("<option value='5'"));
    if (blindTypes[i] == BLIND_TYPE_WINDOW) pageWrite(F(" selected"));
    pageWrite(F(">Window</option>"));
    pageWrite(F("</select></form></div>"));

    pageWrite(F("<div class='bar' style='margin-top:6px'>"));
    pageWrite(F("<div class='group'>"));
    pageWrite(F("<button class='btn btn-sm act' data-b='")); pageWrite(String(i)); pageWrite(F("' data-c='OPEN'>Open</button>"));
    pageWrite(F("<button class='btn btn-sm act' data-b='")); pageWrite(String(i)); pageWrite(F("' data-c='STOP'>Stop</button>"));
    pageWrite(F("<button class='btn btn-sm act' data-b='")); pageWrite(String(i)); pageWrite(F("' data-c='CLOSE'>Close</button>"));
    pageWrite(F("<button class='btn btn-sm btn-prog act' data-b='")); pageWrite(String(i)); pageWrite(F("' data-c='PROG'>Prog</button>"));
    pageWrite(F("</div>"));

    pageWrite(F("<form method='POST' action='/regen' "
                "onsubmit=\"return confirm('Regenerate Remote ID for this blind?');\">"
                "<input type='hidden' name='b' value='"));
    pageWrite(String(i));
    pageWrite(F("'><button class='btn btn-sm btn-warn' type='submit'>Regenerate Remote ID</button></form>"));

    pageWrite(F("</div>"));
    pageWrite(F("</div>"));
  }
  pageWrite(F("</div></div>"));

  pageWrite(F(
    "<script>"
    "document.querySelectorAll('.act').forEach(b=>b.addEventListener('click',e=>{"
      "const k=e.currentTarget;k.disabled=true;"
      "fetch('/api/cmd',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},"
        "body:'b='+k.dataset.b+'&cmd='+k.dataset.c})"
        ".then(r=>r.json()).then(()=>{})"
        ".catch(()=>{})"
        ".finally(()=>{k.disabled=false;});"
    "}));"
    "</script>"
  ));

  pageEnd();
}

void handleApPortalGet() {
  String s;
  s.reserve(3000);
  s +=
  "<!doctype html><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
  "<title>ESPSomfyRemote - Setup</title>"
  "<style>"
  "body{font-family:system-ui,-apple-system,Segoe UI,Roboto,Arial,sans-serif;margin:14px;line-height:1.35}"
  "h1{font-size:18px;margin:0 0 10px}"
  ".card{border:1px solid #ddd;border-radius:10px;padding:12px;margin:10px 0}"
  "label{display:block;margin:6px 0 2px;font-size:13px;color:#333}"
  "input,select,button{padding:8px;border:1px solid #ccc;border-radius:8px}"
  ".row{display:grid;grid-template-columns:1fr 1fr;gap:8px}"
  ".btn{cursor:pointer;background:#fafafa}"
  ".muted{color:#666;font-size:12px}"
  "</style>";

  s += "<h1>ESPSomfyRemote - Setup</h1>";
  s += "<div class='card'><div class='muted'>Device is in Access Point mode. Connect to Wi-Fi below or restore a backup.</div></div>";

  s += "<div class='card'><h3>Wi-Fi</h3>"
       "<form method='POST' action='/ap_portal_config'>"
       "<label>SSID</label><input name='wifi_ssid' type='text' value=''>"
       "<label>Password</label><input name='wifi_pass' type='password' value=''>"
       "<div style='margin-top:8px;display:flex;gap:8px;flex-wrap:wrap'>"
       "<button class='btn' type='submit'>Save & Connect</button>"
       "<button class='btn' type='button' onclick='scanW()'>Scan</button>"
       "<select id='nets' style='min-width:220px'></select>"
       "</div>"
       "</form>"
       "<div class='muted' style='margin-top:6px'>Tip: Tap <b>Scan</b>, pick a network, then press <b>Save & Connect</b>.</div>"
       "</div>";

  s += "<div class='card'><h3>Restore Backup</h3>"
       "<form method='POST' action='/restore_backup' enctype='multipart/form-data'>"
       "<input type='file' name='upload' accept='.json'> "
       "<button class='btn' type='submit'>Restore</button>"
       "</form>"
       "<div class='muted' style='margin-top:6px'>Uploads a JSON created via the <b>Backup</b> button.</div>"
       "</div>";

  s += "<script>"
       "function scanW(){fetch('/wifi_scan',{cache:'no-store'}).then(r=>r.json()).then(arr=>{"
       " const sel=document.getElementById('nets'); sel.innerHTML='';"
       " arr.sort((a,b)=>b.rssi-a.rssi).forEach(n=>{"
       "   const o=document.createElement('option');"
       "   o.text=(n.ssid||'(hidden)')+'  ['+n.rssi+'dBm]'; o.value=n.ssid; sel.add(o);"
       " });"
       " sel.onchange=()=>{document.querySelector(\"input[name='wifi_ssid']\").value=sel.value};"
       "}).catch(()=>alert('Scan failed'));}"
       "</script>";

  server.send(200, "text/html; charset=utf-8", s);
}

void handleApPortalConfigPost() {
  if (server.hasArg("wifi_ssid")) setStr(cfg.wifi_ssid, sizeof(cfg.wifi_ssid), server.arg("wifi_ssid"));
  if (server.hasArg("wifi_pass")) setStr(cfg.wifi_pass, sizeof(cfg.wifi_pass), server.arg("wifi_pass"));
  saveConfig();

  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(cfg.wifi_ssid, cfg.wifi_pass);

  unsigned long t0 = millis();
  bool ok = false;
  while (millis() - t0 < 12000) {
    if (WiFi.status() == WL_CONNECTED) { ok = true; break; }
    delay(150);
  }

  String s =
    String("<!doctype html><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>")
    + "<title>Wi-Fi</title><style>body{font-family:system-ui;margin:14px}</style>"
    + (ok
      ? String("<h3>Connected!</h3><p>IP: <b>") + WiFi.localIP().toString() + "</b></p>"
        "<p>You can now browse the full UI at that address. This AP may turn off automatically.</p>"
        "<p><a href='/'>Continue</a></p>"
      : "<h3>Failed to connect</h3><p>Please double-check SSID/password and try again.</p>"
        "<p><a href='/'>Back</a></p>");

  server.send(200, "text/html; charset=utf-8", s);
}

void handleConfigPost() {
  if (server.hasArg("device_id")) {
    deviceId = normalizeDeviceId(server.arg("device_id"));
    eepromSaveDeviceId(deviceId);
  }
  if (server.hasArg("wifi_ssid")) setStr(cfg.wifi_ssid, sizeof(cfg.wifi_ssid), server.arg("wifi_ssid"));
  if (server.hasArg("wifi_pass")) setStr(cfg.wifi_pass, sizeof(cfg.wifi_pass), server.arg("wifi_pass"));
  if (server.hasArg("mqtt_server")) setStr(cfg.mqtt_server, sizeof(cfg.mqtt_server), server.arg("mqtt_server"));
  if (server.hasArg("mqtt_port"))   cfg.mqtt_port = server.arg("mqtt_port").toInt();
  if (server.hasArg("mqtt_user"))   setStr(cfg.mqtt_user, sizeof(cfg.mqtt_user), server.arg("mqtt_user"));
  if (server.hasArg("mqtt_pass"))   setStr(cfg.mqtt_pass, sizeof(cfg.mqtt_pass), server.arg("mqtt_pass"));
  if (server.hasArg("ap_ssid"))     apSsid = server.arg("ap_ssid");
  if (server.hasArg("ap_pass"))     apPass = server.arg("ap_pass");
  cfg.ha_discovery = server.hasArg("ha_discovery");

  if (server.hasArg("tx_pin"))  txPin = sanitizeGpio(server.arg("tx_pin").toInt());
  if (server.hasArg("led_pin")) ledPin = sanitizeLedGpio(server.arg("led_pin").toInt());
  if (server.hasArg("btn_pin")) buttonPin = sanitizeButtonGpio(server.arg("btn_pin").toInt());
  buttonActiveLow = server.hasArg("btn_invert");
  ledActiveLow = server.hasArg("led_invert");

  saveConfig();

  sendRebootingPage(
    "Saved",
    "If the page does not return, reconnect via the new IP or AP 192.168.4.1.",
    10,
    5000
  );

  if (server.client()) server.client().flush();
  delay(150);
  scheduleReboot(2200);
}

void handleConfigGet() {
  pageBegin("Configuration");

  pageWrite(F(
    "<style>"
    ".form-sec{margin:10px 0}"
    ".form-grid{display:grid;grid-template-columns:repeat(2,minmax(230px,1fr));gap:14px}"
    "@media(max-width:900px){.form-grid{grid-template-columns:repeat(2,minmax(230px,1fr))}}"
    "@media(max-width:600px){.form-grid{grid-template-columns:1fr}}"
    ".field{display:flex;flex-direction:column;gap:6px}"
    ".inline{display:flex;align-items:center;gap:10px;flex-wrap:wrap}"
    ".rowline{display:flex;justify-content:space-between;align-items:center;gap:12px}"
    ".rowline .desc{color:var(--muted)}"
    "</style>"
  ));

  auto gpioOpt = [&](int gpio, const char* label, int current)->String {
    String sel = (current == gpio) ? " selected" : "";
    return String("<option value='") + gpio + "'" + sel + ">" + label + (gpio >= 0 ? (" (GPIO" + String(gpio) + ")") : "") + "</option>";
  };
  auto gpioOptions = [&](int current)->String {
    String o;
    o += gpioOpt(-1,"None",current);
    o += gpioOpt(16,"D0",current); o += gpioOpt(5,"D1",current);  o += gpioOpt(4,"D2",current);
    o += gpioOpt(0,"D3",current);  o += gpioOpt(2,"D4",current);  o += gpioOpt(14,"D5",current);
    o += gpioOpt(12,"D6",current); o += gpioOpt(13,"D7",current); o += gpioOpt(15,"D8",current);
    return o;
  };

  pageWrite(F("<h1>Configuration</h1>"));
  pageWrite(F("<form method='POST' action='/config'>"));

  pageWrite(F("<div class='card form-sec'><h3>Device</h3>"));
  pageWrite(F("<div class='form-grid'>"));
    pageWrite(F("<div class='field'><label>Device ID</label><input name='device_id' type='text' value='"));
    pageWrite(prettyDeviceNameForUi());
    pageWrite(F("'></div>"));
    pageWrite(F("<div class='field'><label>Somfy TX GPIO</label><select name='tx_pin'>"));
    pageWrite(gpioOptions(txPin));
    pageWrite(F("</select></div>"));
  pageWrite(F("</div>"));

  pageWrite(F("<div class='form-grid' style='margin-top:8px'>"));
    pageWrite(F("<div class='field'><label>Link Status LED GPIO</label><div class='inline'><select name='led_pin'>"));
    pageWrite(gpioOptions(ledPin));
    pageWrite(F("</select><label class='inline'><input type='checkbox' name='led_invert' "));
    if (ledActiveLow) pageWrite(F("checked"));
    pageWrite(F("> Active low</label></div></div>"));

    pageWrite(F("<div class='field'><label>Reset Button GPIO (hold 15s)</label><div class='inline'><select name='btn_pin'>"));
    pageWrite(gpioOptions(buttonPin));
    pageWrite(F("</select><label class='inline'><input type='checkbox' name='btn_invert' "));
    if (buttonActiveLow) pageWrite(F("checked"));
    pageWrite(F("> Use internal pull-up</label></div></div>"));
  pageWrite(F("</div>"));
  pageWrite(F("</div>"));

  pageWrite(F("<div class='card form-sec'><h3>WiFi</h3>"));
  pageWrite(F("<div class='form-grid'>"));
    pageWrite(F("<div class='field'><label>WiFi SSID (Station)</label><div class='inline'><input id='staSsid' name='wifi_ssid' type='text' style='min-width:200px' value='"));
    pageWrite(String(cfg.wifi_ssid));
    pageWrite(F("'><select id='ssidSelect' style='min-width:220px'><option value=''> Select from scan </option></select><button type='button' class='btn' onclick='doScan()'>Scan</button></div></div>"));
    pageWrite(F("<div class='field'><label>WiFi Password (Station)</label><input name='wifi_pass' type='password' value='"));
    pageWrite(String(cfg.wifi_pass));
    pageWrite(F("'></div>"));
  pageWrite(F("</div>"));

  pageWrite(F("<div class='form-grid' style='margin-top:8px'>"));
    pageWrite(F("<div class='field'><label>AP SSID</label><input name='ap_ssid' type='text' value='"));
    pageWrite(apSsid);
    pageWrite(F("'></div>"));
    pageWrite(F("<div class='field'><label>AP Password</label><input name='ap_pass' type='password' value='"));
    pageWrite(apPass);
    pageWrite(F("'></div>"));
  pageWrite(F("</div>"));
  pageWrite(F("</div>"));

  pageWrite(F("<div class='card form-sec'><h3>MQTT</h3>"));
  pageWrite(F("<div class='form-grid'>"));
    pageWrite(F("<div class='field'><label>MQTT Server</label><input name='mqtt_server' type='text' value='"));
    pageWrite(String(cfg.mqtt_server));
    pageWrite(F("'></div>"));
    pageWrite(F("<div class='field'><label>MQTT Port</label><input name='mqtt_port' type='number' value='"));
    pageWrite(String(cfg.mqtt_port));
    pageWrite(F("'></div>"));
  pageWrite(F("</div>"));

  pageWrite(F("<div class='form-grid' style='margin-top:8px'>"));
    pageWrite(F("<div class='field'><label>MQTT User</label><input name='mqtt_user' type='text' value='"));
    pageWrite(String(cfg.mqtt_user));
    pageWrite(F("'></div>"));
    pageWrite(F("<div class='field'><label>MQTT Password</label><input name='mqtt_pass' type='password' value='"));
    pageWrite(String(cfg.mqtt_pass));
    pageWrite(F("'></div>"));
  pageWrite(F("</div>"));

  pageWrite(F("<div style='margin-top:8px'><label class='inline'><input name='ha_discovery' type='checkbox' "));
  if (cfg.ha_discovery) pageWrite(F("checked"));
  pageWrite(F("> Enable Home Assistant Discovery</label></div>"));

  pageWrite(F("<div style='margin-top:10px;display:flex;gap:8px;flex-wrap:wrap'><button class='btn' type='submit'>Save & Reboot</button><a class='btn' href='/'>Back</a></div>"));
  pageWrite(F("</div>"));
  pageWrite(F("</form>"));

  pageWrite(F("<div class='card'>"
              "<div class='rowline'>"
                "<div class='desc'>Regenerate all active Remote IDs (1.."));
  pageWrite(String(blindCount));
  pageWrite(F(") and reset rolling codes. This will require re-pairing your Somfy motors.</div>"
                "<form method='POST' action='/regen' onsubmit=\"return confirm('Regenerate all active remotes?\\nThis resets IDs and rolling codes and requires re-pairing.');\">"
                  "<input type='hidden' name='all' value='1'>"
                  "<input type='hidden' name='redirect' value='/config'>"
                  "<button class='btn btn-warn' type='submit'>Regenerate ALL</button>"
                "</form>"
              "</div>"
              "<div class='rowline' style='margin-top:10px'>"
                "<div class='desc'>Reboot device (Home Assistant discovery will be republished during boot).</div>"
                "<button class='btn' type='button' onclick=\"location.href='/reboot'\">Reboot</button>"
              "</div>"
              "<div class='rowline' style='margin-top:10px'>"
                "<div class='desc'>Manually republish Home Assistant discovery topics now.</div>"
                "<form method='POST' action='/ha/rediscover'>"
                  "<input type='hidden' name='redirect' value='/config'>"
                  "<button class='btn' type='submit'>Republish HA Discovery</button>"
                "</form>"
              "</div>"
            "</div>"));

  pageWrite(F("<div class='card'><div style='display:flex;gap:10px;flex-wrap:wrap;align-items:center'>"
                "<form method='GET' action='/backup'><button class='btn' type='submit'>Backup</button></form>"
                "<form method='POST' action='/restore_backup' enctype='multipart/form-data'>"
                  "<input type='file' name='upload' accept='.json' />"
                  "<button class='btn' type='submit'>Restore</button>"
                "</form>"
              "</div><div class='muted' style='margin-top:6px'>Restore expects a JSON file previously downloaded via <b>Backup</b>.</div></div>"));

  pageWrite(F(
    "<script>"
    "function doScan(){"
      "const sel=document.getElementById('ssidSelect');"
      "const ss=document.getElementById('staSsid');"
      "if(!sel||!ss) return;"
      "sel.innerHTML='<option>Scanning...</option>'; sel.disabled=true;"
      "fetch('/wifi_scan',{cache:'no-store'})"
        ".then(r=>r.json()).then(list=>{"
          "list.sort((a,b)=>(b.rssi||-999)-(a.rssi||-999));"
          "let opts=\"<option value=''> Select from scan </option>\";"
          "for(const n of list){"
            "const s=n.ssid||''; const r=n.rssi; const enc=(n.enc&&n.enc!==0)?' [LOCK]':'';"
            "const label=(s.length?s:'(hidden)')+'  ['+r+' dBm]'+enc;"
            "const val=s.replace(/\"/g,'&quot;');"
            "opts += \"<option value=\\\"\"+val+\"\\\">\"+label+\"</option>\";"
          "}"
          "sel.innerHTML=opts; sel.disabled=false;"
          "sel.onchange=()=>{ if(sel.value) ss.value=sel.value; };"
        "})"
        ".catch(()=>{ sel.innerHTML=\"<option value=''>Scan failed - try again</option>\"; sel.disabled=false; });"
    "}"
    "</script>"
  ));

  pageEnd();
}

void handleRegenPost() {
  bool did = false;

  if (server.hasArg("all") && server.arg("all") == "1") {
    regenAllScheduled = true;
    rediscoveryScheduled = true;
    rediscoveryPtr = 1;
    nextRediscoveryAt = millis() + 500;
    did = true;
  } else if (server.hasArg("b")) {
    int b = server.arg("b").toInt();
    if (isValidBlind(b)) {
      remoteId[b] = genRandom24();
      rollingCode[b] = 1;
      if (!saveRemotes()) Serial.println("[REM] save FAILED");
      rediscoveryScheduled = true;
      rediscoveryPtr = b;
      nextRediscoveryAt = millis() + 200;
      did = true;
    }
  }

  String redir = server.hasArg("redirect") ? server.arg("redirect") : String("/");
  server.sendHeader("Location", redir);
  server.send(302, "text/plain", did ? "ok" : "noop");
}

static const char* RESTORE_TMP = "/.restore.tmp";

void handleRestoreBackupUpload() {
  HTTPUpload& up = server.upload();
  static File uf;

  if (up.status == UPLOAD_FILE_START) {
    if (LittleFS.exists(RESTORE_TMP)) LittleFS.remove(RESTORE_TMP);
    uf = LittleFS.open(RESTORE_TMP, "w");
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (uf) uf.write(up.buf, up.currentSize);
  } else if (up.status == UPLOAD_FILE_END) {
    if (uf) uf.close();
  }
}

void handleRestoreBackupPost() {
  if (!LittleFS.exists(RESTORE_TMP)) { server.send(400, "text/plain", "no file uploaded"); return; }

  String body;
  {
    File tf = LittleFS.open(RESTORE_TMP, "r");
    while (tf && tf.available()) body += char(tf.read());
    tf.close();
    LittleFS.remove(RESTORE_TMP);
  }
  body.trim();
  if (!body.length()) { server.send(400, "text/plain", "empty file"); return; }

  auto writeText = [&](const char* path, const String& txt)->bool {
    File f = LittleFS.open(path, "w");
    if (!f) return false;
    f.print(txt);
    f.flush();
    f.close();
    return true;
  };

  bool wroteCfg = false, wroteRem = false;
  String cfgObj, remObj;
  bool hasCfg = extractTopObject(body, "config", cfgObj);
  bool hasRem = extractTopObject(body, "remotes", remObj);

  if (hasCfg || hasRem) {
    if (hasCfg) wroteCfg = writeText(CONFIG_FILE, cfgObj);
    if (hasRem) wroteRem = writeText(REMOTES_FILE, remObj);
  } else {
    bool maybeCfg = (body.indexOf("\"device_id\"") >= 0) || (body.indexOf("\"wifi_ssid\"") >= 0) || (body.indexOf("\"mqtt_server\"") >= 0);
    bool maybeRem = (body.indexOf("\"r1\"") >= 0) || (body.indexOf("\"rc1\"") >= 0) || (body.indexOf("\"n1\"") >= 0);
    if (maybeCfg) wroteCfg = writeText(CONFIG_FILE, body);
    if (maybeRem) wroteRem = writeText(REMOTES_FILE, body);
    if (!wroteCfg && !wroteRem) wroteCfg = writeText(CONFIG_FILE, body);
  }

  {
    DynamicJsonDocument jd(4096);
    DeserializationError err = deserializeJson(jd, hasCfg ? cfgObj : body);
    if (!err) {
      const char* di = nullptr;
      if (jd.containsKey("device_id")) di = jd["device_id"];
      else if (jd.containsKey("config") && jd["config"].containsKey("device_id")) di = jd["config"]["device_id"];
      if (di && di[0]) {
        deviceId = normalizeDeviceId(String(di));
        eepromSaveDeviceId(deviceId);
        WiFi.hostname(deviceId.c_str());
      }
    }
  }

  loadConfig();
  if (LittleFS.exists(REMOTES_FILE)) loadRemotes();

  somfy.setPin(txPin);
  applyLedPin();
  applyButtonPin();

  if (mqtt.connected()) mqtt.disconnect();
  delay(100);
  mqttEnsureConnected();
  rediscoveryScheduled = true;
  rediscoveryPtr = 1;
  nextRediscoveryAt = millis() + 300;

  sendRebootingPage("Restore complete","Settings applied. The device will come back online shortly.",10,5000);
  if (server.client()) server.client().flush();
  delay(150);
  scheduleReboot(2200);
}
