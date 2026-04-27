#include "web_handlers.h"

#include <LittleFS.h>
#include <ArduinoJson.h>
#include <Updater.h>

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
      ".nav{position:sticky;top:0;z-index:10;display:flex;gap:8px;align-items:center;padding:8px 18px;border-bottom:1px solid var(--border);background:var(--bg)}"
      ".nav .sp{flex:1}"
      ".nav-logo{display:flex;align-items:center;gap:10px;text-decoration:none;color:inherit}"
      ".nav-logo img{height:52px;width:auto;display:block}"
      ".nav-title{font-size:18px;font-weight:700;white-space:nowrap}"
      ".btn.nicn{display:inline-flex;align-items:center;justify-content:center;width:42px;height:42px;font-size:22px;padding:0;min-width:unset;min-height:unset;border-radius:10px;color:var(--fg);text-decoration:none;cursor:pointer}"
      ".btn.nicn:hover{filter:brightness(0.92)}"
      ".btn.nicn svg{stroke:currentColor;fill:none;stroke-width:1.8;stroke-linecap:round;stroke-linejoin:round}"
      ".icon-sun{display:none}.icon-moon{display:block}"
      "body.dark .icon-sun{display:block}body.dark .icon-moon{display:none}"
      ".btn.nicn.nicn-red{border-color:#e03131;color:#e03131;background:#ffe3e3}"
      "body.dark .btn.nicn.nicn-red{border-color:#e03131;color:#e03131;background:#ffe3e3}"
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
      ".ok .dot{background:#19c37d}.warn .dot{background:#f59e0b}.err .dot{background:#ef4444}"
      ".bar{display:flex;justify-content:space-between;align-items:center;gap:10px;flex-wrap:wrap}"
      ".btn-sm{padding:6px 10px;font-size:13px;border-radius:8px}"
      "input,select,button,textarea{background:var(--card);color:var(--fg);border:1px solid var(--border);border-radius:8px;padding:6px 8px}"
      "button.btn{cursor:pointer;padding:8px 12px;border-radius:10px}"
      "button.btn:hover{filter:brightness(0.95)}"
      ".btn-warn{border-color:#e03131;background:#ffe3e3;color:#e03131}"
      "body.dark .btn-warn{background:rgba(224,49,49,.1);color:#ff6b6b}"
      ".btn-x{width:30px;height:30px;padding:0;display:inline-flex;align-items:center;justify-content:center;font-weight:700;border-color:#e03131;background:#ffe3e3;color:#e03131}"
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
        "if(wb){"
          "const rssi=b.sta_rssi||0;"
          "wb.classList.toggle('ok',   staUp && rssi>=50);"
          "wb.classList.toggle('warn', staUp && rssi>=30 && rssi<50);"
          "wb.classList.toggle('err',  !staUp || rssi<30);"
        "}"
        "if(wt) wt.textContent = staUp ? ('Wi-Fi: ' + (b.sta_ssid||'') + ' (' + (b.sta_rssi||0) + '%)') : 'Wi-Fi: Disconnected';"
        "const ab=document.getElementById('apBadge');"
        "const a=document.getElementById('apText');"
        "if(ab){ab.classList.toggle('ok', !!b.ap_active); ab.classList.toggle('err', !b.ap_active);}"
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

  server.sendContent(F("<div class='nav'>"
    "<a class='nav-logo' href='/'>"
      "<img src='/logo.png' alt='SmartWay Systems'>"
      "<div class='nav-title'>ESP Somfy RTS - "));
  server.sendContent(prettyDeviceNameForUi());
  server.sendContent(
    F("</div>"
    "</a>"
    "<div class='sp'></div>"
    "<a class='btn nicn' href='/' title='Home'>"
      "<svg width='22' height='22' viewBox='0 0 24 24'><path d='M3 9.5L12 3l9 6.5V20a1 1 0 0 1-1 1H5a1 1 0 0 1-1-1V9.5z'/><path d='M9 21V12h6v9'/></svg>"
    "</a>"
    "<a class='btn nicn' href='/config' title='Config'>"
      "<svg width='20' height='20' viewBox='0 0 24 24'><circle cx='12' cy='12' r='3'/><path d='M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 0 1-2.83 2.83l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 0 1-4 0v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 0 1-2.83-2.83l.06-.06A1.65 1.65 0 0 0 4.68 15a1.65 1.65 0 0 0-1.51-1H3a2 2 0 0 1 0-4h.09A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 0 1 2.83-2.83l.06.06A1.65 1.65 0 0 0 9 4.68a1.65 1.65 0 0 0 1-1.51V3a2 2 0 0 1 4 0v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 0 1 2.83 2.83l-.06.06A1.65 1.65 0 0 0 19.4 9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 0 1 0 4h-.09a1.65 1.65 0 0 0-1.51 1z'/></svg>"
    "</a>"
    "<a class='btn nicn' href='/update' title='Firmware Update'>"
      "<svg width='20' height='20' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2' stroke-linecap='round' stroke-linejoin='round'><polyline points='16 16 12 12 8 16'/><line x1='12' y1='12' x2='12' y2='21'/><path d='M20.39 18.39A5 5 0 0 0 18 9h-1.26A8 8 0 1 0 3 16.3'/><polyline points='16 16 12 12 8 16'/></svg>"
    "</a>"
    "<button class='btn nicn' onclick='toggleDark()' title='Toggle dark mode'>"
      "<svg class='icon-moon' width='20' height='20' viewBox='0 0 24 24'><path d='M21 12.79A9 9 0 1 1 11.21 3 7 7 0 0 0 21 12.79z'/></svg>"
      "<svg class='icon-sun' width='20' height='20' viewBox='0 0 24 24'><circle cx='12' cy='12' r='5'/><line x1='12' y1='1' x2='12' y2='3'/><line x1='12' y1='21' x2='12' y2='23'/><line x1='4.22' y1='4.22' x2='5.64' y2='5.64'/><line x1='18.36' y1='18.36' x2='19.78' y2='19.78'/><line x1='1' y1='12' x2='3' y2='12'/><line x1='21' y1='12' x2='23' y2='12'/><line x1='4.22' y1='19.78' x2='5.64' y2='18.36'/><line x1='18.36' y1='5.64' x2='19.78' y2='4.22'/></svg>"
    "</button>"
    "<button class='btn nicn nicn-red' onclick=\"if(confirm('Reboot the device?'))location.href='/reboot'\" title='Reboot'>"
      "<svg width='20' height='20' viewBox='0 0 24 24'><polyline points='23 4 23 10 17 10'/><path d='M20.49 15a9 9 0 1 1-2.12-9.36L23 10'/></svg>"
    "</button>"
    "</div><div class='container'>")
  );

  server.sendContent(F("<div class='status'>"
  "<div id='wifiBadge' class='badge'><span class='dot'></span><span id='wifiText'>Wi-Fi...</span></div>"
  "<div id='staBadge'  class='badge'><span class='dot'></span><span id='staText'>IP...</span></div>"
  "<div id='mqttBadge' class='badge'><span class='dot'></span><span id='mqttText'>MQTT...</span></div>"
  "<div id='apBadge' class='badge'><span class='dot'></span><span id='apText'>AP...</span></div>"
  "</div>"));
}

static inline void pageWrite(const __FlashStringHelper* s) { server.sendContent(s); }
static inline void pageWrite(const String& s) { if (s.length()) server.sendContent(s); }
static inline void pageWrite(const char* s) { if (s && s[0]) server.sendContent(s); }
static inline void pageFlush() { server.client().flush(); delay(1); }
static inline void pageEnd() { server.sendContent(F("</div></body></html>")); }

void handleRootGet() {
  pageBegin("Home");

  pageWrite(F("<div class='muted' style='font-size:16px;font-weight:600;letter-spacing:0.04em;margin-bottom:6px'>Somfy RTS Motors</div>"));
  pageWrite(F("<div class='card'><div class='bar'>"
              "<div class='muted'>Active blinds: "));
  pageWrite(String(activeBlindCount()));
  pageWrite(F(" / "));
  pageWrite(String(MAX_BLINDS));
  pageWrite(F("</div>"
              "<form method='POST' action='/blind/add'>"));
  if (activeBlindCount() >= MAX_BLINDS) {
    pageWrite(F("<button class='btn btn-sm' type='submit' disabled>Max blinds reached</button>"));
  } else {
    pageWrite(F("<button class='btn btn-sm' type='submit'>Add blind</button>"));
  }
  pageWrite(F("</form></div></div>"));

  pageWrite(F("<div class='card'><div class='grid2'>"));

  for (int i = 1; i <= blindCount; i++) {
    if (!isValidBlind(i)) continue;
    pageWrite(F("<div class='cardb'>"));
    pageWrite(F("<div class='rowline' style='margin:0 0 6px 0'>"));
    pageWrite(F("<div class='title' style='margin:0'>"));
    pageWrite(blindName(i));
    pageWrite(F("</div><form method='POST' action='/blind/remove' style='margin:0' onsubmit=\"return confirm('Remove this blind?');\">"
                "<input type='hidden' name='b' value='"));
    pageWrite(String(i));
    pageWrite(F("'>"));
    if (activeBlindCount() <= MIN_BLINDS) {
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
       "<label>Password</label>"
       "<div style='display:flex;gap:6px;align-items:center'>"
         "<input id='wpass' name='wifi_pass' type='password' value=''>"
         "<label style='display:flex;align-items:center;gap:4px;font-size:13px;color:#555;margin:0;cursor:pointer'>"
           "<input type='checkbox' onclick=\"document.getElementById('wpass').type=this.checked?'text':'password'\">Show"
         "</label>"
       "</div>"
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
       "function scanW(){"
         "const sel=document.getElementById('nets');"
         "sel.innerHTML='<option>Scanning…</option>'; sel.disabled=true;"
         "let t=0;"
         "function poll(){"
           "fetch('/wifi_scan',{cache:'no-store'}).then(r=>r.json()).then(arr=>{"
             "if(arr.length===0&&t++<18){setTimeout(poll,800);return;}"
             "sel.innerHTML='';"
             "const ph=document.createElement('option');"
             "ph.value=''; ph.text='Select from list'; ph.disabled=true; ph.selected=true;"
             "sel.add(ph);"
             "if(!arr.length){sel.disabled=false;return;}"
             "arr.sort((a,b)=>b.rssi-a.rssi).forEach(n=>{"
               "const o=document.createElement('option');"
               "o.text=(n.ssid||'(hidden)')+'  ['+n.rssi+' dBm]'; o.value=n.ssid||''; sel.add(o);"
             "});"
             "sel.disabled=false;"
             "sel.onchange=()=>{const si=document.querySelector(\"input[name='wifi_ssid']\");if(si&&sel.value)si.value=sel.value;};"
           "}).catch(()=>{sel.innerHTML='<option>Scan failed</option>';sel.disabled=false;});"
         "}"
         "poll();"
       "}"
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

  String detail;
  if (ok) {
    String ip = WiFi.localIP().toString();
    detail = String("Wi-Fi connected.<br>STA IP: <b>") + ip + "</b><br>"
           + "After reboot, open: <a href='http://" + ip + "/'>http://" + ip + "/</a>";
  } else {
    detail = "Settings saved. Rebooting...<br>"
             "If connection fails, reconnect to AP at <b>http://192.168.4.1/</b>";
  }

  sendRebootingPage("Saved", detail, 15, 5000);
  if (server.client()) server.client().flush();
  delay(150);
  scheduleReboot(2200);
}

void handleConfigPost() {
  auto nonEmpty = [](const String& v) { return v.length() > 0; };

  if (server.hasArg("device_id") && nonEmpty(server.arg("device_id"))) {
    deviceId = normalizeDeviceId(server.arg("device_id"));
    eepromSaveDeviceId(deviceId);
    apSsid = String(DEFAULT_AP_SSID_PREFIX) + deviceId;
  }
  if (server.hasArg("wifi_ssid"))   setStr(cfg.wifi_ssid,   sizeof(cfg.wifi_ssid),   server.arg("wifi_ssid"));
  if (server.hasArg("wifi_pass"))   setStr(cfg.wifi_pass,   sizeof(cfg.wifi_pass),   server.arg("wifi_pass"));
  if (server.hasArg("wifi_ssid2"))  setStr(cfg.wifi_ssid2,  sizeof(cfg.wifi_ssid2),  server.arg("wifi_ssid2"));
  if (server.hasArg("wifi_pass2"))  setStr(cfg.wifi_pass2,  sizeof(cfg.wifi_pass2),  server.arg("wifi_pass2"));
  if (server.hasArg("mqtt_server")) setStr(cfg.mqtt_server, sizeof(cfg.mqtt_server), server.arg("mqtt_server"));
  if (server.hasArg("mqtt_port") && nonEmpty(server.arg("mqtt_port")))
    cfg.mqtt_port = server.arg("mqtt_port").toInt() > 0 ? server.arg("mqtt_port").toInt() : 1883;
  else if (!cfg.mqtt_port) cfg.mqtt_port = 1883;
  if (server.hasArg("mqtt_user"))   setStr(cfg.mqtt_user,   sizeof(cfg.mqtt_user),   server.arg("mqtt_user"));
  if (server.hasArg("mqtt_pass"))   setStr(cfg.mqtt_pass,   sizeof(cfg.mqtt_pass),   server.arg("mqtt_pass"));
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

  pageWrite(F("<div class='card form-sec'><h3 style='margin-top:0'>Device</h3>"));
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
  pageFlush();

  pageWrite(F("<div class='card form-sec'><h3 style='margin-top:0'>WiFi</h3>"));
  pageWrite(F("<div class='muted' style='margin-bottom:8px'>SSID 1</div>"));
  pageWrite(F("<div class='form-grid'>"));
    pageWrite(F("<div class='field'><label>SSID 1 Network</label><div class='inline'><input id='staSsid' name='wifi_ssid' type='text' style='min-width:200px' placeholder='Network name' value='"));
    pageWrite(String(cfg.wifi_ssid));
    pageWrite(F("'><select id='ssidSelect' style='min-width:220px'><option value=''> Select from scan </option></select><button type='button' class='btn' onclick='doScan()'>Scan</button></div></div>"));
    pageWrite(F("<div class='field'><label>SSID 1 Password</label><input name='wifi_pass' type='password' placeholder='Leave blank for open network' value='"));
    pageWrite(String(cfg.wifi_pass));
    pageWrite(F("'></div>"));
  pageWrite(F("</div>"));
  pageWrite(F("<div class='muted' style='margin:10px 0 6px'>SSID 2</div>"));
  pageWrite(F("<div class='form-grid'>"));
    pageWrite(F("<div class='field'><label>SSID 2 Network</label><input name='wifi_ssid2' type='text' placeholder='Leave blank to disable' value='"));
    pageWrite(String(cfg.wifi_ssid2));
    pageWrite(F("'></div>"));
    pageWrite(F("<div class='field'><label>SSID 2 Password</label><input name='wifi_pass2' type='password' placeholder='Leave blank for open network' value='"));
    pageWrite(String(cfg.wifi_pass2));
    pageWrite(F("'></div>"));
  pageWrite(F("</div>"));

  pageWrite(F("<div class='form-grid' style='margin-top:8px'>"));
    pageWrite(F("<div class='field'><label>AP SSID</label><input type='text' disabled value='"));
    pageWrite(apSsid);
    pageWrite(F("'><div class='muted' style='margin-top:4px'>Auto-set from Device ID</div></div>"));
    pageWrite(F("<div class='field'><label>AP Password</label><input name='ap_pass' type='password' placeholder='Leave blank for open AP' value='"));
    pageWrite(apPass);
    pageWrite(F("'></div>"));
  pageWrite(F("</div>"));
  pageWrite(F("</div>"));
  pageFlush();

  pageWrite(F("<div class='card form-sec'><h3 style='margin-top:0'>MQTT</h3>"));
  pageWrite(F("<div class='form-grid'>"));
    pageWrite(F("<div class='field'><label>MQTT Server</label><input name='mqtt_server' type='text' placeholder='e.g. 192.168.1.100' value='"));
    pageWrite(String(cfg.mqtt_server));
    pageWrite(F("'></div>"));
    pageWrite(F("<div class='field'><label>MQTT Port</label><input name='mqtt_port' type='number' min='1' max='65535' required placeholder='1883' value='"));
    pageWrite(String(cfg.mqtt_port ? cfg.mqtt_port : 1883));
    pageWrite(F("'></div>"));
  pageWrite(F("</div>"));

  pageWrite(F("<div class='form-grid' style='margin-top:8px'>"));
    pageWrite(F("<div class='field'><label>MQTT User</label><input name='mqtt_user' type='text' placeholder='Leave blank if not required' value='"));
    pageWrite(String(cfg.mqtt_user));
    pageWrite(F("'></div>"));
    pageWrite(F("<div class='field'><label>MQTT Password</label><input name='mqtt_pass' type='password' placeholder='Leave blank if not required' value='"));
    pageWrite(String(cfg.mqtt_pass));
    pageWrite(F("'></div>"));
  pageWrite(F("</div>"));

  pageWrite(F("<div style='margin-top:8px'><label class='inline'><input name='ha_discovery' type='checkbox' "));
  if (cfg.ha_discovery) pageWrite(F("checked"));
  pageWrite(F("> Enable Home Assistant Discovery</label></div>"));

  pageWrite(F("</div>"));
  pageWrite(F("<div style='margin-top:14px'><button class='btn' type='submit' style='padding:8px 12px;border-radius:10px'>Save &amp; Reboot</button></div>"));
  pageWrite(F("</form>"));
  pageFlush();

  pageWrite(F("<div class='card'>"
              "<h3 style='margin-top:0'>Somfy RTS</h3>"
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

  pageWrite(F("<div class='card'>"
                "<div class='rowline'>"
                  "<div>"
                    "<div style='font-weight:600;margin-bottom:4px'>Factory Reset</div>"
                    "<div class='muted'>Erases all settings including Wi-Fi, MQTT, blinds and device ID. The device will reboot into AP setup mode.</div>"
                  "</div>"
                  "<form method='POST' action='/factory_reset' "
                    "onsubmit=\"return confirm('Factory reset?\\nThis will erase ALL settings including Wi-Fi, MQTT and all blinds.\\nThe device will reboot into AP mode. This cannot be undone.')\">"
                    "<button class='btn btn-warn' type='submit'>Factory Reset</button>"
                  "</form>"
                "</div>"
              "</div>"));

  pageWrite(F(
    "<script>"
    "function doScan(){"
      "const sel=document.getElementById('ssidSelect');"
      "const ss=document.getElementById('staSsid');"
      "if(!sel||!ss) return;"
      "sel.innerHTML='<option>Scanning...</option>'; sel.disabled=true;"
      "let t=0;"
      "function poll(){"
        "fetch('/wifi_scan',{cache:'no-store'})"
          ".then(r=>r.json()).then(list=>{"
            "if(list.length===0&&t++<18){setTimeout(poll,800);return;}"
            "list.sort((a,b)=>(b.rssi||-999)-(a.rssi||-999));"
            "let opts=\"<option value=''> Select from scan </option>\";"
            "if(!list.length){opts=\"<option value=''>No networks found</option>\";}"
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
      "poll();"
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

void handleUpdateGet() {
  pageBegin("Firmware Update");

  pageWrite(F("<h2 style='margin:0 0 10px;font-size:18px'>Firmware Update</h2>"));

  // Single merged card
  pageWrite(F("<div class='card'>"
    "<div style='display:flex;justify-content:space-between;align-items:flex-start'>"
      "<div>"
        "<div class='muted' style='margin-bottom:4px'>Current version</div>"
        "<div style='font-size:26px;font-weight:700;line-height:1.2'>v"));
  pageWrite(HA_SW_VERSION);
  pageWrite(F("</div>"
      "</div>"
      "<div style='text-align:right'>"
        "<div class='muted' style='margin-bottom:4px'>Latest release</div>"
        "<div id='gh-ver' style='font-size:26px;font-weight:700;line-height:1.2'></div>"
        "<div style='margin-top:8px'>"
          "<a id='gh-dl' href='#' target='_blank' style='display:none' class='btn btn-sm'>Download latest .bin</a>"
        "</div>"
      "</div>"
    "</div>"
    "<div id='gh-status' style='margin-top:12px'></div>"
    "<hr style='border:none;border-top:1px solid var(--border);margin:16px 12px'>"
    "<div style='font-weight:600;margin-bottom:10px'>Upload Firmware File</div>"
    "<form method='POST' action='/update' enctype='multipart/form-data' id='uf'>"
      "<div style='display:flex;gap:8px;flex-wrap:wrap;align-items:center'>"
        "<input type='file' name='firmware' accept='.bin' required id='binFile'>"
        "<button class='btn' type='submit' id='flashBtn'>Flash</button>"
      "</div>"
    "</form>"
    "<div id='prog' style='margin-top:10px;display:none'>"
      "<div style='background:var(--border);border-radius:999px;height:8px;overflow:hidden'>"
        "<div id='progBar' style='background:#1a6fc4;height:100%;width:0%;transition:width .3s'></div>"
      "</div>"
      "<div class='muted' style='margin-top:6px' id='progText'>Uploading...</div>"
    "</div>"
    "<div class='muted' style='margin-top:10px'>Do not power off during update. Device will reboot automatically.</div>"
  "</div>"));

  pageWrite(F(
    "<script>"
    "fetch('https://api.github.com/repos/Hollako/ESP_SomfyRTS_HA_Gateway/releases/latest',{cache:'no-store'})"
      ".then(r=>r.json()).then(d=>{"
        "const ver=d.tag_name||'unknown';"
        "document.getElementById('gh-ver').textContent=ver;"
        "const bin=(d.assets||[]).find(a=>a.name&&a.name.endsWith('.bin'));"
        "const dlBtn=document.getElementById('gh-dl');"
        "if(bin){dlBtn.href=bin.browser_download_url;}"
        "else{dlBtn.href='https://github.com/Hollako/ESP_SomfyRTS_HA_Gateway/releases/latest';}"
        "dlBtn.style.display='inline-block';"
        "const cur='"));
  pageWrite(HA_SW_VERSION);
  pageWrite(F("';"
        "const same=(ver===cur||ver==='v'+cur||'v'+ver===cur||ver.replace(/^v/,'')==cur.replace(/^v/,''));"
        "const st=document.getElementById('gh-status');"
        "if(same){"
          "st.innerHTML=\"<span style='color:#19c37d;font-weight:600'>&#10003; Firmware is up to date</span>\";"
        "}else{"
          "st.innerHTML=\"<span style='color:#f59e0b;font-weight:600'>&#9650; New version available - click Download to get the firmware, Choose the file and click Flash</span>\";"
        "}"
      "})"
      ".catch(()=>{"
        "document.getElementById('gh-ver').textContent='—';"
      "});"
    "document.getElementById('uf').addEventListener('submit',function(e){"
      "e.preventDefault();"
      "const f=document.getElementById('binFile').files[0];"
      "if(!f)return;"
      "document.getElementById('flashBtn').disabled=true;"
      "document.getElementById('prog').style.display='block';"
      "const fd=new FormData();"
      "fd.append('firmware',f);"
      "const xhr=new XMLHttpRequest();"
      "xhr.open('POST','/update');"
      "xhr.upload.onprogress=function(e){"
        "if(e.lengthComputable){"
          "const p=Math.round(e.loaded/e.total*100);"
          "document.getElementById('progBar').style.width=p+'%';"
          "document.getElementById('progText').textContent='Uploading... '+p+'%';"
        "}"
      "};"
      "xhr.onload=function(){"
        "if(xhr.status===200){"
          "document.getElementById('progBar').style.width='100%';"
          "document.getElementById('progText').textContent='Flash complete! Rebooting...';"
          "setTimeout(()=>location.replace('/'),6000);"
        "}else{"
          "document.getElementById('progText').textContent='Flash failed: '+xhr.responseText;"
          "document.getElementById('flashBtn').disabled=false;"
        "}"
      "};"
      "xhr.onerror=function(){"
        "document.getElementById('progText').textContent='Upload error. Check connection and try again.';"
        "document.getElementById('flashBtn').disabled=false;"
      "};"
      "xhr.send(fd);"
    "});"
    "</script>"
  ));

  pageEnd();
}

void handleUpdateUpload() {
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    Serial.printf("[OTA] Start: %s\n", upload.filename.c_str());
    if (!Update.begin((size_t)-1)) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) {
      Serial.printf("[OTA] Success: %u bytes\n", upload.totalSize);
    } else {
      Update.printError(Serial);
    }
  }
}

void handleUpdatePost() {
  bool ok = !Update.hasError();
  server.sendHeader("Connection", "close");
  server.send(ok ? 200 : 500, "text/plain", ok ? "OK" : Update.getErrorString());
  if (ok) {
    if (server.client()) server.client().flush();
    delay(300);
    ESP.restart();
  }
}
