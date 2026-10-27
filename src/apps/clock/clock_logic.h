#pragma once
// Clock's sums: the timer presets, when each alarm rings next, and the time in other cities. No Arduino code, so
// it can be checked on a computer (tools/clocktest). Cities are worked out here from their own rules rather than
// by swapping the process's TZ, which the Notes task reads at the same time.
#include <stdint.h>
#include <stdio.h>
#include <string>

namespace clocks {

// --- the timer ---

static const int PRESETS[] = {60, 120, 180, 300, 600, 900, 1200, 1500, 1800, 2700, 3600, 5400, 7200};  // seconds
static const int PRESET_COUNT = sizeof(PRESETS) / sizeof(PRESETS[0]);
static const int DEFAULT_PRESET = 7;  // 25 min
static const int TIMER_EXTRA = 60;    // A while it rings: a minute more
static const int SNOOZE = 9 * 60;     // A while an alarm rings
static const int RING_EVERY = 3, RING_FOR = 120;  // a chime every 3 s, for up to 2 minutes

inline std::string duration(long secs) {  // "25:00", "1:30:00", "0:05"
  if (secs < 0) secs = 0;
  char out[16];
  if (secs >= 3600) snprintf(out, sizeof out, "%ld:%02ld:%02ld", secs / 3600, secs / 60 % 60, secs % 60);
  else snprintf(out, sizeof out, "%ld:%02ld", secs / 60, secs % 60);
  return out;
}

inline std::string presetName(int i) {  // "25 min", "1 h", "1.5 h"
  const int s = PRESETS[i];
  char out[12];
  if (s < 3600) snprintf(out, sizeof out, "%d min", s / 60);
  else if (s % 3600) snprintf(out, sizeof out, "%d.5 h", s / 3600);
  else snprintf(out, sizeof out, "%d h", s / 3600);
  return out;
}

// --- dates ---

inline long daysFromCivil(int y, int m, int d) {  // days since 1970-01-01
  y -= m <= 2;
  const long era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned)(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (long)doe - 719468;
}

inline void civilFromDays(long z, int &y, int &m, int &d) {
  z += 719468;
  const long era = (z >= 0 ? z : z - 146096) / 146097;
  const unsigned doe = (unsigned)(z - era * 146097);
  const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const unsigned mp = (5 * doy + 2) / 153;
  d = (int)(doy - (153 * mp + 2) / 5 + 1);
  m = (int)(mp < 10 ? mp + 3 : mp - 9);
  y = (int)(yoe + era * 400 + (m <= 2));
}

inline int weekday(long days) { return (int)(((days % 7) + 11) % 7); }  // 0 = Sunday (1970-01-01 was a Thursday)

inline long floorDiv(long long a, long b) { return (long)(a >= 0 ? a / b : -((-a + b - 1) / b)); }

// The day of the month of the nth Sunday (n = 1..4), or the last one (n = 0).
inline int sunday(int y, int m, int n) {
  if (n > 0) {
    const int first = weekday(daysFromCivil(y, m, 1));
    return 1 + (7 - first) % 7 + 7 * (n - 1);
  }
  int ny = y, nm = m + 1;
  if (nm > 12) nm = 1, ny++;
  const long last = daysFromCivil(ny, nm, 1) - 1;
  return (int)(last - daysFromCivil(y, m, 1)) + 1 - weekday(last);
}

// --- cities ---

enum Dst : uint8_t { NO_DST, EU, US, AU, NZ };

struct City {
  const char *name;
  int16_t offset;  // minutes east of UTC, standard time
  Dst dst;
};

static const City CITIES[] = {
  {"Honolulu", -600, NO_DST}, {"Los Angeles", -480, US},  {"Denver", -420, US},      {"Chicago", -360, US},
  {"Mexico City", -360, NO_DST}, {"New York", -300, US},  {"Sao Paulo", -180, NO_DST}, {"London", 0, EU},
  {"Paris", 60, EU},          {"Berlin", 60, EU},         {"Lagos", 60, NO_DST},     {"Athens", 120, EU},
  {"Johannesburg", 120, NO_DST}, {"Istanbul", 180, NO_DST}, {"Moscow", 180, NO_DST}, {"Dubai", 240, NO_DST},
  {"Delhi", 330, NO_DST},     {"Bangkok", 420, NO_DST},   {"Singapore", 480, NO_DST}, {"Hong Kong", 480, NO_DST},
  {"Beijing", 480, NO_DST},   {"Tokyo", 540, NO_DST},     {"Sydney", 600, AU},       {"Auckland", 720, NZ},
};
static const int CITY_COUNT = sizeof(CITIES) / sizeof(CITIES[0]);
static const int HOME = 7;  // London: the device's own time (clock.cpp's TZ)

// UTC seconds of a local wall-clock time at a given offset.
inline long long utcOf(int y, int m, int d, int hour, int offsetMin) {
  return (long long)daysFromCivil(y, m, d) * 86400 + hour * 3600 - offsetMin * 60;
}

// Minutes east of UTC in that city at that moment, summer time included.
inline int offsetAt(const City &c, long long utc) {
  int y, m, d;
  civilFromDays(floorDiv(utc + c.offset * 60, 86400), y, m, d);
  bool summer = false;
  switch (c.dst) {
    case EU:  // last Sunday of March to last Sunday of October, at 01:00 UTC
      summer = utc >= utcOf(y, 3, sunday(y, 3, 0), 1, 0) && utc < utcOf(y, 10, sunday(y, 10, 0), 1, 0);
      break;
    case US:  // second Sunday of March 02:00 standard to first Sunday of November 02:00 summer time
      summer = utc >= utcOf(y, 3, sunday(y, 3, 2), 2, c.offset) && utc < utcOf(y, 11, sunday(y, 11, 1), 2, c.offset + 60);
      break;
    case AU:  // first Sunday of October 02:00 standard to first Sunday of April 03:00 summer time
      summer = utc < utcOf(y, 4, sunday(y, 4, 1), 3, c.offset + 60) || utc >= utcOf(y, 10, sunday(y, 10, 1), 2, c.offset);
      break;
    case NZ:  // last Sunday of September 02:00 standard to first Sunday of April 03:00 summer time
      summer = utc < utcOf(y, 4, sunday(y, 4, 1), 3, c.offset + 60) || utc >= utcOf(y, 9, sunday(y, 9, 0), 2, c.offset);
      break;
    case NO_DST:
      break;
  }
  return c.offset + (summer ? 60 : 0);
}

struct Local {
  int year, month, day, hour, minute, second, wday;
  long days;  // since 1970-01-01, there
};

inline Local localIn(const City &c, long long utc) {
  const long long t = utc + offsetAt(c, utc) * 60LL;
  Local l;
  l.days = floorDiv(t, 86400);
  const long s = (long)(t - (long long)l.days * 86400);
  civilFromDays(l.days, l.year, l.month, l.day);
  l.hour = (int)(s / 3600);
  l.minute = (int)(s / 60 % 60);
  l.second = (int)(s % 60);
  l.wday = weekday(l.days);
  return l;
}

// The UTC time of a wall-clock time in that city. A time skipped when the clocks go forward counts as an hour later;
// one that happens twice when they go back is the first.
inline long long utcFromLocal(const City &c, long days, int minuteOfDay) {
  const long long wall = (long long)days * 86400 + minuteOfDay * 60LL;
  const long long summer = wall - (c.offset + 60) * 60LL, standard = wall - c.offset * 60LL;
  if (c.dst != NO_DST && offsetAt(c, summer) == c.offset + 60) return summer;
  return standard;
}

inline std::string hhmm(int hour, int minute) {  // "7:05", "18:30"
  char out[8];
  snprintf(out, sizeof out, "%d:%02d", hour, minute);
  return out;
}

// "+9 h", "-5 h", "+4:30", "same": from here (home) to there, now.
inline std::string difference(const City &there, long long utc) {
  const int diff = offsetAt(there, utc) - offsetAt(CITIES[HOME], utc);
  if (!diff) return "same";
  const int a = diff < 0 ? -diff : diff;
  char out[12];
  if (a % 60) snprintf(out, sizeof out, "%c%d:%02d", diff < 0 ? '-' : '+', a / 60, a % 60);
  else snprintf(out, sizeof out, "%c%d h", diff < 0 ? '-' : '+', a / 60);
  return out;
}

// --- alarms ---

enum Repeat : uint8_t { ONCE, DAILY, WEEKDAYS, WEEKENDS, REPEATS };
static const char *const REPEAT_NAMES[REPEATS] = {"Once", "Every day", "Weekdays", "Weekends"};
static const int ALARMS = 3;

struct Alarm {
  bool on;
  uint8_t hour, minute;
  Repeat repeat;
};
static const Alarm DEFAULT_ALARM = {false, 7, 0, WEEKDAYS};

// Saved as one number: minute | hour << 6 | repeat << 11 | on << 13. 0 and anything that doesn't fit = the default.
inline int32_t pack(const Alarm &a) { return a.minute | a.hour << 6 | a.repeat << 11 | (a.on ? 1 : 0) << 13 | 1 << 14; }

inline Alarm unpack(int32_t v) {
  if (!(v & 1 << 14)) return DEFAULT_ALARM;
  Alarm a = {(v >> 13 & 1) != 0, (uint8_t)(v >> 6 & 31), (uint8_t)(v & 63), (Repeat)(v >> 11 & 3)};
  if (a.hour > 23 || a.minute > 59) return DEFAULT_ALARM;
  return a;
}

inline bool ringsOn(Repeat r, int wday) {
  if (r == WEEKDAYS) return wday >= 1 && wday <= 5;
  if (r == WEEKENDS) return wday == 0 || wday == 6;
  return true;  // once (the next time it comes round), every day
}

// When an alarm (on or not) rings next after `utc`, in UTC, at its time here (home). -1 if it never would.
inline long long nextRing(const Alarm &a, long long utc) {
  const City &home = CITIES[HOME];
  const long today = localIn(home, utc).days;
  for (long d = today; d <= today + 8; d++) {
    if (!ringsOn(a.repeat, weekday(d))) continue;
    const long long at = utcFromLocal(home, d, a.hour * 60 + a.minute);
    if (at > utc) return at;
  }
  return -1;
}

// "Weekdays 7:00" style words for when it next rings: "today", "tomorrow", or the day.
inline std::string whenWords(long long ring, long long utc) {
  static const char *const DAYS[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  const Local r = localIn(CITIES[HOME], ring), now = localIn(CITIES[HOME], utc);
  if (r.days == now.days) return "today";
  if (r.days == now.days + 1) return "tomorrow";
  return DAYS[r.wday];
}

}  // namespace clocks
