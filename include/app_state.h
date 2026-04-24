#ifndef APP_STATE_H
#define APP_STATE_H

#include <Arduino.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>

#include "SomfyRemote.h"

#define MQTT_MAX_PACKET_SIZE 1024
#include <PubSubClient.h>

#define DEFAULT_AP_SSID_PREFIX  "ESPSomfyRemote_"
#define DEFAULT_AP_PASS         ""
#define CONFIG_FILE             "/config.json"
#define REMOTES_FILE            "/remotes.json"
#define EEPROM_SIZE             512
#define DEBUG_FS                0

#define HA_DISCOVERY_PREFIX     "homeassistant"
#define BASE_TOPIC_PREFIX       "somfy"
#define HA_ONLINE               "online"
#define HA_OFFLINE              "offline"
#define MAX_BLINDS              32
#define MIN_BLINDS              1

#define BLIND_TYPE_BLIND        0
#define BLIND_TYPE_SHADE        1
#define BLIND_TYPE_SHUTTER      2
#define BLIND_TYPE_CURTAIN      3
#define BLIND_TYPE_AWNING       4
#define BLIND_TYPE_WINDOW       5

struct DevIdBlob {
  uint32_t magic;
  char     id[32];
  uint8_t  crc;
} __attribute__((packed));

struct AppConfig {
  char device_id[32];
  char wifi_ssid[64];
  char wifi_pass[64];
  char mqtt_server[64];
  int  mqtt_port;
  char mqtt_user[32];
  char mqtt_pass[32];
  bool ha_discovery;
};

extern const uint32_t DEV_ID_MAGIC;
extern const char* HA_MANUFACTURER;
extern const char* HA_MODEL;
extern const char* HA_SW_VERSION;

extern ESP8266WebServer server;
extern WiFiClient espClient;
extern PubSubClient mqtt;
extern SomfyRemote somfy;

extern int txPin;
extern int ledPin;
extern bool ledActiveLow;
extern int buttonPin;
extern bool buttonActiveLow;

extern volatile bool rebootScheduled;
extern unsigned long rebootAtMs;

extern volatile bool regenAllScheduled;
extern volatile bool rediscoveryScheduled;
extern uint8_t rediscoveryPtr;
extern unsigned long nextRediscoveryAt;

extern bool staBusy;
extern unsigned long nextStaAttempt;
extern const unsigned long STA_CONNECT_GRACE;
extern const unsigned long STA_RETRY_INTERVAL;
extern const uint8_t MAX_STA_RETRY;

extern String deviceId;
extern String apSsid;
extern String apPass;
extern AppConfig cfg;

extern const IPAddress AP_IP;
extern const IPAddress AP_GW;
extern const IPAddress AP_SN;

extern volatile wl_status_t staStatus;
extern volatile uint8_t staDiscReason;
extern String staLastEvent;
extern String staTriedSsid;
extern uint8_t staRetries;
extern unsigned long lastStaChangeMs;

extern bool apActive;
extern unsigned long staConnectedAt;

extern uint8_t blindCount;
extern bool blindCountConfigured;

extern uint32_t remoteId[MAX_BLINDS + 1];
extern uint16_t rollingCode[MAX_BLINDS + 1];
extern String blindNames[MAX_BLINDS + 1];
extern uint8_t blindTypes[MAX_BLINDS + 1];

#endif
