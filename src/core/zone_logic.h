#pragma once
// The device's time zone, as a POSIX TZ rule ("CET-1CEST,M3.5.0,M10.5.0/3"): the same text the C library's TZ takes,
// so one saved string sets the system clock (clock.cpp) and the sums that can't swap TZ while other tasks read it
// (Clock's alarms, the calendar link's reader). Plus the zones Settings offers. No Arduino in here, so
// tools/clocktest/zone_test.cpp checks it on a computer against its own time zone database.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

namespace tz {

typedef long long Secs;  // seconds since 1970-01-01

inline Secs floorDiv(Secs a, Secs b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }

// A day as a count from 1970-01-01 (Howard Hinnant's days_from_civil and civil_from_days).
inline Secs daysFromCivil(Secs y, int m, int d) {
  y -= m <= 2;
  const Secs era = (y >= 0 ? y : y - 399) / 400;
  const Secs yoe = y - era * 400;
  const Secs doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const Secs doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}

inline void civilFromDays(Secs z, int &y, int &m, int &d) {
  z += 719468;
  const Secs era = (z >= 0 ? z : z - 146096) / 146097;
  const Secs doe = z - era * 146097;
  const Secs yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const Secs doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const Secs mp = (5 * doy + 2) / 153;
  d = (int)(doy - (153 * mp + 2) / 5 + 1);
  m = (int)(mp < 10 ? mp + 3 : mp - 9);
  y = (int)(yoe + era * 400 + (m <= 2));
}

inline int weekday(Secs day) { return (int)(((day % 7) + 11) % 7); }  // 0 = Sunday (1970-01-01 was a Thursday)

inline int daysInMonth(int y, int m) {
  static const int DAYS[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  return DAYS[m - 1] + (m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0));
}

// The nth `wday` of a month (nth < 0 counts from the end), or a day outside the month if there isn't one.
inline Secs nthWeekday(int y, int m, int nth, int wday) {
  if (nth > 0) {
    const Secs first = daysFromCivil(y, m, 1);
    return first + (wday - weekday(first) + 7) % 7 + (nth - 1) * 7;
  }
  const Secs last = daysFromCivil(y, m, daysInMonth(y, m));
  return last - (weekday(last) - wday + 7) % 7 + (nth + 1) * 7;
}

// A time zone's yearly rule (from a TZ string, or a calendar file's VTIMEZONE). Offsets in seconds east of UTC.
struct Change {
  int month, nth, wday;
  Secs at;  // local time of day the change happens, before it
};
struct Zone {
  Secs std = 0, dst = 0;
  bool hasDst = false;
  Change toDst = Change(), toStd = Change();
  bool inDst(Secs utc) const {
    int y, m, d;
    civilFromDays(floorDiv(utc, 86400), y, m, d);
    const Secs start = nthWeekday(y, toDst.month, toDst.nth, toDst.wday) * 86400 + toDst.at - std;
    const Secs end = nthWeekday(y, toStd.month, toStd.nth, toStd.wday) * 86400 + toStd.at - dst;
    return start < end ? utc >= start && utc < end : utc >= start || utc < end;  // the south: summer over new year
  }
  Secs offsetAt(Secs utc) const { return hasDst && inDst(utc) ? dst : std; }
  // The UTC time of a wall-clock time here. One skipped when the clocks go forward counts as an hour later; one that
  // happens twice when they go back is the first.
  Secs toUtc(Secs local) const {
    if (hasDst && inDst(local - dst)) return local - dst;
    return local - std;
  }
  Secs local(Secs utc) const { return utc + offsetAt(utc); }  // wall clock, in seconds since 1970 there
  bool sameAs(const Zone &o) const {  // the same clock, whatever the zones are called
    if (std != o.std || hasDst != o.hasDst) return false;
    if (!hasDst) return true;
    return dst == o.dst && toDst.month == o.toDst.month && toDst.nth == o.toDst.nth && toDst.wday == o.toDst.wday &&
           toDst.at == o.toDst.at && toStd.month == o.toStd.month && toStd.nth == o.toStd.nth &&
           toStd.wday == o.toStd.wday && toStd.at == o.toStd.at;
  }
};

namespace detail {
// "GMT", "CEST" or "<+0330>": a zone's name, which only labels it.
inline bool name(const char *&p) {
  if (*p == '<') {
    const char *end = strchr(p, '>');
    if (!end || end - p < 4) return false;  // 3 or more characters inside
    p = end + 1;
    return true;
  }
  const char *start = p;
  while ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z')) p++;
  return p - start >= 3;
}

// "[+-]hh[:mm[:ss]]" as seconds. Hours up to 167, as POSIX allows for rule times.
inline bool hms(const char *&p, Secs &out, int maxHours) {
  int sign = 1;
  if (*p == '+' || *p == '-') sign = *p++ == '-' ? -1 : 1;
  if (*p < '0' || *p > '9') return false;
  char *end;
  const long h = strtol(p, &end, 10);
  if (h > maxHours || end - p > 3) return false;
  p = end;
  Secs s = h * 3600;
  for (int part = 0; part < 2 && *p == ':'; part++) {
    p++;
    if (p[0] < '0' || p[0] > '9' || p[1] < '0' || p[1] > '9') return false;
    const int v = (p[0] - '0') * 10 + (p[1] - '0');
    if (v > 59) return false;
    s += part == 0 ? v * 60 : v;
    p += 2;
  }
  out = sign * s;
  return true;
}

// ",Mm.w.d[/time]": the wth (5 = last) weekday d (0 = Sunday) of month m, at a local time (2:00 if none). The time
// can't be negative here (the time zone database writes one for Greenland only): not every C library reads it.
inline bool change(const char *&p, Change &c) {
  if (p[0] != ',' || p[1] != 'M') return false;
  p += 2;
  char *end;
  const long m = strtol(p, &end, 10);
  if (end == p || *end != '.' || m < 1 || m > 12) return false;
  p = end + 1;
  const long w = strtol(p, &end, 10);
  if (end == p || *end != '.' || w < 1 || w > 5) return false;
  p = end + 1;
  const long d = strtol(p, &end, 10);
  if (end == p || d < 0 || d > 6) return false;
  p = end;
  c.month = (int)m;
  c.nth = w == 5 ? -1 : (int)w;
  c.wday = (int)d;
  c.at = 2 * 3600;
  if (*p == '/') {
    p++;
    if (*p == '-' || *p == '+' || !hms(p, c.at, 167)) return false;
  }
  return true;
}

inline int put(char *out, size_t len, Secs s) {  // "1", "-5:30", "5:45:30"
  const Secs a = s < 0 ? -s : s;
  const char *sign = s < 0 ? "-" : "";
  if (a % 60) return snprintf(out, len, "%s%d:%02d:%02d", sign, (int)(a / 3600), (int)(a / 60 % 60), (int)(a % 60));
  if (a % 3600) return snprintf(out, len, "%s%d:%02d", sign, (int)(a / 3600), (int)(a / 60 % 60));
  return snprintf(out, len, "%s%d", sign, (int)(a / 3600));
}
}  // namespace detail

// A TZ string to a Zone. False for anything else, and for the forms this device doesn't follow: a summer time with
// no rule for when (the C library would guess US dates) and "Jn"/"n" day-of-year rules.
inline bool parse(const char *s, Zone &z) {
  if (!s || strlen(s) > 63) return false;
  const char *p = s;
  Zone out;
  Secs west;
  if (!detail::name(p) || !detail::hms(p, west, 24)) return false;
  out.std = -west;
  if (*p) {
    if (!detail::name(p)) return false;
    out.dst = out.std + 3600;  // an hour ahead unless it says
    if (*p && *p != ',') {
      if (!detail::hms(p, west, 24)) return false;
      out.dst = -west;
    }
    if (!detail::change(p, out.toDst) || !detail::change(p, out.toStd)) return false;
    out.hasDst = true;
  }
  if (*p) return false;
  z = out;
  return true;
}

// A Zone as the plainest TZ string ("STD-1DST,M3.5.0/2,M10.5.0/3"), for the C library: every name in letters,
// every field written out, so it never meets a form it might not know ("<+03>", a summer offset left out).
inline void format(const Zone &z, char *out, size_t len) {
  if (!len) return;
  char *p = out, *end = out + len;
  auto room = [&]() { return (size_t)(p < end ? end - p : 0); };
  auto add = [&](int n) { p += n < 0 ? 0 : n; if (p > end) p = end; };
  add(snprintf(p, room(), "STD"));
  add(detail::put(p, room(), -z.std));
  if (z.hasDst) {
    add(snprintf(p, room(), "DST"));
    add(detail::put(p, room(), -z.dst));
    for (const Change *c : {&z.toDst, &z.toStd}) {
      add(snprintf(p, room(), ",M%d.%d.%d/", c->month, c->nth < 0 ? 5 : c->nth, c->wday));
      add(detail::put(p, room(), c->at));
    }
  }
  if (p == end) out[len - 1] = 0;
}

// The zones Settings > Time zone goes through, west to east. Each rule is the one the time zone database itself
// gives for that city (checked by tools/clocktest/zone_test.cpp); a computer sets any other zone over USB.
struct Preset {
  const char *name, *rule, *iana;  // iana: the database's name for it, for the test
};
static const Preset PRESETS[] = {
  {"Honolulu", "HST10", "Pacific/Honolulu"},
  {"Anchorage", "AKST9AKDT,M3.2.0,M11.1.0", "America/Anchorage"},
  {"Los Angeles", "PST8PDT,M3.2.0,M11.1.0", "America/Los_Angeles"},
  {"Phoenix", "MST7", "America/Phoenix"},
  {"Denver", "MST7MDT,M3.2.0,M11.1.0", "America/Denver"},
  {"Chicago", "CST6CDT,M3.2.0,M11.1.0", "America/Chicago"},
  {"Mexico City", "CST6", "America/Mexico_City"},
  {"New York", "EST5EDT,M3.2.0,M11.1.0", "America/New_York"},
  {"Halifax", "AST4ADT,M3.2.0,M11.1.0", "America/Halifax"},
  {"Sao Paulo", "<-03>3", "America/Sao_Paulo"},
  {"Buenos Aires", "<-03>3", "America/Argentina/Buenos_Aires"},
  {"Reykjavik", "GMT0", "Atlantic/Reykjavik"},
  {"London", "GMT0BST,M3.5.0/1,M10.5.0", "Europe/London"},
  {"Dublin", "GMT0IST,M3.5.0/1,M10.5.0", "Europe/Dublin"},
  {"Lisbon", "WET0WEST,M3.5.0/1,M10.5.0", "Europe/Lisbon"},
  {"Paris", "CET-1CEST,M3.5.0,M10.5.0/3", "Europe/Paris"},
  {"Berlin", "CET-1CEST,M3.5.0,M10.5.0/3", "Europe/Berlin"},
  {"Madrid", "CET-1CEST,M3.5.0,M10.5.0/3", "Europe/Madrid"},
  {"Rome", "CET-1CEST,M3.5.0,M10.5.0/3", "Europe/Rome"},
  {"Amsterdam", "CET-1CEST,M3.5.0,M10.5.0/3", "Europe/Amsterdam"},
  {"Stockholm", "CET-1CEST,M3.5.0,M10.5.0/3", "Europe/Stockholm"},
  {"Warsaw", "CET-1CEST,M3.5.0,M10.5.0/3", "Europe/Warsaw"},
  {"Lagos", "WAT-1", "Africa/Lagos"},
  {"Athens", "EET-2EEST,M3.5.0/3,M10.5.0/4", "Europe/Athens"},
  {"Helsinki", "EET-2EEST,M3.5.0/3,M10.5.0/4", "Europe/Helsinki"},
  {"Kyiv", "EET-2EEST,M3.5.0/3,M10.5.0/4", "Europe/Kyiv"},
  {"Cairo", "EET-2EEST,M4.5.5/0,M10.5.4/24", "Africa/Cairo"},
  {"Johannesburg", "SAST-2", "Africa/Johannesburg"},
  {"Istanbul", "<+03>-3", "Europe/Istanbul"},
  {"Moscow", "MSK-3", "Europe/Moscow"},
  {"Dubai", "<+04>-4", "Asia/Dubai"},
  {"Karachi", "PKT-5", "Asia/Karachi"},
  {"Delhi", "IST-5:30", "Asia/Kolkata"},
  {"Kathmandu", "<+0545>-5:45", "Asia/Kathmandu"},
  {"Bangkok", "<+07>-7", "Asia/Bangkok"},
  {"Singapore", "<+08>-8", "Asia/Singapore"},
  {"Hong Kong", "HKT-8", "Asia/Hong_Kong"},
  {"Beijing", "CST-8", "Asia/Shanghai"},
  {"Perth", "AWST-8", "Australia/Perth"},
  {"Tokyo", "JST-9", "Asia/Tokyo"},
  {"Seoul", "KST-9", "Asia/Seoul"},
  {"Adelaide", "ACST-9:30ACDT,M10.1.0,M4.1.0/3", "Australia/Adelaide"},
  {"Brisbane", "AEST-10", "Australia/Brisbane"},
  {"Sydney", "AEST-10AEDT,M10.1.0,M4.1.0/3", "Australia/Sydney"},
  {"Auckland", "NZST-12NZDT,M9.5.0,M4.1.0/3", "Pacific/Auckland"},
};
static const int PRESET_COUNT = sizeof(PRESETS) / sizeof(PRESETS[0]);
static const int LONDON = 12;  // the default: the device was London-only before it had a setting
static const char *const DEFAULT_RULE = PRESETS[LONDON].rule;

inline Zone zoneOf(const char *rule) {  // London if the rule can't be read
  Zone z;
  if (!parse(rule, z)) parse(DEFAULT_RULE, z);
  return z;
}

// The preset a saved zone is (by name, else the first with the same clock), or -1.
inline int presetOf(const char *rule, const char *name) {
  for (int i = 0; i < PRESET_COUNT; i++)
    if (name && !strcmp(name, PRESETS[i].name) && !strcmp(rule, PRESETS[i].rule)) return i;
  Zone z;
  if (!parse(rule, z)) return -1;
  for (int i = 0; i < PRESET_COUNT; i++)
    if (zoneOf(PRESETS[i].rule).sameAs(z)) return i;
  return -1;
}

// "UTC+1", "UTC-3:30", "UTC": a zone with no name, by its standard offset.
inline void offsetName(const Zone &z, char *out, size_t len) {
  const Secs a = z.std < 0 ? -z.std : z.std;
  if (!z.std) snprintf(out, len, "UTC");
  else if (a % 3600) snprintf(out, len, "UTC%c%d:%02d", z.std < 0 ? '-' : '+', (int)(a / 3600), (int)(a / 60 % 60));
  else snprintf(out, len, "UTC%c%d", z.std < 0 ? '-' : '+', (int)(a / 3600));
}

}  // namespace tz
