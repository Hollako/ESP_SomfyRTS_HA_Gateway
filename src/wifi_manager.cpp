#include "wifi_manager.h"

#include "mqtt_ha.h"
#include "storage.h"

static const unsigned long BTN_LONG_MS = 15000;
static const unsigned long BTN_SAMPLE_MS = 25;

static unsigned long btnLastSample = 0;
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
    case 15:
    case 16:
      return pin;
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
    case 15:
    case 16:
      return pin;
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
    case 15:
    case 16:
      return pin;
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
  if (strlen(cfg.wifi_ssid) == 0) return;

  Serial.printf("[WIFI] begin STA ssid='%s'\n", cfg.wifi_ssid);
  staTriedSsid = String(cfg.wifi_ssid);

  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.hostname(deviceId.c_str());

  WiFi.disconnect(true);
  delay(50);
  WiFi.begin(cfg.wifi_ssid, cfg.wifi_pass);

  staBusy = true;
  staRetries++;
  nextStaAttempt = millis() + STA_CONNECT_GRACE;
}

void startAP() {
  if (apActive) return;

  if (apSsid.length() == 0) apSsid = String(DEFAULT_AP_SSID_PREFIX) + deviceId;
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
  WiFi.mode(WIFI_STA);
  apActive = false;
}

void updateWiFiSM() {
  if (WiFi.status() == WL_CONNECTED) {
    if (apActive) {
      stopAP();
      Serial.println("[NET] STA connected -> AP OFF");
      Serial.printf("[IP] Connected, IP address: %s\n", WiFi.localIP().toString().c_str());
    }
    staBusy = false;
    return;
  }

  if (!apActive && (strlen(cfg.wifi_ssid) == 0 || staRetries >= MAX_STA_RETRY)) {
    startAP();
  }

  if (strlen(cfg.wifi_ssid) > 0) {
    if (!staBusy && millis() >= nextStaAttempt && staRetries < MAX_STA_RETRY) {
      Serial.printf("[WIFI] retry %u to '%s' status=%d\n", staRetries, cfg.wifi_ssid, WiFi.status());
      beginSTAIfCreds();
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
    staBusy = false;
    staRetries = 0;
    Serial.printf("[WIFI] %s gw=%s mask=%s rssi=%d dBm\n",
      staLastEvent.c_str(), ev.gw.toString().c_str(), ev.mask.toString().c_str(), WiFi.RSSI());
  });

  WiFi.onStationModeDisconnected([](const WiFiEventStationModeDisconnected& ev){
    staStatus = WL_DISCONNECTED;
    staDiscReason = ev.reason;
    staLastEvent = String("DISCONNECTED from ") + ev.ssid + " reason=" + reasonToStr(ev.reason)
                 + " (" + String(ev.reason) + ")";
    lastStaChangeMs = millis();
    staBusy = false;

    const bool manualLeave = (ev.reason == 8);
    if (!manualLeave && staRetries < MAX_STA_RETRY) staRetries++;

    nextStaAttempt = millis() + STA_RETRY_INTERVAL;
    Serial.printf("[WIFI] %s (retry=%u)\n", staLastEvent.c_str(), staRetries);
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