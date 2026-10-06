#include "battery.h"
#include <Arduino.h>
#include "battery_logic.h"

static const int PIN_BATTERY = 4;  // ADC1 ch3, behind a divider by 2

// Calibration: with a full battery and the charger finished, the raw reading was 4.08-4.09 V where the
// cell really sits at about 4.17 V, so the ADC reads about 2% low through the 200k/200k divider.
static const float CALIBRATION = 4170.0f / 4085.0f;

int batteryMillivolts() {
  analogSetPinAttenuation(PIN_BATTERY, ADC_11db);  // the pin sees about 2.1 V at a full battery
  uint32_t sum = 0;
  for (int i = 0; i < 32; i++) sum += analogReadMilliVolts(PIN_BATTERY);
  return sum / 32 * 2 * CALIBRATION;
}

RTC_DATA_ATTR static battery::State state;  // survives sleep (constant-initialized, so set only at power-up)

int batteryPercent() {
  return battery::update(state, batteryMillivolts(), batteryCharging(), millis());
}

// No charger-status pin is read, so this means "USB power present", not proof the battery is taking charge:
// a full battery on USB shows it too. A dumb charger sends no USB data, so the voltage is the fallback.
bool batteryCharging() {
  return HWCDC::isPlugged() || batteryMillivolts() >= 4190;  // the charger holds 4.20 V while topping up
}
