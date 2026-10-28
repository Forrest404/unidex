// Checks the chime's tunes (src/core/ringtones.h) on a computer, no board needed:
//   c++ -std=c++17 -O2 -I src/core tools/ringtonetest/ringtone_test.cpp -o /tmp/ringtone_test && /tmp/ringtone_test
#include <cstdio>
#include <cstring>
#include "ringtones.h"

using namespace ringtones;
static int failures;

static void check(bool ok, const char *what) {
  printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
  failures += !ok;
}

int main() {
  check(COUNT == 5, "five tunes");
  check(!strcmp(TUNES[0].name, "Chime") && lengthMs(TUNES[0]) == 360, "the first is the chime it always had");
  for (int i = 0; i < COUNT; i++) {
    const Tune &t = TUNES[i];
    char what[80];
    snprintf(what, sizeof what, "%s: %d ms, up to %d, with sound in it", t.name, lengthMs(t), MAX_MS);
    bool sound = false, audible = true;
    for (const Note &n : t.notes) {
      if (!n.hz && !n.ms) break;
      sound |= n.hz > 0;
      if (n.hz && (n.hz < 200 || n.hz > 4000)) audible = false;  // the little speaker can't do much outside this
    }
    check(lengthMs(t) > 0 && lengthMs(t) <= MAX_MS && sound && audible, what);
    for (int j = 0; j < i; j++)
      if (!strcmp(TUNES[j].name, t.name)) check(false, "names are different");
    check(strlen(t.name) <= 8, "a short name, for the Settings row");
  }
  printf(failures ? "\n%d FAILED\n" : "\nall passed\n", failures);
  return failures ? 1 : 0;
}
