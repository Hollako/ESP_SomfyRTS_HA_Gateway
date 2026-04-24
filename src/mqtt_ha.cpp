#include "mqtt_ha.h"

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <EEPROM.h>

#include "storage.h"

uint16_t getRolling(int b) {
  if (b < 1 || b > 32) return 1;
  if (rollingCode[b] == 0) rollingCode[b] = 1;
  return rollingCode[b];
}

void setRolling(int b, uint16_t next) {
  if (b < 1 || b > 32) return;
  rollingCode[b] = next;
  saveRemotes();
}

void sendSomfy(int blind, byte button) {
  if (blind < 1 || blind > 32) return;
  uint16_t next = somfy.sendButton(remoteId[blind], button, getRolling(blind));
  setRolling(blind, next);
}

String availabilityTopic() {
  return String(BASE_TOPIC_PREFIX) + "/" + deviceId + "/availability";
}

String blindBaseTopic(int n) {
  return String(BASE_TOPIC_PREFIX) + "/" + deviceId + "/blind_" + String(n);
}

void publishAvailability(const char* payload) {
  mqtt.publish(availabilityTopic().c_str(), payload, true);
}

void publishState(int n, const char* state) {
  mqtt.publish((blindBaseTopic(n) + "/state").c_str(), state, true);
}

void addHadeviceBlock(JsonDocument& doc) {
  JsonObject dev = doc.createNestedObject("device");

  JsonArray ids = dev.createNestedArray("identifiers");
  ids.add(deviceId);

  String mac = WiFi.macAddress();
  JsonArray conns = dev.createNestedArray("connections");
  conns.add(JsonArray());
  conns[0].add("mac");
  conns[0].add(mac);

  dev["manufacturer"] = HA_MANUFACTURER;
  dev["model"] = HA_MODEL;
  dev["name"] = String("Somfy RTS ") + deviceId;
  dev["sw_version"] = HA_SW_VERSION;

  IPAddress ip = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP() : WiFi.softAPIP();
  dev["configuration_url"] = String("http://") + ip.toString() + "/";
}

void publishHACover(int n) {
  String discTopic = String(HA_DISCOVERY_PREFIX) + "/cover/" + deviceId + "/blind_" + String(n) + "/config";

  DynamicJsonDocument doc(768);
  doc["name"] = blindName(n);
  doc["unique_id"] = deviceId + String("_blind_") + String(n);
  doc["command_topic"] = blindBaseTopic(n) + "/set";
  doc["stop_command_topic"] = blindBaseTopic(n) + "/stop";
  doc["state_topic"] = blindBaseTopic(n) + "/state";
  doc["availability_topic"] = availabilityTopic();
  doc["payload_open"] = "OPEN";
  doc["payload_close"] = "CLOSE";
  doc["payload_stop"] = "STOP";
  doc["optimistic"] = true;

  addHadeviceBlock(doc);

  String payload;
  serializeJson(doc, payload);
  mqtt.publish(discTopic.c_str(), payload.c_str(), true);
}

void publishHAProgButton(int n) {
  String discTopic = String(HA_DISCOVERY_PREFIX) + "/button/" + deviceId + "/blind_" + String(n) + "_prog/config";

  DynamicJsonDocument doc(512);
  doc["name"] = blindName(n) + " PROG";
  doc["unique_id"] = deviceId + String("_blind_") + String(n) + "_prog";
  doc["command_topic"] = blindBaseTopic(n) + "/prog";
  doc["availability_topic"] = availabilityTopic();
  doc["payload_press"] = "PRESS";

  addHadeviceBlock(doc);

  String payload;
  serializeJson(doc, payload);
  mqtt.publish(discTopic.c_str(), payload.c_str(), true);
}

void publishAllDiscoverySafe() {
  if (!cfg.ha_discovery) return;
  for (int i = 1; i <= 32; i++) {
    publishHACover(i);
    publishHAProgButton(i);
    publishState(i, "unknown");
    if ((i & 3) == 0) delay(0);
  }
}

void factoryResetAndReboot() {
  LittleFS.remove(CONFIG_FILE);
  LittleFS.remove(REMOTES_FILE);
  DevIdBlob z = {};
  EEPROM.put(0, z);
  EEPROM.commit();
  ESP.restart();
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String msg; msg.reserve(length);
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];
  String top = String(topic);

  String prefix = String(BASE_TOPIC_PREFIX) + "/" + deviceId + "/blind_";
  if (!top.startsWith(prefix)) return;

  String rest = top.substring(prefix.length());
  int slash = rest.indexOf('/');
  if (slash < 0) return;
  int blind = rest.substring(0, slash).toInt();
  if (blind < 1 || blind > 32) return;
  String action = rest.substring(slash + 1);

  if (action == "set") {
    if (msg == "OPEN") {
      sendSomfy(blind, REMOTE_RAISE);
      publishState(blind, "open");
    } else if (msg == "CLOSE") {
      sendSomfy(blind, REMOTE_LOWER);
      publishState(blind, "closed");
    } else if (msg == "STOP") {
      sendSomfy(blind, REMOTE_STOP);
      publishState(blind, "stopped");
    }
  } else if (action == "stop") {
    sendSomfy(blind, REMOTE_STOP);
    publishState(blind, "stopped");
  } else if (action == "prog") {
    if (msg == "PRESS") sendSomfy(blind, REMOTE_PROG);
  }
}

void mqttEnsureConnected() {
  static unsigned long lastMqttAttempt = 0;
  const unsigned long MQTT_RETRY_MS = 5000;

  if (String(cfg.mqtt_server).length() == 0) return;
  if (WiFi.status() != WL_CONNECTED) return;
  if (mqtt.connected()) return;
  if (millis() - lastMqttAttempt < MQTT_RETRY_MS) return;

  lastMqttAttempt = millis();

  mqtt.setServer(cfg.mqtt_server, cfg.mqtt_port);
  mqtt.setBufferSize(1024);
  mqtt.setCallback(mqttCallback);

  String willTopic = availabilityTopic();
  String cid = "SomfyESP-" + deviceId + "-" + String(ESP.getChipId(), HEX);

  bool ok;
  if (strlen(cfg.mqtt_user) > 0) {
    ok = mqtt.connect(cid.c_str(), cfg.mqtt_user, cfg.mqtt_pass, willTopic.c_str(), 0, true, HA_OFFLINE);
  } else {
    ok = mqtt.connect(cid.c_str(), willTopic.c_str(), 0, true, HA_OFFLINE);
  }

  if (ok) {
    publishAvailability(HA_ONLINE);
    for (int i = 1; i <= 32; i++) {
      mqtt.subscribe((blindBaseTopic(i) + "/set").c_str());
      mqtt.subscribe((blindBaseTopic(i) + "/stop").c_str());
      mqtt.subscribe((blindBaseTopic(i) + "/prog").c_str());
    }
    rediscoveryScheduled = true;
    rediscoveryPtr = 1;
    nextRediscoveryAt = millis() + 300;
  }
}
