#include "storage.h"
#include "wifi_manager.h"

#include <LittleFS.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <string.h>
#include <ctype.h>

extern "C" {
  #include <user_interface.h>
}

uint8_t crc8(const uint8_t* data, size_t len) {
  uint8_t c = 0;
  for (size_t i = 0; i < len; i++) {
    c ^= data[i];
    for (uint8_t b = 0; b < 8; b++) c = (c & 0x80) ? (c << 1) ^ 0x07 : (c << 1);
  }
  return c;
}

bool eepromLoadDeviceId(String &out) {
  DevIdBlob blob;
  EEPROM.get(0, blob);
  if (blob.magic != DEV_ID_MAGIC) return false;
  blob.id[sizeof(blob.id)-1] = '\0';
  uint8_t want = crc8((uint8_t*)blob.id, sizeof(blob.id));
  if (want != blob.crc) return false;
  if (blob.id[0] == '\0') return false;
  out = String(blob.id);
  return true;
}

bool eepromSaveDeviceId(const String &id) {
  DevIdBlob blob;
  blob.magic = DEV_ID_MAGIC;
  memset(blob.id, 0, sizeof(blob.id));
  strncpy(blob.id, id.c_str(), sizeof(blob.id)-1);
  blob.crc = crc8((uint8_t*)blob.id, sizeof(blob.id));
  EEPROM.put(0, blob);
  return EEPROM.commit();
}

String normalizeDeviceId(const String& raw) {
  String out;
  out.reserve(32);

  for (size_t i = 0; i < raw.length(); i++) {
    char c = raw[i];
    if (isalnum((unsigned char)c) || c == '_' || c == '-') out += c;
    else if (c == ' ' || c == '.') out += '_';
    else out += '_';
    if (out.length() >= 31) break;
  }

  while (out.startsWith("_")) out.remove(0, 1);
  while (out.endsWith("_")) out.remove(out.length() - 1, 1);
  while (out.indexOf("__") >= 0) out.replace("__", "_");

  if (out.length() == 0) out = String(ESP.getChipId(), HEX);
  return out;
}

uint8_t sanitizeBlindType(int rawType) {
  if (rawType < BLIND_TYPE_BLIND || rawType > BLIND_TYPE_WINDOW) return BLIND_TYPE_BLIND;
  return (uint8_t)rawType;
}

const char* blindHaDeviceClass(int n) {
  if (!isValidBlind(n)) return "blind";
  switch (blindTypes[n]) {
    case BLIND_TYPE_SHADE: return "shade";
    case BLIND_TYPE_SHUTTER: return "shutter";
    case BLIND_TYPE_CURTAIN: return "curtain";
    case BLIND_TYPE_AWNING: return "awning";
    case BLIND_TYPE_WINDOW: return "window";
    case BLIND_TYPE_BLIND:
    default: return "blind";
  }
}

int clampBlindCount(int n) {
  if (n < MIN_BLINDS) return MIN_BLINDS;
  if (n > MAX_BLINDS) return MAX_BLINDS;
  return n;
}

bool isValidBlind(int n) {
  return n >= 1 && n <= MAX_BLINDS && remoteId[n] != 0;
}

int activeBlindCount() {
  int c = 0;
  for (int i = 1; i <= MAX_BLINDS; i++) if (remoteId[i]) c++;
  return c;
}

static void ensureBlindInitialized(int n) {
  if (n < 1 || n > MAX_BLINDS) return;
  if (remoteId[n] == 0) {
    uint32_t v; int guard = 0;
    do {
      v = genRandom24();
      guard++;
    } while (!isUniqueId(v) && guard < 200);
    remoteId[n] = v;
  }
  if (rollingCode[n] == 0) rollingCode[n] = 1;
  if (blindNames[n].length() == 0) blindNames[n] = String("Blind ") + n;
  blindTypes[n] = sanitizeBlindType(blindTypes[n]);
}


int addBlind() {
  // Fill the first empty slot, expanding blindCount only if no gap exists
  for (int i = 1; i <= MAX_BLINDS; i++) {
    if (remoteId[i] == 0) {
      ensureBlindInitialized(i);
      if (i > blindCount) blindCount = i;
      saveConfig();
      saveRemotes();
      return i;
    }
  }
  return 0; // All slots occupied
}

bool removeBlind(int n) {
  if (!isValidBlind(n)) return false;
  if (activeBlindCount() <= MIN_BLINDS) return false;

  remoteId[n]    = 0;
  rollingCode[n] = 0;
  blindNames[n]  = "";
  blindTypes[n]  = BLIND_TYPE_BLIND;

  // Shrink blindCount to the highest remaining active slot
  while (blindCount > 0 && remoteId[blindCount] == 0) blindCount--;
  if (blindCount < MIN_BLINDS) blindCount = MIN_BLINDS;

  return saveConfig() && saveRemotes();
}

String blindName(int n) {
  if (!isValidBlind(n)) return "Blind ?";
  if (blindNames[n].length() == 0) return String("Blind ") + n;
  return blindNames[n];
}

String readWholeFile(const char* path) {
  if (!LittleFS.exists(path)) return String("null");
  File f = LittleFS.open(path, "r");
  if (!f) return String("null");
  String body; body.reserve(f.size() + 16);
  while (f && f.available()) body += char(f.read());
  f.close();
  if (body.length() == 0) return String("null");
  return body;
}

void setStr(char* dest, size_t len, const String& s) {
  strncpy(dest, s.c_str(), len);
  dest[len-1] = '\0';
}

uint32_t genRandom24() {
  uint32_t v = os_random() & 0xFFFFFF;
  if (v == 0) v = 0xABCDE1;
  return v;
}

bool isUniqueId(uint32_t candidate) {
  for (int i = 1; i <= MAX_BLINDS; i++) {
    if (remoteId[i] == candidate) return false;
  }
  return true;
}

void genAllRemoteIdsRandom() {
  for (int i = 1; i <= blindCount; i++) {
    uint32_t v; int guard = 0;
    do {
      v = genRandom24();
      if ((guard & 7) == 0) delay(0);
      guard++;
      if (guard > 200) break;
    } while (!isUniqueId(v));
    remoteId[i] = v;
    rollingCode[i] = 1;
    if ((i & 3) == 0) delay(0);
  }
}

bool loadConfig() {
  if (!LittleFS.exists(CONFIG_FILE)) {
    #if DEBUG_FS
    Serial.println("[CFG] /config.json missing");
    #endif
    return false;
  }

  File f = LittleFS.open(CONFIG_FILE, "r");
  if (!f) {
    #if DEBUG_FS
    Serial.println("[CFG] open(r) failed");
    #endif
    return false;
  }

  DynamicJsonDocument doc(2048);
  DeserializationError err = deserializeJson(doc, f);
  f.close();

  if (err) {
    #if DEBUG_FS
    Serial.printf("[CFG] parse error: %s (keeping defaults in RAM)\n", err.c_str());
    #endif
    return false;
  }

  if (doc.containsKey("ap_ssid"))      apSsid = String((const char*)doc["ap_ssid"]);
  if (doc.containsKey("ap_pass"))      apPass = String((const char*)doc["ap_pass"]);
  if (doc.containsKey("wifi_ssid"))    setStr(cfg.wifi_ssid,  sizeof(cfg.wifi_ssid),  String((const char*)doc["wifi_ssid"]));
  if (doc.containsKey("wifi_pass"))    setStr(cfg.wifi_pass,  sizeof(cfg.wifi_pass),  String((const char*)doc["wifi_pass"]));
  if (doc.containsKey("wifi_ssid2"))   setStr(cfg.wifi_ssid2, sizeof(cfg.wifi_ssid2), String((const char*)doc["wifi_ssid2"]));
  if (doc.containsKey("wifi_pass2"))   setStr(cfg.wifi_pass2, sizeof(cfg.wifi_pass2), String((const char*)doc["wifi_pass2"]));
  if (doc.containsKey("mqtt_server"))  setStr(cfg.mqtt_server, sizeof(cfg.mqtt_server), String((const char*)doc["mqtt_server"]));
  if (doc.containsKey("mqtt_port"))    cfg.mqtt_port = (int)doc["mqtt_port"];
  if (doc.containsKey("mqtt_user"))    setStr(cfg.mqtt_user, sizeof(cfg.mqtt_user), String((const char*)doc["mqtt_user"]));
  if (doc.containsKey("mqtt_pass"))    setStr(cfg.mqtt_pass, sizeof(cfg.mqtt_pass), String((const char*)doc["mqtt_pass"]));
  if (doc.containsKey("ha_discovery")) cfg.ha_discovery = (bool)doc["ha_discovery"];
  blindCountConfigured = doc.containsKey("blind_count");
  if (blindCountConfigured) blindCount = clampBlindCount((int)doc["blind_count"]);
  if (doc.containsKey("tx_pin"))       txPin = sanitizeGpio((int)doc["tx_pin"]);
  if (doc.containsKey("led_pin"))      ledPin = sanitizeLedGpio((int)doc["led_pin"]);
  if (doc.containsKey("led_invert"))   ledActiveLow = (bool)doc["led_invert"];
  if (doc.containsKey("btn_pin"))      buttonPin = sanitizeButtonGpio((int)doc["btn_pin"]);
  if (doc.containsKey("btn_invert"))   buttonActiveLow = (bool)doc["btn_invert"];

  #if DEBUG_FS
  Serial.println("[CFG] loaded OK (kept RAM defaults for any missing keys)");
  #endif
  return true;
}

bool fsEnsureWritable() {
  File t = LittleFS.open("/.w", "w");
  if (!t) {
    Serial.println("[FS] test write failed - formatting FS...");
    LittleFS.end();
    if (!LittleFS.format()) { Serial.println("[FS] format FAILED"); return false; }
    if (!LittleFS.begin())  { Serial.println("[FS] re-mount FAILED"); return false; }
    t = LittleFS.open("/.w", "w");
    if (!t) { Serial.println("[FS] test write still failing after format"); return false; }
  }
  t.print("ok");
  t.close();
  LittleFS.remove("/.w");
  return true;
}

bool saveConfig() {
  deviceId = normalizeDeviceId(deviceId);

  DynamicJsonDocument doc(1792);
  doc["device_id"]    = deviceId;
  doc["ap_ssid"]      = apSsid;
  doc["ap_pass"]      = apPass;
  doc["wifi_ssid"]    = cfg.wifi_ssid;
  doc["wifi_pass"]    = cfg.wifi_pass;
  doc["wifi_ssid2"]   = cfg.wifi_ssid2;
  doc["wifi_pass2"]   = cfg.wifi_pass2;
  doc["mqtt_server"]  = cfg.mqtt_server;
  doc["mqtt_port"]    = cfg.mqtt_port;
  doc["mqtt_user"]    = cfg.mqtt_user;
  doc["mqtt_pass"]    = cfg.mqtt_pass;
  doc["ha_discovery"] = cfg.ha_discovery;
  doc["blind_count"]  = blindCount;
  doc["tx_pin"]       = txPin;
  doc["led_pin"]      = ledPin;
  doc["led_invert"]   = ledActiveLow;
  doc["btn_pin"]      = buttonPin;
  doc["btn_invert"]   = buttonActiveLow;

  File f = LittleFS.open(CONFIG_FILE, "w");
  if (!f) return false;
  size_t n = serializeJson(doc, f);
  f.flush();
  f.close();

  return (n > 0);
}

bool loadRemotes() {
  if (!LittleFS.exists(REMOTES_FILE)) { Serial.println("[REM] no file"); return false; }

  File f = LittleFS.open(REMOTES_FILE, "r");
  if (!f) { Serial.println("[REM] open FAIL"); return false; }

  size_t sz = f.size();
  size_t cap = sz + (sz >> 2) + 1024;
  if (cap < 4096)  cap = 4096;
  if (cap > 49152) cap = 49152;

  DynamicJsonDocument doc(cap);
  DeserializationError err = deserializeJson(doc, f, DeserializationOption::NestingLimit(4));
  f.close();

  if (err) {
    Serial.printf("[REM] parse error: %s (size=%u cap=%u)\n", err.c_str(), (unsigned)sz, (unsigned)cap);
    return false;
  }

  int inferredBlindCount = MIN_BLINDS;
  for (int i = 1; i <= MAX_BLINDS; i++) {
    String rk = "r"  + String(i);
    String ck = "rc" + String(i);
    String nk = "n"  + String(i);
    String tk = "t"  + String(i);
    bool hasAny = doc.containsKey(rk) || doc.containsKey(ck) || doc.containsKey(nk);
    if (hasAny) inferredBlindCount = i;

    if (!doc.containsKey(rk)) {
      // Absent key = inactive slot, leave as zero
      remoteId[i] = 0; rollingCode[i] = 0; blindNames[i] = ""; blindTypes[i] = BLIND_TYPE_BLIND;
      continue;
    }
    remoteId[i]    = (uint32_t)(doc[rk] | 0);
    rollingCode[i] = (uint16_t)(doc[ck] | 1);
    blindNames[i]  = doc[nk] | ("Blind " + String(i));
    blindTypes[i]  = sanitizeBlindType((int)(doc[tk] | BLIND_TYPE_BLIND));
    if (remoteId[i] == 0) { blindNames[i] = ""; continue; } // Explicitly saved as inactive
    if (rollingCode[i] == 0) rollingCode[i] = 1;
  }
  if (!blindCountConfigured) blindCount = clampBlindCount(inferredBlindCount);

  Serial.printf("[REM] loaded OK (%u bytes)\n", (unsigned)sz);
  return true;
}

String jsonEscape(const String& s) {
  String o; o.reserve(s.length()+8);
  for (size_t i=0;i<s.length();++i) {
    char c=s[i];
    if (c=='"' || c=='\\') { o += '\\'; o += c; }
    else if (c=='\n') o += "\\n";
    else if (c=='\r') o += "\\r";
    else if (c=='\t') o += "\\t";
    else o += c;
  }
  return o;
}

bool extractTopObject(const String& src, const char* key, String& out) {
  out = "";
  String pat = "\"" + String(key) + "\"";
  int k = src.indexOf(pat);
  if (k < 0) return false;
  int colon = src.indexOf(':', k + pat.length());
  if (colon < 0) return false;
  int i = colon + 1;
  while (i < (int)src.length() && (src[i] == ' ' || src[i] == '\n' || src[i] == '\r' || src[i] == '\t')) i++;
  if (i >= (int)src.length() || src[i] != '{') return false;

  int start = i, depth = 0;
  for (; i < (int)src.length(); i++) {
    char c = src[i];
    if (c == '{') depth++;
    else if (c == '}') {
      depth--;
      if (depth == 0) {
        out = src.substring(start, i + 1);
        return true;
      }
    }
  }
  return false;
}

bool saveRemotes() {
  File f = LittleFS.open(REMOTES_FILE, "w");
  if (!f) return false;

  f.print("{\"v\":1");
  for (int i = 1; i <= blindCount; i++) {
    if (remoteId[i] == 0) continue; // Skip inactive slots
    f.print(",\"r");  f.print(i); f.print("\":"); f.print(remoteId[i]);
    f.print(",\"rc"); f.print(i); f.print("\":"); f.print(rollingCode[i]);
    f.print(",\"n");  f.print(i); f.print("\":\"");
    f.print(jsonEscape(blindNames[i]));
    f.print("\"");
    f.print(",\"t");  f.print(i); f.print("\":"); f.print(blindTypes[i]);
    if ((i % 4) == 0) delay(0);
  }
  f.print("}");
  f.flush();
  f.close();
  return true;
}
