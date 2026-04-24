#ifndef STORAGE_H
#define STORAGE_H

#include "app_state.h"

uint8_t crc8(const uint8_t* data, size_t len);
bool eepromLoadDeviceId(String& out);
bool eepromSaveDeviceId(const String& id);

String blindName(int n);
String readWholeFile(const char* path);

void setStr(char* dest, size_t len, const String& s);
uint32_t genRandom24();
bool isUniqueId(uint32_t candidate);
void genAllRemoteIdsRandom();

bool loadConfig();
bool fsEnsureWritable();
bool saveConfig();
bool loadRemotes();
bool saveRemotes();

bool extractTopObject(const String& src, const char* key, String& out);
String jsonEscape(const String& s);

#endif