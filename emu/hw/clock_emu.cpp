// The clock is the browser's.
#include "../../src/core/clock.h"
#include <Arduino.h>
void clockBegin() {}
bool clockValid() { return time(nullptr) > 1704067200; }
const char *clockSync() { return "No WiFi in the demo"; }
const char *clockSyncOnline() { return "No WiFi in the demo"; }
bool clockSet(time_t) { return true; }
void clockStatus(char *out, size_t len) { snprintf(out, len, "browser clock"); }
struct tm clockLocal() {
  time_t t = time(nullptr);
  struct tm tm;
  localtime_r(&t, &tm);
  return tm;
}

// The time zone is saved and named as on the device (Settings > Time zone, USB `Z`), but the time shown stays the
// browser's own: Emscripten's localtime() reads the browser, not TZ.
#include "../../src/core/storage.h"
static String zoneRule, zoneName;
static tz::Zone zone;
static void loadZone() {
  static bool loaded;
  if (loaded) return;
  loaded = true;
  zoneRule = storageGetString("tz", tz::DEFAULT_RULE);
  zoneName = storageGetString("tz_name", "");
  if (!tz::parse(zoneRule.c_str(), zone)) zoneRule = tz::DEFAULT_RULE, zoneName = "", tz::parse(zoneRule.c_str(), zone);
  if (!zoneName.length()) {
    const int i = tz::presetOf(zoneRule.c_str(), nullptr);
    char offset[16];
    tz::offsetName(zone, offset, sizeof offset);
    zoneName = i >= 0 ? tz::PRESETS[i].name : offset;
  }
}
const char *clockZoneRule() { return loadZone(), zoneRule.c_str(); }
const char *clockZoneName() { return loadZone(), zoneName.c_str(); }
const tz::Zone &clockHomeZone() { return loadZone(), zone; }
bool clockSetZone(const char *rule, const char *name) {
  tz::Zone check;
  if (!rule || !tz::parse(rule, check) || !name || strlen(name) > 23) return false;
  for (const char *c = name; *c; c++)
    if (*c < 0x20 || *c > 0x7e) return false;
  storagePutString("tz", rule);
  storagePutString("tz_name", name);
  zoneRule = rule;
  zone = check;
  zoneName = name;
  if (!zoneName.length()) {
    const int i = tz::presetOf(rule, nullptr);
    char offset[16];
    tz::offsetName(zone, offset, sizeof offset);
    zoneName = i >= 0 ? tz::PRESETS[i].name : offset;
  }
  return true;
}
