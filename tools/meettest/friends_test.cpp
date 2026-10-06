// Checks the Pet's friends list on a computer (no board needed):
//   c++ -std=c++17 -O2 -I src/apps/pet tools/meettest/friends_test.cpp -o /tmp/friends_test && /tmp/friends_test
#include <cstdio>
#include "friends_logic.h"

using namespace friends;
static int failures;

static void check(bool ok, const char *what) {
  printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
  failures += !ok;
}

int main() {
  List l;
  check(l.count == 0 && l.find(5) == -1, "a new list is empty");
  l.meet(5, 0x100, "Mochi", 1000);
  check(l.count == 1 && l.f[0].met == 1 && strcmp(l.f[0].name, "Mochi") == 0, "meeting adds a friend");
  l.meet(5, 0x200, "Moch", 2000);
  check(l.count == 1 && l.f[0].met == 2 && l.f[0].look == 0x200 && strcmp(l.f[0].name, "Moch") == 0 &&
            l.f[0].lastMet == 2000,
        "meeting again counts it and brings the look and name up to date");
  l.meet(6, 0, "ABCDEFGHIJKLMNOP", 3000);
  check(strlen(l.f[1].name) == 12, "a long name is kept to 12 characters");
  for (uint32_t id = 10; id < 30; id++) l.meet(id, 0, "x", 4000 + id);
  check(l.count == MAX && l.find(5) == -1 && l.find(29) >= 0, "a full list makes room by forgetting the one met longest ago");
  const int i = l.find(29);
  l.remove(i);
  check(l.count == MAX - 1 && l.find(29) == -1, "removing a friend");
  l.remove(-1);
  l.remove(99);
  check(l.count == MAX - 1, "removing nothing changes nothing");

  List back;
  check(back.load(&l, sizeof l) && back.count == l.count && back.find(28) == l.find(28), "a saved list loads back");
  List damaged = l;
  damaged.version = 99;
  check(!back.load(&damaged, sizeof damaged) && back.count == 0, "a block from another version loads as no friends");
  check(!back.load(&l, sizeof l - 1) && back.count == 0, "a block of the wrong size loads as no friends");
  damaged = l;
  damaged.count = 200;
  check(back.load(&damaged, sizeof damaged) && back.count == 0, "a damaged count loads as no friends");

  printf(failures ? "\n%d check(s) failed\n" : "\nall checks passed\n", failures);
  return failures ? 1 : 0;
}
