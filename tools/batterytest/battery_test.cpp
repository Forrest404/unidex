// Checks the battery % (on battery and while charging) on a computer (no board needed):
//   c++ -std=c++17 -O2 -I src/core tools/batterytest/battery_test.cpp -o /tmp/battery_test && /tmp/battery_test
#include <cstdio>
#include <cstdlib>
#include "battery_logic.h"

using namespace battery;
static int failures;

static void check(bool ok, const char *what) {
  printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
  failures += !ok;
}

static const uint32_t MIN = 60000;

// The resting voltage of a battery at `pct` (the curve read backwards).
static int restingMv(int pct) {
  for (int i = 1; i < POINTS; i++)
    if (pct <= CURVE[i].pct)
      return CURVE[i - 1].mv + (pct - CURVE[i - 1].pct) * (CURVE[i].mv - CURVE[i - 1].mv) / (CURVE[i].pct - CURVE[i - 1].pct);
  return CURVE[POINTS - 1].mv;
}

// A small, repeatable ±20 mV of ADC noise.
static int noise(uint32_t &rng) {
  rng ^= rng << 13, rng ^= rng >> 17, rng ^= rng << 5;
  return (int)(rng % 41) - 20;
}

int main() {
  {
    State s;
    check(update(s, 1200, false, 0) == -1, "no battery: below 2.5 V shows nothing");
  }
  {
    State s;
    const int before = update(s, restingMv(60), false, 0);
    // The charger lifts the reading by ~130 mV: read through the resting curve that's ~81%.
    const int lifted = restingMv(60) + 130;
    const int now = update(s, lifted, true, MIN);
    printf("      60%% on battery, plugged in: shows %d%% (resting curve of the lifted reading: %d%%)\n", now,
           fromCurve(lifted));
    check(before == 60 && now >= 58 && now <= 62, "plugging in at 60% still shows about 60%");
  }
  {
    // A whole charge from 30%: the cell gains 1% a minute, the charger lifts the reading 150 mV, then holds
    // 4.20 V from ~85% for the last 35 minutes. The % shown must never drop, never reach 100 too early, and
    // stay near the real charge.
    State s;
    uint32_t rng = 7, t = 0;
    for (int i = 0; i < 10; i++, t += MIN) update(s, restingMv(30) + noise(rng), false, t);
    int shown = 0, worst = 0, lastShown = 0;
    bool dropped = false, early100 = false;
    double soc = 30;
    for (int m = 0; m < 200; m++, t += MIN) {
      int mv = restingMv((int)soc) + 150;
      if (mv >= 4200) {
        mv = 4200;
        soc += (100 - soc) / 12;  // the current tapers
      } else {
        soc += 1;
      }
      if (soc > 100) soc = 100;
      shown = update(s, mv + noise(rng), true, t);
      if (m && shown < lastShown) dropped = true;
      if (shown == 100 && soc < 95) early100 = true;
      const int off = abs(shown - (int)soc);
      if (off > worst) worst = off;
      lastShown = shown;
    }
    printf("      whole charge from 30%%: furthest from the real charge %d points, ends at %d%%\n", worst, shown);
    check(!dropped, "while charging the % never goes down, even with noise");
    check(!early100, "never 100% while the real charge is below 95%");
    check(shown == 100, "a whole charge ends at 100%");
    check(worst <= 15, "while charging the % stays within 15 of the real charge");

    // Unplug: the reading falls back to a full cell's resting 4.17 V, and it follows the resting curve again.
    const int after = update(s, 4170, false, t);
    check(after == 100, "unplugged full: shows 100%");
    const int later = update(s, restingMv(80), false, t + 60 * MIN);
    check(later == 80, "on battery again it follows the resting curve down");
  }
  {
    // Topping up starts at 85%: 99 at most until TOPUP_MIN minutes, then 100.
    State s;
    update(s, restingMv(85), false, 0);
    update(s, restingMv(85) + 100, true, MIN);
    const int start = update(s, 4200, true, 2 * MIN);
    const int middle = update(s, 4200, true, 2 * MIN + TOPUP_MIN / 2 * MIN);
    const int nearly = update(s, 4200, true, 2 * MIN + (TOPUP_MIN - 1) * MIN);
    const int done = update(s, 4200, true, 2 * MIN + TOPUP_MIN * MIN);
    printf("      topping up from 85%%: %d, %d, %d, %d\n", start, middle, nearly, done);
    check(start == 85 && middle > 85 && middle < 99 && nearly == 99 && done == 100,
          "topping up climbs steadily, 99 at most until the time is up");
  }
  {
    // Started up on USB (no reading from before): a half-charged battery uses the default lift...
    State s;
    const int now = update(s, restingMv(50) + DEFAULT_OFFSET, true, 0);
    check(now >= 48 && now <= 52, "started up on USB while charging: the default lift is taken off");
    // ...and a full one, charger finished, counts as topping up from now instead of sticking short of 100.
    State f;
    const int first = update(f, 4170, true, 0);
    const int done = update(f, 4170, true, TOPUP_MIN * MIN);
    printf("      started up on USB, full: %d%% then %d%%\n", first, done);
    check(first < 100 && done == 100, "started up on USB with a full battery reaches 100% after topping up");
  }
  {
    // Plugged in full (charger finishes at once): no lift, so it stays at 100.
    State s;
    update(s, 4170, false, 0);
    check(update(s, 4175, true, MIN) == 100, "plugged in already full: stays at 100%");
  }
  {
    // On battery, small noise can't make it bounce up; a power bank (no USB data) jumping 5+ is followed.
    // (Near 50% the curve is steep, 0.5% per mV, so only a few mV: the 32-sample average gives about that.)
    State s;
    uint32_t rng = 3;
    int last = update(s, restingMv(50), false, 0), up = 0;
    for (int i = 0; i < 100; i++) {
      const int p = update(s, restingMv(50) + noise(rng) / 5, false, i * MIN);
      if (p > last) up++;
      last = p;
    }
    check(up == 0, "on battery the % doesn't bounce up with noise");
    check(update(s, restingMv(70), false, 200 * MIN) == 70, "on battery a jump of 5 or more is followed");
  }
  {
    // Low and empty: only on battery, and never when no battery is fitted (the pin reads near 0 V).
    check(level(3900, false) == Level::OK, "3.90 V: OK");
    check(level(3451, false) == Level::OK, "3.451 V: OK");
    check(level(3450, false) == Level::LOW_, "3.45 V: low");
    check(level(3351, false) == Level::LOW_, "3.351 V: low");
    check(level(3350, false) == Level::EMPTY, "3.35 V: empty");
    check(level(3000, false) == Level::EMPTY, "3.00 V: empty");
    check(level(3300, true) == Level::OK, "charging: never low");
    check(level(100, false) == Level::OK, "no battery fitted: never low");
  }
  printf(failures ? "\n%d FAILED\n" : "\nall passed\n", failures);
  return failures != 0;
}
