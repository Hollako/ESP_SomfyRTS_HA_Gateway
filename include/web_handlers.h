#ifndef WEB_HANDLERS_H
#define WEB_HANDLERS_H

#include <Arduino.h>

void sendRebootingPage(const String& title, const String& detail, int countdownSec = 10, uint32_t startPollDelayMs = 5000);

void handleRootGet();
void handleApPortalGet();
void handleApPortalConfigPost();
void handleConfigPost();
void handleConfigGet();
void handleRegenPost();
void handleRestoreBackupUpload();
void handleRestoreBackupPost();
void handleUpdateGet();
void handleUpdatePost();
void handleUpdateUpload();

#endif
