// Checks which saved WiFi network is tried first on a computer (no board needed):
//   c++ -std=c++17 -O2 -I src/core tools/nettest/wifi_test.cpp -o /tmp/wifi_test && /tmp/wifi_test
#include <cstdio>
#include <initializer_list>
#include "wifi_logic.h"

using namespace wifi;
static int failures;

static void check(bool ok, const char *what) {
  printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
  failures += !ok;
}

static bool is(const bool has[SLOTS], int last, std::initializer_list<int> want) {
  int out[SLOTS];
  const int n = order(has, last, out);
  if (n != (int)want.size()) return false;
  int i = 0;
  for (int w : want)
    if (out[i++] != w) return false;
  return true;
}

int main() {
  const bool none[SLOTS] = {false, false, false, false, false};
  const bool one[SLOTS] = {true, false, false, false, false};
  const bool all[SLOTS] = {true, true, true, true, true};
  const bool gaps[SLOTS] = {true, false, true, false, true};

  check(is(none, -1, {}), "no networks: nothing to try");
  check(is(none, 2, {}), "no networks, even with a remembered slot: nothing to try");
  check(is(one, -1, {0}), "one network: just that one");
  check(is(all, -1, {0, 1, 2, 3, 4}), "five networks, none remembered: in slot order");
  check(is(all, 3, {3, 0, 1, 2, 4}), "the one that worked last goes first, the rest keep their order");
  check(is(all, 0, {0, 1, 2, 3, 4}), "last = slot 0 changes nothing");
  check(is(gaps, -1, {0, 2, 4}), "empty slots are skipped");
  check(is(gaps, 4, {4, 0, 2}), "a remembered slot that is still saved goes first");
  check(is(gaps, 1, {0, 2, 4}), "a remembered slot that was removed is ignored");
  check(is(all, 9, {0, 1, 2, 3, 4}), "a remembered slot out of range is ignored");
  check(is(all, -5, {0, 1, 2, 3, 4}), "a negative remembered slot is ignored");

  printf(failures ? "%d FAILED\n" : "all passed\n", failures);
  return failures ? 1 : 0;
}
