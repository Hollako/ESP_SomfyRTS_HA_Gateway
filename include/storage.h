#ifndef STORAGE_H
#define STORAGE_H

#include "app_state.h"

uint8_t crc8(const uint8_t* data, size_t len);
bool eepromLoadDeviceId(String& out);
bool eepromSaveDeviceId(const String& id);
String normalizeDeviceId(const String& raw);
uint8_t sanitizeBlindType(int rawType);
const char* blindHaDeviceClass(int n);

String blindName(int n);
int clampBlindCount(int n);
bool isValidBlind(int n);
bool addBlind();
bool removeBlind(int n);
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
