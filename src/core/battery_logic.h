#pragma once
#include <stdint.h>

// Battery % from the voltage, with no Arduino in it (tools/batterytest checks it on a computer).
// The board has no charge-status pin and no current sensor, so while charging the % is an estimate:
// the charger lifts the reading above the battery's resting voltage (then holds it at 4.20 V), which
// read through the resting curve would show ~100% at once.
namespace battery {

// Resting LiPo curve, mV -> %. Voltage is far from linear in charge, so interpolate between points.
// 100% from 4.15 V: a full cell settles there within minutes of the charger stopping.
const struct { int mv, pct; } CURVE[] = {
  {3300, 0},  {3610, 5},  {3690, 10}, {3710, 15}, {3730, 20}, {3750, 25}, {3770, 30},
  {3790, 35}, {3800, 40}, {3820, 45}, {3840, 50}, {3850, 55}, {3870, 60}, {3910, 65},
  {3950, 70}, {3980, 75}, {4020, 80}, {4080, 85}, {4110, 90}, {4150, 100},
};
const int POINTS = sizeof(CURVE) / sizeof(CURVE[0]);

const int NO_BATTERY_MV = 2500;  // below this the pin is floating: no battery connected
const int TOPUP_MV = 4190;       // the charger holds 4.20 V while topping up; the voltage says no more
const int MAX_OFFSET = 300;      // how far the charger can lift the reading above resting (mV)
const int DEFAULT_OFFSET = 120;  // the lift when there's no reading from before plugging in (mV)
const int TOPUP_MIN = 40;        // minutes at 4.20 V from the start of topping up to full

inline int fromCurve(int mv) {
  if (mv <= CURVE[0].mv) return 0;
  for (int i = 1; i < POINTS; i++)
    if (mv < CURVE[i].mv)
      return CURVE[i - 1].pct + (mv - CURVE[i - 1].mv) * (CURVE[i].pct - CURVE[i - 1].pct) / (CURVE[i].mv - CURVE[i - 1].mv);
  return 100;
}

struct State {
  int16_t restMv = 0;     // last reading on battery (0: none since power-up)
  int16_t offset = 0;     // how much the charger lifts the reading, measured at plug-in
  int8_t shown = -1;      // the % last shown (-1: none yet)
  int8_t topupFrom = 0;   // the % when topping up started
  bool charging = false;
  bool toppingUp = false;
  uint32_t topupAt = 0;   // ms when topping up started
};

// The % to show for a reading of `mv`, or -1 with no battery. On battery the % only goes down, so ADC noise
// can't make it bounce (except a jump of 5 or more: charging from a power bank, which sends no USB data).
// While charging it only goes up: the reading minus the lift measured at plug-in, then, once the charger
// holds 4.20 V, a steady climb to 100 over TOPUP_MIN minutes.
inline int update(State &s, int mv, bool charging, uint32_t nowMs) {
  if (mv < NO_BATTERY_MV) return -1;
  int pct;
  if (!charging) {
    s.charging = s.toppingUp = false;
    s.restMv = mv;
    pct = fromCurve(mv);
    if (s.shown < 0 || pct < s.shown || pct >= s.shown + 5) s.shown = pct;
    return s.shown;
  }
  if (!s.charging) {  // just plugged in
    s.charging = true;
    const int lift = s.restMv ? mv - s.restMv : DEFAULT_OFFSET;
    s.offset = lift < 0 ? 0 : lift > MAX_OFFSET ? MAX_OFFSET : lift;
  }
  // Topping up: the charger holds 4.20 V. Started up on USB with nothing to compare with, a reading of a
  // full battery could equally be one that's topping up or done, so count from now either way.
  if (!s.toppingUp && (mv >= TOPUP_MV || (!s.restMv && mv >= CURVE[POINTS - 1].mv))) {
    s.toppingUp = true;
    s.topupAt = nowMs;
    const int from = s.shown >= 0 ? s.shown : fromCurve(mv - s.offset);
    s.topupFrom = from > 99 ? 99 : from;
  }
  if (s.toppingUp) {
    const uint32_t minutes = (nowMs - s.topupAt) / 60000;
    pct = minutes >= (uint32_t)TOPUP_MIN ? 100 : s.topupFrom + (100 - s.topupFrom) * (int)minutes / TOPUP_MIN;
    if (minutes < (uint32_t)TOPUP_MIN && pct > 99) pct = 99;
  } else {
    pct = fromCurve(mv - s.offset);
  }
  if (pct > s.shown) s.shown = pct;
  return s.shown;
}

// How much charge is left for heavy work. On USB power, or with no battery fitted (it reads near 0 V), it's always OK.
// LOW: enough to keep using the device, not for WiFi, the radio or recording (a brown-out in the middle of a card write
// can damage it). EMPTY: time to switch off, so a flat battery isn't drained further.
enum class Level { OK, LOW_, EMPTY };
const int LOW_MV = 3450, EMPTY_MV = 3350;  // warn and refuse heavy work; switch off

inline Level level(int mv, bool charging) {
  if (charging || mv < NO_BATTERY_MV) return Level::OK;
  if (mv <= EMPTY_MV) return Level::EMPTY;
  if (mv <= LOW_MV) return Level::LOW_;
  return Level::OK;
}

}  // namespace battery
