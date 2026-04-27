#include "wifi_manager.h"

#include "mqtt_ha.h"
#include "storage.h"

static const unsigned long BTN_LONG_MS = 15000;
static const unsigned long BTN_SAMPLE_MS = 25;

static unsigned long apKeepUntil = 0;

void keepApAliveFor(unsigned long ms) {
  apKeepUntil = millis() + ms;
}

static unsigned long btnLastSample = 0;
static unsigned long staBusySince  = 0;
static const unsigned long STA_ATTEMPT_TIMEOUT = 12000UL; // force-reset if busy > 12 s
static unsigned long btnDownAt = 0;
static bool btnWasDown = false;

int sanitizeGpio(int pin) {
  switch (pin) {
    case 0:
    case 2:
    case 4:
    case 5:
    case 12:
    case 13:
    case 14:
    case 16:
      return pin;
    // GPIO15 excluded: must be held LOW at boot
    default:
      return 4;
  }
}

int sanitizeLedGpio(int pin) {
  if (pin == -1) return -1;
  switch (pin) {
    case 0:
    case 2:
    case 4:
    case 5:
    case 12:
    case 13:
    case 14:
    case 16:
      return pin;
    // GPIO15 excluded: must be held LOW at boot on all ESP8266 boards
    default:
      return -1;
  }
}

int sanitizeButtonGpio(int pin) {
  if (pin == -1) return -1;
  switch (pin) {
    case 0:
    case 2:
    case 4:
    case 5:
    case 12:
    case 13:
    case 14:
    case 16:
      return pin;
    // GPIO15 excluded: always LOW at boot — would permanently read as "pressed"
    default:
      return -1;
  }
}

void scheduleReboot(uint32_t delayMs) {
  rebootScheduled = true;
  rebootAtMs = millis() + delayMs;
}

void serviceScheduledReboot() {
  if (rebootScheduled && (int32_t)(millis() - rebootAtMs) >= 0) {
    ESP.restart();
  }
}

bool isApPortalMode() {
  return WiFi.status() != WL_CONNECTED;
}

void setLed(bool on) {
  if (ledPin < 0) return;
  digitalWrite(ledPin, (ledActiveLow ? (on ? LOW : HIGH) : (on ? HIGH : LOW)));
}

// ── LED pattern engine ───────────────────────────────────────────────────────
// Patterns: alternating ON/OFF durations (ms), starting with ON at step 0.
static const uint16_t PAT_SLOW[]   = {500, 1500};          // AP mode
static const uint16_t PAT_FAST[]   = {100,  100};           // WiFi connecting
static const uint16_t PAT_DOUBLE[] = {120,  120, 120, 900}; // WiFi OK, MQTT disconnected

enum LedPatId { LP_OFF, LP_SOLID, LP_SLOW, LP_FAST, LP_DOUBLE };

static LedPatId  _ledPat     = LP_OFF;
static uint8_t   _ledStep    = 0;
static unsigned long _ledAt  = 0;

static LedPatId detectPattern() {
  const bool staUp   = (WiFi.status() == WL_CONNECTED);
  const bool mqttUp  = mqtt.connected();
  const bool hasMqtt = (cfg.mqtt_server[0] != '\0');

  if (staUp && (mqttUp || !hasMqtt)) return LP_SOLID;
  if (staUp && hasMqtt && !mqttUp)   return LP_DOUBLE;
  if (!staUp && staRetries < MAX_STA_RETRY) return LP_FAST;
  return LP_SLOW; // AP-only / retries exhausted
}

void updateLed() {
  if (ledPin < 0) return;

  LedPatId pat = detectPattern();

  if (pat == LP_SOLID) { setLed(true);  _ledPat = pat; return; }
  if (pat == LP_OFF)   { setLed(false); _ledPat = pat; return; }

  // Pattern changed → restart from step 0 (ON)
  if (pat != _ledPat) {
    _ledPat  = pat;
    _ledStep = 0;
    _ledAt   = millis();
    setLed(true);
    return;
  }

  const uint16_t* steps;
  uint8_t         count;
  switch (pat) {
    case LP_SLOW:   steps = PAT_SLOW;   count = 2; break;
    case LP_FAST:   steps = PAT_FAST;   count = 2; break;
    case LP_DOUBLE: steps = PAT_DOUBLE; count = 4; break;
    default: return;
  }

  if (millis() - _ledAt >= steps[_ledStep]) {
    _ledStep = (_ledStep + 1) % count;
    _ledAt   = millis();
    setLed((_ledStep % 2) == 0); // even step = ON, odd = OFF
  }
}

void applyLedPin() {
  static int prevPin = -1;
  if (prevPin != ledPin && prevPin >= 0) {
    pinMode(prevPin, INPUT);
  }
  if (ledPin >= 0) {
    pinMode(ledPin, OUTPUT);
    setLed(false);
  }
  prevPin = ledPin;
}

void applyButtonPin() {
  static int prevBtn = -1;
  if (prevBtn != buttonPin && prevBtn >= 0) {
    pinMode(prevBtn, INPUT);
  }
  if (buttonPin >= 0) {
    if (buttonActiveLow) pinMode(buttonPin, INPUT_PULLUP);
    else pinMode(buttonPin, INPUT);
  }
  prevBtn = buttonPin;
  btnDownAt = 0;
  btnWasDown = false;
}

void pollButtonLongPress() {
  if (buttonPin < 0) return;
  if (millis() - btnLastSample < BTN_SAMPLE_MS) return;
  btnLastSample = millis();

  int level = digitalRead(buttonPin);
  bool isDown = buttonActiveLow ? (level == LOW) : (level == HIGH);

  if (isDown) {
    if (!btnWasDown) {
      btnWasDown = true;
      btnDownAt = millis();
    } else {
      if (btnDownAt && (millis() - btnDownAt >= BTN_LONG_MS)) {
        factoryResetAndReboot();
      }
    }
  } else {
    btnWasDown = false;
    btnDownAt = 0;
  }
}

void beginSTAIfCreds() {
  const char* ssid = (staWhichSsid == 2 && cfg.wifi_ssid2[0]) ? cfg.wifi_ssid2 : cfg.wifi_ssid;
  const char* pass = (staWhichSsid == 2 && cfg.wifi_ssid2[0]) ? cfg.wifi_pass2 : cfg.wifi_pass;
  if (strlen(ssid) == 0) return;

  Serial.printf("[WIFI] begin STA ssid='%s' (slot %d)\n", ssid, staWhichSsid);
  staTriedSsid = String(ssid);

  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.hostname(deviceId.c_str());

  WiFi.disconnect(false);
  delay(100);
  WiFi.begin(ssid, pass);

  staBusy      = true;
  staBusySince = millis();
  staRetries++;
  nextStaAttempt = millis() + STA_CONNECT_GRACE;
  Serial.printf("[WIFI] WiFi.begin('%s') called, staRetries now %u\n", ssid, staRetries);
}

void requestStaConnect() {
  staWhichSsid = 1;
  staRetries = 0;
  staBusy = false;
  nextStaAttempt = 0;
}

void startAP() {
  if (apActive) return;

  if (apSsid.length() == 0) apSsid = String(DEFAULT_AP_SSID_PREFIX) + String(ESP.getChipId(), HEX);
  if (apPass.length() < 8)  apPass = "Wtouch6980";

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPdisconnect(true);
  WiFi.softAPConfig(AP_IP, AP_GW, AP_SN);

  bool ok = WiFi.softAP(apSsid.c_str(), apPass.c_str(), 6, 0, 4);
  apActive = ok;

  server.close();
  delay(50);
  server.begin();

  Serial.printf("[NET] AP %s (%s)\n", ok ? "UP" : "FAIL", WiFi.softAPIP().toString().c_str());
}

void stopAP() {
  if (!apActive) return;
  WiFi.softAPdisconnect(true);
  apActive = false;
}

void updateWiFiSM() {
  static bool lastSsidSaved = false;

  if (WiFi.status() == WL_CONNECTED) {
    if (apActive) {
      if (millis() >= apKeepUntil) {
        stopAP();
        Serial.println("[NET] STA connected -> AP OFF");
        Serial.printf("[IP] Connected, IP address: %s\n", WiFi.localIP().toString().c_str());
      }
    }
    // Save the connected SSID name once per connection (safe: runs in main loop)
    if (!lastSsidSaved) {
      String ssid = WiFi.SSID();
      if (ssid.length() > 0) {
        lastSsidSaved = true;
        if (strcmp(cfg.last_ssid_name, ssid.c_str()) != 0) {
          strncpy(cfg.last_ssid_name, ssid.c_str(), sizeof(cfg.last_ssid_name) - 1);
          cfg.last_ssid_name[sizeof(cfg.last_ssid_name) - 1] = '\0';
          saveConfig();
          Serial.printf("[WIFI] Last SSID = '%s'\n", cfg.last_ssid_name);
        }
      }
    }
    staBusy = false;
    return;
  }

  lastSsidSaved = false;  // reset when disconnected so next connection saves again

  // Unstick staBusy if SDK never fired a disconnect event (e.g. WL_NO_SSID_AVAIL)
  if (staBusy && millis() - staBusySince > STA_ATTEMPT_TIMEOUT) {
    Serial.printf("[WIFI] attempt timed out after %lu ms (retries=%u slot=%u) — forcing next\n",
      millis() - staBusySince, staRetries, staWhichSsid);
    staBusy = false;
    nextStaAttempt = millis() + STA_RETRY_INTERVAL;
  }

  bool hasPrimary   = cfg.wifi_ssid[0]  != '\0';
  bool hasSecondary = cfg.wifi_ssid2[0] != '\0';

  // No credentials at all → go straight to AP
  if (!hasPrimary && !hasSecondary) {
    if (!apActive) startAP();
    return;
  }

  // Periodic SM state dump (every 5 s, only when not mid-attempt)
  static unsigned long lastDump = 0;
  if (!staBusy && millis() - lastDump >= 5000UL) {
    lastDump = millis();
    Serial.printf("[SM] slot=%u retry=%u/%u ssid1='%s' ssid2='%s' last='%s'\n",
      staWhichSsid, staRetries, MAX_STA_RETRY,
      cfg.wifi_ssid, cfg.wifi_ssid2, cfg.last_ssid_name);
  }

  // Primary exhausted → try secondary if available
  if (staRetries >= MAX_STA_RETRY && staWhichSsid == 1 && hasSecondary) {
    Serial.printf("[WIFI] SSID 1 exhausted (%u/%u), switching to SSID 2 '%s'\n",
      staRetries, MAX_STA_RETRY, cfg.wifi_ssid2);
    staWhichSsid = 2;
    staRetries   = 0;
    staBusy      = false;
    nextStaAttempt = millis();
  }

  // Both exhausted → AP mode, then keep retrying every 30 s
  if (staRetries >= MAX_STA_RETRY) {
    if (!apActive) startAP();
    Serial.printf("[WIFI] Both SSIDs exhausted (%u/%u), AP up, retrying in 30 s\n",
      staRetries, MAX_STA_RETRY);
    staWhichSsid   = 1;
    staRetries     = 0;
    staBusy        = false;
    nextStaAttempt = millis() + 30000UL;
    return;
  }

  if (!staBusy && millis() >= nextStaAttempt) {
    const char* active = (staWhichSsid == 2 && hasSecondary) ? cfg.wifi_ssid2 : cfg.wifi_ssid;
    Serial.printf("[WIFI] attempt %u/%u -> '%s' (slot %d)\n",
      staRetries + 1, MAX_STA_RETRY, active, staWhichSsid);
    beginSTAIfCreds();
  } else if (staBusy) {
    // already waiting for connection result - do nothing
  } else {
    // waiting for nextStaAttempt
    static unsigned long lastWaitLog = 0;
    if (millis() - lastWaitLog >= 2000UL) {
      lastWaitLog = millis();
      Serial.printf("[WIFI] waiting %lu ms before next attempt (slot=%u retry=%u)\n",
        nextStaAttempt - millis(), staWhichSsid, staRetries);
    }
  }
}

static const char* reasonToStr(uint8_t r) {
  switch (r) {
    case 1: return "UNSPECIFIED";
    case 2: return "AUTH_EXPIRE";
    case 3: return "AUTH_LEAVE";
    case 4: return "ASSOC_EXPIRE";
    case 5: return "ASSOC_TOOMANY";
    case 6: return "NOT_AUTHED";
    case 7: return "NOT_ASSOCED";
    case 8: return "ASSOC_LEAVE";
    case 9: return "ASSOC_NOT_AUTHED";
    case 14: return "4WAY_HANDSHAKE_TIMEOUT";
    case 15: return "GROUP_KEY_UPDATE_TIMEOUT";
    case 16: return "IE_IN_4WAY_DIFFERS";
    case 17: return "GROUP_CIPHER_INVALID";
    case 18: return "PAIRWISE_CIPHER_INVALID";
    case 19: return "AKMP_INVALID";
    case 20: return "UNSUPP_RSN_IE_VERSION";
    case 21: return "INVALID_RSN_IE_CAP";
    case 22: return "802_1X_AUTH_FAILED";
    case 23: return "CIPHER_SUITE_REJECTED";
    case 200: return "BEACON_TIMEOUT";
    case 201: return "NO_AP_FOUND";
    case 202: return "AUTH_FAIL";
    case 203: return "ASSOC_FAIL";
    case 204: return "HANDSHAKE_TIMEOUT";
    default: return "UNKNOWN";
  }
}

void installWiFiDebugHandlers() {
  WiFi.onStationModeConnected([](const WiFiEventStationModeConnected& ev){
    staStatus = WL_CONNECTED;
    staDiscReason = 0;
    staLastEvent = "CONNECTED to " + String(ev.ssid) + " (ch " + String(ev.channel) + ")";
    lastStaChangeMs = millis();
    staBusy = true;
    Serial.printf("[WIFI] %s\n", staLastEvent.c_str());
  });

  WiFi.onStationModeGotIP([](const WiFiEventStationModeGotIP& ev){
    staStatus = WL_CONNECTED;
    staDiscReason = 0;
    staLastEvent = "GOT_IP " + ev.ip.toString();
    lastStaChangeMs = millis();
    staBusy  = false;
    staRetries = 0;
    // Keep staWhichSsid as-is — do not reset to 1 here so SM retries same slot on drop
    Serial.printf("[WIFI] %s (slot=%u) gw=%s mask=%s rssi=%d dBm\n",
      staLastEvent.c_str(), staWhichSsid, ev.gw.toString().c_str(), ev.mask.toString().c_str(), WiFi.RSSI());
  });

  WiFi.onStationModeDisconnected([](const WiFiEventStationModeDisconnected& ev){
    staStatus = WL_DISCONNECTED;
    staDiscReason = ev.reason;
    staLastEvent = String("DISCONNECTED from ") + ev.ssid + " reason=" + reasonToStr(ev.reason)
                 + " (" + String(ev.reason) + ")";
    lastStaChangeMs = millis();
    staBusy = false;
    nextStaAttempt = millis() + STA_RETRY_INTERVAL;
    Serial.printf("[WIFI] %s | staRetries=%u slot=%u\n", staLastEvent.c_str(), staRetries, staWhichSsid);
  });

  WiFi.onStationModeDHCPTimeout([](){
    staStatus = WL_DISCONNECTED;
    staDiscReason = 0;
    staLastEvent = "DHCP_TIMEOUT";
    lastStaChangeMs = millis();
    staBusy = false;
    if (staRetries < MAX_STA_RETRY) staRetries++;
    nextStaAttempt = millis() + (STA_RETRY_INTERVAL / 2);
    Serial.println("[WIFI] DHCP_TIMEOUT");
  });
}