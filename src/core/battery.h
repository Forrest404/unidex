#pragma once

int batteryMillivolts();  // battery voltage in mV (GPIO4 through the board's divider by 2)
int batteryPercent();     // 0-100 estimated from voltage, or -1 if no battery is detected
bool batteryCharging();    // USB power seems present (host attached, or voltage above a full battery)
// nullptr when there's charge for heavy work (WiFi, the radio, recording), else the reason to show ("Battery too
// low"). Always fine on USB or with no battery fitted.
const char *batteryTooLow();
bool batteryEmpty();  // flat: time to switch off (power.cpp does)
#if UNIDEX_DEV
void batteryFake(int mv);  // test build: pretend the battery reads mv (0: the real reading)
#endif
