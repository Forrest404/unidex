#include "battery.h"
#include <Arduino.h>

static const int PIN_BATTERY = 4;  // ADC1 ch3, behind a divider by 2

// LiPo discharge curve, mV -> %. Voltage is far from linear in charge, so interpolate between points.
static const struct { int mv, pct; } CURVE[] = {
  {3300, 0}, {3600, 15}, {3750, 40}, {3950, 70}, {4200, 100},
};
static const int POINTS = sizeof(CURVE) / sizeof(CURVE[0]);

int batteryMillivolts() {
  analogSetPinAttenuation(PIN_BATTERY, ADC_11db);  // the pin sees about 2.1 V at a full battery
  uint32_t sum = 0;
  for (int i = 0; i < 8; i++) sum += analogReadMilliVolts(PIN_BATTERY);
  return sum / 8 * 2;
}

int batteryPercent() {
  const int mv = batteryMillivolts();
  if (mv < 2500) return -1;  // pin floating: no battery connected
  if (mv <= CURVE[0].mv) return 0;
  for (int i = 1; i < POINTS; i++)
    if (mv < CURVE[i].mv)
      return CURVE[i - 1].pct + (mv - CURVE[i - 1].mv) * (CURVE[i].pct - CURVE[i - 1].pct) / (CURVE[i].mv - CURVE[i - 1].mv);
  return 100;
}

// No charger-status pin is read, so this means "USB power present", not proof the battery is taking charge:
// a full battery on USB shows it too. A dumb charger sends no USB data, so the voltage is the fallback.
bool batteryCharging() {
  return HWCDC::isPlugged() || batteryMillivolts() > 4250;
}
