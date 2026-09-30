#include "battery.h"
#include <Arduino.h>

static const int PIN_BATTERY = 4;  // ADC1 ch3, behind a divider by 2

// Calibration: with a full battery and the charger finished, the raw reading was 4.08-4.09 V where the
// cell really sits at about 4.17 V, so the ADC reads about 2% low through the 200k/200k divider.
static const float CALIBRATION = 4170.0f / 4085.0f;

// Resting LiPo curve, mV -> %. Voltage is far from linear in charge, so interpolate between points.
// 100% from 4.15 V: a full cell settles there within minutes of the charger stopping.
static const struct { int mv, pct; } CURVE[] = {
  {3300, 0},  {3610, 5},  {3690, 10}, {3710, 15}, {3730, 20}, {3750, 25}, {3770, 30},
  {3790, 35}, {3800, 40}, {3820, 45}, {3840, 50}, {3850, 55}, {3870, 60}, {3910, 65},
  {3950, 70}, {3980, 75}, {4020, 80}, {4080, 85}, {4110, 90}, {4150, 100},
};
static const int POINTS = sizeof(CURVE) / sizeof(CURVE[0]);

int batteryMillivolts() {
  analogSetPinAttenuation(PIN_BATTERY, ADC_11db);  // the pin sees about 2.1 V at a full battery
  uint32_t sum = 0;
  for (int i = 0; i < 32; i++) sum += analogReadMilliVolts(PIN_BATTERY);
  return sum / 32 * 2 * CALIBRATION;
}

static int fromCurve(int mv) {
  if (mv <= CURVE[0].mv) return 0;
  for (int i = 1; i < POINTS; i++)
    if (mv < CURVE[i].mv)
      return CURVE[i - 1].pct + (mv - CURVE[i - 1].mv) * (CURVE[i].pct - CURVE[i - 1].pct) / (CURVE[i].mv - CURVE[i - 1].mv);
  return 100;
}

int batteryPercent() {
  RTC_DATA_ATTR static int8_t shown = -1;  // survives sleep
  const int mv = batteryMillivolts();
  if (mv < 2500) return -1;  // pin floating: no battery connected
  const int pct = fromCurve(mv);
  // On battery it only goes down, so ADC noise can't make it bounce. It follows the reading while
  // on USB, and on a jump of 5 or more (charging from a power bank, which sends no USB data).
  if (shown < 0 || batteryCharging() || pct < shown || pct >= shown + 5) shown = pct;
  return shown;
}

// No charger-status pin is read, so this means "USB power present", not proof the battery is taking charge:
// a full battery on USB shows it too. A dumb charger sends no USB data, so the voltage is the fallback.
bool batteryCharging() {
  return HWCDC::isPlugged() || batteryMillivolts() > 4250;
}
