#pragma once

void clockBegin();        // once per boot: timezone, then load the PCF85063 time into system time
bool clockValid();
const char *clockSync();  // NTP over WiFi, radio off after; nullptr on success, else a short reason
