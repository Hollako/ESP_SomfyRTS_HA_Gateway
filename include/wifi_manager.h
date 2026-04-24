#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "app_state.h"

int sanitizeGpio(int pin);
int sanitizeLedGpio(int pin);
int sanitizeButtonGpio(int pin);

void setLed(bool on);
void applyLedPin();
void applyButtonPin();
void pollButtonLongPress();

void scheduleReboot(uint32_t delayMs = 1200);
void serviceScheduledReboot();

bool isApPortalMode();
void beginSTAIfCreds();
void startAP();
void stopAP();
void updateWiFiSM();
void installWiFiDebugHandlers();

#endif