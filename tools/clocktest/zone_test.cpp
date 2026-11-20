// Checks the device's time zones (src/core/zone_logic.h) on a computer: every Settings preset, and the plain rule the
// C library gets for it (tz::format), against this computer's time zone database; reading TZ rules; the names.
//   c++ -std=c++11 -O2 -Wall -I src/core tools/clocktest/zone_test.cpp -o /tmp/zone_test && /tmp/zone_test
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include "zone_logic.h"

using tz::Secs;
static int failures;

static void check(bool ok, const char *what) {
  printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
  failures += !ok;
}

// The C library's offset (seconds east of UTC) at a moment under a TZ value: a database name or a rule.
static Secs libcOffset(const char *zone, Secs utc) {
  setenv("TZ", zone, 1);
  tzset();
  const time_t t = (time_t)utc;
  struct tm l;
  localtime_r(&t, &l);
  const Secs wall = tz::daysFromCivil(l.tm_year + 1900, l.tm_mon + 1, l.tm_mday) * 86400 + l.tm_hour * 3600 +
                    l.tm_min * 60 + l.tm_sec;
  return wall - utc;
}

static Secs at(int y, int m, int d, int h, int mi) { return tz::daysFromCivil(y, m, d) * 86400 + h * 3600 + mi * 60; }

// Every 6 hours from 2026 to 2030, and every minute of any 6 hours in which the clocks change. 0 = all the same.
static int compare(const tz::Zone &z, const char *libcZone, Secs &firstWrong) {
  int wrong = 0;
  for (Secs t = at(2026, 1, 1, 0, 0); t < at(2031, 1, 1, 0, 0); t += 6 * 3600) {
    if (z.offsetAt(t) != libcOffset(libcZone, t) && !wrong++) firstWrong = t;
    if (z.offsetAt(t) != z.offsetAt(t + 6 * 3600) || libcOffset(libcZone, t) != libcOffset(libcZone, t + 6 * 3600))
      for (Secs u = t; u <= t + 6 * 3600; u += 60)
        if (z.offsetAt(u) != libcOffset(libcZone, u) && !wrong++) firstWrong = u;
  }
  return wrong;
}

int main() {
  // Every preset: our sums, and the plain rule the device gives its C library, against the database
  char what[160];
  for (int i = 0; i < tz::PRESET_COUNT; i++) {
    const tz::Preset &p = tz::PRESETS[i];
    tz::Zone z;
    const bool read = tz::parse(p.rule, z);
    Secs first = 0;
    const int wrong = read ? compare(z, p.iana, first) : -1;
    snprintf(what, sizeof what, "%s (%s) matches %s, 2026-2030", p.name, p.rule, p.iana);
    check(read && !wrong, what);
    if (wrong) printf("        %d wrong, first at %lld\n", wrong, first);
    char plain[80];
    tz::format(z, plain, sizeof plain);
    const int wrongLibc = compare(z, plain, first);
    snprintf(what, sizeof what, "  as the C library gets it: %s", plain);
    check(!wrongLibc, what);
    tz::Zone again;
    check(tz::parse(plain, again) && again.sameAs(z), "  and it reads back the same");
  }
  check(!strcmp(tz::PRESETS[tz::LONDON].name, "London") && !strcmp(tz::DEFAULT_RULE, "GMT0BST,M3.5.0/1,M10.5.0"),
        "London is the default, as before there was a setting");

  // The issue this came from (#29): Central European time, a Friday evening in November
  tz::Zone paris;
  tz::parse("CET-1CEST,M3.5.0,M10.5.0/3", paris);
  const Secs friday = at(2026, 11, 13, 17, 57);
  check(paris.local(friday) == at(2026, 11, 13, 18, 57), "17:57 UTC on 13 November reads 18:57 in Paris (GMT+1)");
  check(tz::zoneOf(tz::DEFAULT_RULE).local(friday) == friday, "and 17:57 in London");
  check(paris.local(at(2026, 7, 1, 12, 0)) == at(2026, 7, 1, 14, 0), "summer in Paris: GMT+2");

  // Wall clock to UTC across the changes (alarms ring by this)
  check(paris.toUtc(at(2026, 3, 29, 2, 30)) == at(2026, 3, 29, 1, 30),
        "2:30 doesn't happen in Paris on 29 March: 3:30");
  check(paris.toUtc(at(2026, 10, 25, 2, 30)) == at(2026, 10, 25, 0, 30), "2:30 happens twice on 25 October: the first");
  check(paris.toUtc(at(2026, 11, 13, 7, 0)) == at(2026, 11, 13, 6, 0), "7:00 in Paris in November is 06:00 UTC");
  tz::Zone sydney;
  tz::parse("AEST-10AEDT,M10.1.0,M4.1.0/3", sydney);
  check(sydney.local(at(2026, 1, 1, 0, 0)) == at(2026, 1, 1, 11, 0) &&
          sydney.local(at(2026, 7, 1, 0, 0)) == at(2026, 7, 1, 10, 0),
        "Sydney: summer over new year");

  // Reading rules
  struct Case {
    const char *rule;
    bool ok;
    Secs std, dst;
  };
  const Case CASES[] = {
    {"UTC0", true, 0, 0},
    {"EST5", true, -5 * 3600, 0},
    {"IST-5:30", true, 5 * 3600 + 1800, 0},
    {"<+0545>-5:45", true, 5 * 3600 + 2700, 0},
    {"<-03>3", true, -3 * 3600, 0},
    {"LHST-10:30LHDT-11,M10.1.0,M4.1.0", true, 37800, 39600},  // Lord Howe: half an hour of summer time
    {"STD-1DST,M3.5.0/2,M10.5.0/3", true, 3600, 7200},
    {"STD+1DST+0,M3.5.0,M10.5.0", true, -3600, 0},
    {"EET-2EEST,M4.5.5/0,M10.5.4/24", true, 7200, 10800},
    {"", false, 0, 0},
    {"GMT", false, 0, 0},            // no offset
    {"AB0", false, 0, 0},            // a name needs 3 letters
    {"CET-1CEST", false, 0, 0},      // summer time, but no dates
    {"CET-1CEST,J60,J300", false, 0, 0},
    {"CET-1CEST,M3.5.0", false, 0, 0},
    {"CET-1CEST,M13.5.0,M10.5.0", false, 0, 0},
    {"CET-1CEST,M3.6.0,M10.5.0", false, 0, 0},
    {"CET-1CEST,M3.5.7,M10.5.0", false, 0, 0},
    {"<-02>2<-01>,M3.5.0/-1,M10.5.0/0", false, 0, 0},  // a negative time: not every C library reads it
    {"CET-1 ", false, 0, 0},
    {"CET-25", false, 0, 0},
    {"<+03-3", false, 0, 0},
    {"Europe/Paris", false, 0, 0},
    {"CET-1CEST,M3.5.0,M10.5.0/3,M11.1.0", false, 0, 0},
    {"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA0", false, 0, 0},  // too long
  };
  for (const Case &c : CASES) {
    tz::Zone z;
    const bool ok = tz::parse(c.rule, z);
    snprintf(what, sizeof what, "\"%s\" %s", c.rule, c.ok ? "reads" : "is refused");
    check(ok == c.ok && (!ok || (z.std == c.std && (!z.hasDst || z.dst == c.dst))), what);
  }

  // Names
  check(tz::presetOf("CET-1CEST,M3.5.0,M10.5.0/3", "Madrid") == 17, "a preset by its name: Madrid");
  check(tz::presetOf("CET-1CEST,M3.5.0,M10.5.0/3", nullptr) == 15, "the same clock, no name: the first, Paris");
  check(tz::presetOf("STD-1DST,M3.5.0/2,M10.5.0/3", "Brussels") == 15, "same clock, other names: Paris's");
  check(tz::presetOf("STD-3:30", "Tehran") == -1, "no preset for Tehran");
  char name[16];
  tz::offsetName(tz::zoneOf("STD-3:30"), name, sizeof name);
  check(!strcmp(name, "UTC+3:30"), "UTC+3:30");
  tz::offsetName(tz::zoneOf("STD5"), name, sizeof name);
  check(!strcmp(name, "UTC-5"), "UTC-5");
  tz::offsetName(tz::zoneOf("GMT0"), name, sizeof name);
  check(!strcmp(name, "UTC"), "UTC");
  check(tz::zoneOf("nonsense").sameAs(tz::zoneOf(tz::DEFAULT_RULE)), "a damaged rule reads as London");

  if (failures) printf("%d failed\n", failures);
  return failures ? 1 : 0;
}
