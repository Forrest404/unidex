#pragma once
#include <time.h>
#include "zone_logic.h"

void clockBegin();        // once per boot: time zone, then load the PCF85063 time into system time
bool clockValid();
const char *clockSync();  // NTP over WiFi, radio off after; nullptr on success, else a short reason
const char *clockSyncOnline();  // the same with WiFi already up (left on)
bool clockSet(time_t utc);  // false (and nothing changed) for a time outside 2024-2099
void clockStatus(char *out, size_t len);  // chip registers + system time, for the USB `C` command
struct tm clockLocal();   // the system time here (in the device's time zone)

// The device's time zone: London unless Settings > Time zone or a computer (USB `Z`, the Tools page, the Mac agent)
// set another. The chip and the system clock keep UTC; this only decides how it reads.
const char *clockZoneRule();      // the TZ rule as saved, e.g. "CET-1CEST,M3.5.0,M10.5.0/3"
const char *clockZoneName();      // what to call it: "Paris", or "UTC+1" for a rule that came with no name
const tz::Zone &clockHomeZone();  // the rule, for sums that can't swap TZ (Clock's alarms, the calendar link)
// Saves and uses a new zone. `name` may be empty (then it's the preset's name, or the offset). False, and nothing
// changed, for a rule this can't read or a name that isn't plain text up to 23 characters.
bool clockSetZone(const char *rule, const char *name);
