// The clock is the browser's.
#include "../../src/core/clock.h"
#include <Arduino.h>
void clockBegin() {}
bool clockValid() { return time(nullptr) > 1704067200; }
const char *clockSync() { return "No WiFi in the demo"; }
void clockSet(time_t) {}
void clockStatus(char *out, size_t len) { snprintf(out, len, "browser clock"); }
struct tm clockLocal() {
  time_t t = time(nullptr);
  struct tm tm;
  localtime_r(&t, &tm);
  return tm;
}
