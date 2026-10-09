#include "battery.h"
#include <Arduino.h>
#include "battery_logic.h"

static const int PIN_BATTERY = 4;  // ADC1 ch3, behind a divider by 2

// Calibration: with a full battery and the charger finished, the raw reading was 4.08-4.09 V where the
// cell really sits at about 4.17 V, so the ADC reads about 2% low through the 200k/200k divider.
static const float CALIBRATION = 4170.0f / 4085.0f;

#if UNIDEX_DEV
static int fakeMv;  // test build (X BATTMV <mv>): pretend the battery reads this (0 = the real reading)
void batteryFake(int mv) { fakeMv = mv; }
#endif

int batteryMillivolts() {
#if UNIDEX_DEV
  if (fakeMv) return fakeMv;
#endif
  analogSetPinAttenuation(PIN_BATTERY, ADC_11db);  // the pin sees about 2.1 V at a full battery
  uint32_t sum = 0;
  for (int i = 0; i < 32; i++) sum += analogReadMilliVolts(PIN_BATTERY);
  return sum / 32 * 2 * CALIBRATION;
}

RTC_DATA_ATTR static battery::State state;  // survives sleep (constant-initialized, so set only at power-up)

#if UNIDEX_DEV
// Test build: every % worked out (by the screens or the battery log), for X BATTCALLS.
struct Call {
  uint32_t ms;
  int16_t mv;
  int8_t pct;
  uint8_t plugged, charging;
};
static Call calls[300];
static uint32_t callCount;
void batteryPrintCalls() {
  for (uint32_t i = callCount > 300 ? callCount - 300 : 0; i < callCount; i++) {
    const Call &c = calls[i % 300];
    Serial.printf("BC %lu %d %d %d %d\n", (unsigned long)c.ms, c.mv, c.plugged, c.charging, c.pct);
  }
}
#endif

int batteryPercent() {
  const int mv = batteryMillivolts();
  const bool charging = batteryCharging();
  const int pct = battery::update(state, mv, charging, millis());
#if UNIDEX_DEV
  calls[callCount++ % 300] = {millis(), (int16_t)mv, (int8_t)pct, (uint8_t)HWCDC::isPlugged(), (uint8_t)charging};
#endif
  return pct;
}

const char *batteryTooLow() {
  return battery::level(batteryMillivolts(), batteryCharging()) == battery::Level::OK ? nullptr
                                                                                     : "Battery too low";
}

bool batteryEmpty() { return battery::level(batteryMillivolts(), batteryCharging()) == battery::Level::EMPTY; }

// No charger-status pin is read, so this means "USB power present", not proof the battery is taking charge:
// a full battery on USB shows it too. A dumb charger sends no USB data, so the voltage is the fallback.
bool batteryCharging() {
#if UNIDEX_DEV
  if (fakeMv) return fakeMv >= 4190;  // a pretend reading: as if on battery
#endif
  return HWCDC::isPlugged() || batteryMillivolts() >= 4190;  // the charger holds 4.20 V while topping up
}
