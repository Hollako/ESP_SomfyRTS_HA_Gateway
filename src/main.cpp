#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <EEPROM.h>

#include "app_state.h"
#include "storage.h"
#include "wifi_manager.h"
#include "mqtt_ha.h"
#include "web_handlers.h"

const uint32_t DEV_ID_MAGIC = 0x534F4D46;
const char* HA_MANUFACTURER = "Hollako";
const char* HA_MODEL = "ESPSomfyRTS";
const char* HA_SW_VERSION = "1.0.0";

ESP8266WebServer server(80);
WiFiClient espClient;
PubSubClient mqtt(espClient);
SomfyRemote somfy;

int txPin = 4;
int ledPin = -1;
bool ledActiveLow = false;
int buttonPin = -1;
bool buttonActiveLow = true;

volatile bool rebootScheduled = false;
unsigned long rebootAtMs = 0;

volatile bool regenAllScheduled = false;
volatile bool rediscoveryScheduled = false;
uint8_t rediscoveryPtr = 0;
unsigned long nextRediscoveryAt = 0;

bool staBusy = false;
unsigned long nextStaAttempt = 0;
const unsigned long STA_CONNECT_GRACE = 4000;
const unsigned long STA_RETRY_INTERVAL = 6000;
const uint8_t MAX_STA_RETRY = 5;

String deviceId;
String apSsid;
String apPass;
AppConfig cfg;

const IPAddress AP_IP(192,168,4,1);
const IPAddress AP_GW(192,168,4,1);
const IPAddress AP_SN(255,255,255,0);

volatile wl_status_t staStatus = WL_IDLE_STATUS;
volatile uint8_t staDiscReason = 0;
String staLastEvent = "";
String staTriedSsid = "";
uint8_t staRetries = 0;
unsigned long lastStaChangeMs = 0;

bool apActive = false;
unsigned long staConnectedAt = 0;

uint32_t remoteId[33];
uint16_t rollingCode[33];
String blindNames[33];

void setup() {
  Serial.begin(115200);
  delay(100);

  EEPROM.begin(EEPROM_SIZE);
  String eid;
  if (eepromLoadDeviceId(eid)) deviceId = eid;
  else { deviceId = String(ESP.getChipId(), HEX); eepromSaveDeviceId(deviceId); }

  if (!LittleFS.begin()) { LittleFS.format(); LittleFS.begin(); }

  apSsid = String(DEFAULT_AP_SSID_PREFIX) + deviceId;
  apPass = DEFAULT_AP_PASS;

  setStr(cfg.wifi_ssid,   sizeof(cfg.wifi_ssid),   "");
  setStr(cfg.wifi_pass,   sizeof(cfg.wifi_pass),   "");
  setStr(cfg.mqtt_server, sizeof(cfg.mqtt_server), "");
  cfg.mqtt_port = 1883;
  setStr(cfg.mqtt_user,   sizeof(cfg.mqtt_user),   "");
  setStr(cfg.mqtt_pass,   sizeof(cfg.mqtt_pass),   "");
  cfg.ha_discovery = true;

  bool loaded = loadConfig();
  if (!loaded && !LittleFS.exists(CONFIG_FILE)) {
    saveConfig();
  }

  ledPin = sanitizeLedGpio(ledPin);
  applyLedPin();
  txPin = sanitizeGpio(txPin);
  somfy.setPin(txPin);
  buttonPin = sanitizeButtonGpio(buttonPin);
  applyButtonPin();

  WiFi.hostname(deviceId.c_str());

  if (!loadRemotes()) {
    if (!LittleFS.exists(REMOTES_FILE)) {
      genAllRemoteIdsRandom();
      for (int i = 1; i <= 32; i++) blindNames[i] = "Blind " + String(i);
      saveRemotes();
      Serial.println("[REM] created new remotes.json");
    } else {
      Serial.println("[REM] keeping existing remotes.json despite load failure");
    }
  }

  server.on("/", HTTP_GET, [](){
    if (isApPortalMode()) handleApPortalGet();
    else handleRootGet();
  });

  server.on("/ap_portal_config", HTTP_POST, handleApPortalConfigPost);
  server.on("/config", HTTP_GET, handleConfigGet);
  server.on("/config", HTTP_POST, handleConfigPost);
  server.on("/regen", HTTP_POST, handleRegenPost);

  server.on("/rename", HTTP_POST, [](){
    if (!server.hasArg("b") || !server.hasArg("name")) { server.send(400, "text/plain", "bad"); return; }
    int b = server.arg("b").toInt();
    if (b < 1 || b > 32) { server.send(400, "text/plain", "bad"); return; }
    String nm = server.arg("name");
    nm.trim();
    if (nm.length() == 0) nm = String("Blind ") + b;
    blindNames[b] = nm;
    saveRemotes();
    publishHACover(b);
    publishHAProgButton(b);
    server.sendHeader("Location", "/");
    server.send(302, "text/plain", "");
  });

  server.on("/api/cmd", HTTP_POST, []() {
    if (!server.hasArg("b") || !server.hasArg("cmd")) { server.send(400, "application/json", "{\"ok\":false}"); return; }

    int b = server.arg("b").toInt();
    String c = server.arg("cmd");
    if (b < 1 || b > 32) { server.send(400, "application/json", "{\"ok\":false}"); return; }

    if (c == "OPEN") { sendSomfy(b, REMOTE_RAISE); publishState(b, "open"); }
    else if (c == "CLOSE") { sendSomfy(b, REMOTE_LOWER); publishState(b, "closed"); }
    else if (c == "STOP") { sendSomfy(b, REMOTE_STOP); publishState(b, "stopped"); }
    else if (c == "PROG") { sendSomfy(b, REMOTE_PROG); }
    else { server.send(400, "application/json", "{\"ok\":false}"); return; }

    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/config.json", HTTP_GET, [](){
    if (!LittleFS.exists(CONFIG_FILE)) { server.send(404, "text/plain", "no config"); return; }
    File f = LittleFS.open(CONFIG_FILE, "r");
    String body;
    while (f && f.available()) body += char(f.read());
    f.close();
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", body);
  });

  server.on("/reboot", HTTP_GET, [](){
    sendRebootingPage("Reboot requested", "");
    scheduleReboot(800);
  });

  server.on("/backup", HTTP_GET, [](){
    String cfgJ = readWholeFile(CONFIG_FILE);
    String remJ = readWholeFile(REMOTES_FILE);
    String out = "{\"version\":2,\"config\":" + cfgJ + ",\"remotes\":" + remJ + "}";

    String fname = "ESPSomfyRemote_" + deviceId + ".json";
    server.sendHeader("Content-Disposition", "attachment; filename=\"" + fname + "\"");
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", out);
  });

  server.on("/restore_backup", HTTP_POST, handleRestoreBackupPost, handleRestoreBackupUpload);

  server.on("/status", HTTP_GET, [](){
    DynamicJsonDocument doc(384);

    const bool staUp = (WiFi.status() == WL_CONNECTED);
    doc["mqtt_connected"] = mqtt.connected();
    doc["mqtt_broker"] = String(cfg.mqtt_server);

    if (staUp) {
      doc["sta_ip"] = WiFi.localIP().toString();
      doc["sta_ssid"] = WiFi.SSID();
      int32_t rssi = WiFi.RSSI();
      int pct = constrain(map(rssi, -90, -30, 0, 100), 0, 100);
      doc["sta_rssi"] = pct;
    } else {
      doc["sta_ip"] = "";
      doc["sta_ssid"] = "";
      doc["sta_rssi"] = 0;
    }

    doc["ap_active"] = apActive;
    doc["ap_ssid"] = apSsid;

    server.sendHeader("Cache-Control", "no-store");
    String out;
    serializeJson(doc, out);
    server.send(200, "application/json", out);
  });

  server.on("/wifi_scan", HTTP_GET, [](){
    int n = WiFi.scanComplete();
    if (n == WIFI_SCAN_RUNNING) {
      server.sendHeader("Cache-Control","no-store");
      server.send(200, "application/json", "[]");
      return;
    }
    if (n == -2) {
      WiFi.scanNetworks(true, true);
      server.sendHeader("Cache-Control","no-store");
      server.send(200, "application/json", "[]");
      return;
    }

    String out = "[";
    for (int i = 0; i < n; i++) {
      if (i) out += ",";
      out += "{\"ssid\":\"" + jsonEscape(WiFi.SSID(i)) + "\""
          +  ",\"rssi\":" + String(WiFi.RSSI(i))
          +  ",\"enc\":"  + String(WiFi.encryptionType(i))
          +  ",\"ch\":"   + String(WiFi.channel(i))
          +  "}";
    }
    out += "]";
    WiFi.scanDelete();
    server.sendHeader("Cache-Control","no-store");
    server.send(200, "application/json", out);
  });

  server.on("/eeid", HTTP_GET, [](){
    String e;
    bool ok = eepromLoadDeviceId(e);
    server.send(200, "text/plain", ok ? ("EEPROM device_id=" + e) : "EEPROM empty/invalid");
  });

  server.begin();

  startAP();
  installWiFiDebugHandlers();
  beginSTAIfCreds();

  if (MDNS.begin(deviceId.c_str())) MDNS.addService("http", "tcp", 80);
  ArduinoOTA.setHostname(deviceId.c_str());
  ArduinoOTA.begin();

  mqttEnsureConnected();
}

void loop() {
  server.handleClient();
  ArduinoOTA.handle();

  updateWiFiSM();
  pollButtonLongPress();
  serviceScheduledReboot();

  if (regenAllScheduled) {
    genAllRemoteIdsRandom();
    saveRemotes();
    regenAllScheduled = false;

    rediscoveryScheduled = true;
    rediscoveryPtr = 1;
    nextRediscoveryAt = millis() + 300;
  }

  if (rediscoveryScheduled && mqtt.connected()) {
    if (millis() >= nextRediscoveryAt) {
      if (rediscoveryPtr >= 1 && rediscoveryPtr <= 32) {
        publishHACover(rediscoveryPtr);
        publishHAProgButton(rediscoveryPtr);
        publishState(rediscoveryPtr, "unknown");
        rediscoveryPtr++;
        nextRediscoveryAt = millis() + 120;
        delay(0);
      } else {
        rediscoveryScheduled = false;
      }
    }
  }

  if (String(cfg.mqtt_server).length() > 0) {
    if (!mqtt.connected()) {
      if (WiFi.status() == WL_CONNECTED) mqttEnsureConnected();
    } else {
      mqtt.loop();
    }
  }

  static bool lastMQTT = false;
  bool nowMQTT = mqtt.connected();
  if (nowMQTT != lastMQTT) {
    setLed(nowMQTT);
    lastMQTT = nowMQTT;
  }
}
