#pragma once

int batteryMillivolts();  // battery voltage in mV (GPIO4 through the board's divider by 2)
int batteryPercent();     // 0-100 estimated from voltage, or -1 if no battery is detected
bool batteryCharging();    // USB power seems present (host attached, or voltage above a full battery)
