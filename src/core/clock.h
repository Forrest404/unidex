#pragma once
#include <time.h>

void clockBegin();        // once per boot: timezone, then load the PCF85063 time into system time
bool clockValid();
const char *clockSync();  // NTP over WiFi, radio off after; nullptr on success, else a short reason
const char *clockSyncOnline();  // the same with WiFi already up (left on)
bool clockSet(time_t utc);  // false (and nothing changed) for a time outside 2024-2099
void clockStatus(char *out, size_t len);  // chip registers + system time, for the USB `C` command
struct tm clockLocal();   // the system time in London time
