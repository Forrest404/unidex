#pragma once
#include <time.h>

void clockBegin();        // once per boot: timezone, then load the PCF85063 time into system time
bool clockValid();
const char *clockSync();  // NTP over WiFi, radio off after; nullptr on success, else a short reason
void clockSet(time_t utc);  // system time + clock chip (used by the Mac USB sync)
