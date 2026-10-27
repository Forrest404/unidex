// Checks Clock's sums on a computer (no board needed): the cities against this computer's time zone database,
// alarms, presets and packing.
//   c++ -std=c++17 -O2 -I src/apps/clock tools/clocktest/clock_test.cpp -o /tmp/clock_test && /tmp/clock_test
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include "clock_logic.h"

using namespace clocks;
static int failures;

static void check(bool ok, const char *what) {
  printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
  failures += !ok;
}

static const char *const ZONES[CITY_COUNT] = {
  "Pacific/Honolulu", "America/Los_Angeles", "America/Denver",     "America/Chicago",  "America/Mexico_City",
  "America/New_York", "America/Sao_Paulo",   "Europe/London",      "Europe/Paris",     "Europe/Berlin",
  "Africa/Lagos",     "Europe/Athens",       "Africa/Johannesburg", "Europe/Istanbul", "Europe/Moscow",
  "Asia/Dubai",       "Asia/Kolkata",        "Asia/Bangkok",       "Asia/Singapore",   "Asia/Hong_Kong",
  "Asia/Shanghai",    "Asia/Tokyo",          "Australia/Sydney",   "Pacific/Auckland",
};

// The computer's answer for a city at a moment, as minutes since 1970 there.
static long long systemLocal(const char *zone, long long utc) {
  setenv("TZ", zone, 1);
  tzset();
  const time_t t = (time_t)utc;
  struct tm l;
  localtime_r(&t, &l);
  return (long long)daysFromCivil(l.tm_year + 1900, l.tm_mon + 1, l.tm_mday) * 1440 + l.tm_hour * 60 + l.tm_min;
}

static long long at(int y, int m, int d, int hour, int minute) { return utcOf(y, m, d, hour, 0) + minute * 60; }  // UTC

static long long ours(const City &c, long long utc) {
  const Local l = localIn(c, utc);
  return (long long)l.days * 1440 + l.hour * 60 + l.minute;
}

int main() {
  // Every city, every 6 hours from 2026 to 2030, and a minute either side of each change of the clocks there
  const long long from = utcOf(2026, 1, 1, 0, 0), to = utcOf(2031, 1, 1, 0, 0);
  for (int i = 0; i < CITY_COUNT; i++) {
    int wrong = 0;
    long long firstWrong = 0;
    for (long long t = from; t < to; t += 6 * 3600) {
      for (long long u : {t, t + 1800}) {
        if (ours(CITIES[i], u) != systemLocal(ZONES[i], u) && !wrong++) firstWrong = u;
      }
      // a change of the clocks in the next 6 hours: check either side of it to the minute
      if (offsetAt(CITIES[i], t) != offsetAt(CITIES[i], t + 6 * 3600))
        for (long long u = t; u <= t + 6 * 3600; u += 60)
          if (ours(CITIES[i], u) != systemLocal(ZONES[i], u) && !wrong++) firstWrong = u;
    }
    char what[96];
    snprintf(what, sizeof what, "%s matches %s, 2026-2030", CITIES[i].name, ZONES[i]);
    check(!wrong, what);
    if (wrong) printf("        %d wrong, first at %lld\n", wrong, firstWrong);
  }

  // Dates
  int y, m, d;
  civilFromDays(daysFromCivil(2028, 2, 29), y, m, d);
  check(y == 2028 && m == 2 && d == 29, "a leap day round trip");
  check(sunday(2026, 3, 0) == 29 && sunday(2026, 10, 0) == 25, "last Sundays of March and October 2026");
  check(sunday(2026, 3, 2) == 8 && sunday(2026, 11, 1) == 1, "US changes in 2026: 8 March, 1 November");
  check(weekday(daysFromCivil(2026, 10, 27)) == 2, "27 October 2026 is a Tuesday");

  // Presets
  check(PRESET_COUNT == 13 && PRESETS[DEFAULT_PRESET] == 1500, "13 presets, 25 min by default");
  check(presetName(0) == "1 min" && presetName(10) == "1 h" && presetName(11) == "1.5 h", "preset names");
  check(duration(1500) == "25:00" && duration(5400) == "1:30:00" && duration(5) == "0:05" && duration(-3) == "0:00",
        "durations");

  // Packing
  const Alarm a = {true, 6, 45, WEEKENDS};
  const Alarm b = unpack(pack(a));
  check(b.on && b.hour == 6 && b.minute == 45 && b.repeat == WEEKENDS, "an alarm packs and unpacks");
  const Alarm z = unpack(0);
  check(!z.on && z.hour == 7 && z.minute == 0 && z.repeat == WEEKDAYS, "nothing saved: 7:00 weekdays, off");
  check(!unpack(pack({true, 23, 59, ONCE})).repeat && unpack(pack({true, 23, 59, ONCE})).hour == 23, "23:59 once");

  // Next ring (London)
  const long long tue0900 = utcOf(2026, 10, 27, 9, 0);  // Tuesday 27 October 2026, 09:00 GMT
  check(nextRing({true, 7, 0, DAILY}, tue0900) == utcOf(2026, 10, 28, 7, 0), "every day at 7:00: tomorrow");
  check(nextRing({true, 9, 30, DAILY}, tue0900) == at(2026, 10, 27, 9, 30), "every day at 9:30: today");
  check(nextRing({true, 9, 0, DAILY}, tue0900) == utcOf(2026, 10, 28, 9, 0), "this very minute: tomorrow's");
  check(nextRing({true, 8, 0, WEEKENDS}, tue0900) == utcOf(2026, 10, 31, 8, 0), "weekends: Saturday");
  const long long fri2300 = utcOf(2026, 10, 30, 23, 0);
  check(nextRing({true, 7, 0, WEEKDAYS}, fri2300) == utcOf(2026, 11, 2, 7, 0), "weekdays from Friday night: Monday");
  check(nextRing({true, 7, 0, ONCE}, fri2300) == utcOf(2026, 10, 31, 7, 0), "once: the next 7:00, any day");
  // Summer time in London: 7:00 is still 7:00 on the clock (06:00 UTC)
  check(nextRing({true, 7, 0, DAILY}, utcOf(2026, 6, 1, 12, 0)) == utcOf(2026, 6, 2, 6, 0), "7:00 BST is 06:00 UTC");
  // Across the change: Saturday 28 March 2026 GMT, then Sunday 29 March BST
  check(nextRing({true, 7, 0, DAILY}, utcOf(2026, 3, 28, 12, 0)) == utcOf(2026, 3, 29, 6, 0),
        "the morning the clocks go forward: 7:00 BST");
  check(nextRing({true, 1, 30, DAILY}, utcOf(2026, 3, 28, 12, 0)) == at(2026, 3, 29, 1, 30),
        "1:30 doesn't happen that night: it rings at 2:30 BST");
  check(nextRing({true, 1, 30, DAILY}, utcOf(2026, 10, 24, 12, 0)) == at(2026, 10, 25, 0, 30),
        "1:30 happens twice when they go back: the first");

  // Words
  check(whenWords(utcOf(2026, 10, 27, 18, 0), tue0900) == "today" &&
            whenWords(utcOf(2026, 10, 28, 7, 0), tue0900) == "tomorrow" &&
            whenWords(utcOf(2026, 10, 30, 7, 0), tue0900) == "Fri",
        "today, tomorrow, Fri");
  check(difference(CITIES[21], tue0900) == "+9 h" && difference(CITIES[5], tue0900) == "-4 h" &&
            difference(CITIES[16], tue0900) == "+5:30" && difference(CITIES[7], tue0900) == "same",
        "differences from London: Tokyo, New York (still on summer time), Delhi, London");
  check(difference(CITIES[5], utcOf(2026, 11, 10, 12, 0)) == "-5 h", "New York is 5 h behind once its clocks go back too");
  check(hhmm(7, 5) == "7:05" && hhmm(18, 30) == "18:30", "times");

  printf(failures ? "\n%d check(s) failed\n" : "\nall checks passed\n", failures);
  return failures ? 1 : 0;
}
