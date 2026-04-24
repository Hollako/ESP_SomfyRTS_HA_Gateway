#ifndef MQTT_HA_H
#define MQTT_HA_H

#include "app_state.h"
#include <ArduinoJson.h>

uint16_t getRolling(int b);
void setRolling(int b, uint16_t next);
void sendSomfy(int blind, byte button);

String availabilityTopic();
String blindBaseTopic(int n);
String gatewayBaseTopic();
void publishAvailability(const char* payload);
void publishState(int n, const char* state);

void addHadeviceBlock(JsonDocument& doc);
void publishHACover(int n);
void publishHAProgButton(int n);
void publishHAGatewayDiscovery();
void publishGatewayDiagnostics();
void clearHABlindDiscovery(int n);
void publishAllDiscoverySafe();

void factoryResetAndReboot();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void mqttEnsureConnected();

#endif
