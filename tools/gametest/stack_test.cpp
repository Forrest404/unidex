// Checks Stack's rules on a computer (no board needed):
//   c++ -std=c++17 -O2 -I src/apps/games tools/gametest/stack_test.cpp -o /tmp/stack_test && /tmp/stack_test
#include <cstdio>
#include <cstdlib>
#include "stack_logic.h"

using namespace stack;
static int failures;

static void check(bool ok, const char *what) {
  printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
  failures += !ok;
}

static int px(const State &s) { return (s.x + U / 2) / U; }

// Slides (no drop) until the block is close enough to lined up for a perfect drop; false if it never gets
// there within two passes.
static bool slideToPerfect(State &s) {
  for (int i = 0; i < 2000; i++) {
    if (abs(px(s) - top(s).x) <= PERFECT) return true;
    step(s, false);
  }
  return false;
}

// A started round with the block placed `off` px right of lined up (left if negative), then dropped.
static State dropAt(int off) {
  State t;
  reset(t, 0);
  step(t, true);
  t.x = (BASE_X + off) * U;
  step(t, true);
  return t;
}

int main() {
  State s;
  reset(s, 0);
  const int x0 = s.x;
  for (int i = 0; i < 20; i++) step(s, false);
  check(waiting(s) && s.x == x0 && s.score == 0, "the block waits for the first press");
  step(s, true);
  check(!waiting(s) && s.score == 0, "the first press only starts the sliding (nothing dropped)");

  bool snaps = true;
  for (int off = -PERFECT; off <= PERFECT; off++) {
    const State t = dropAt(off);
    snaps &= t.score == 1 && t.perfect && top(t).w == BASE_W && top(t).x == BASE_X && !t.cut.w;
  }
  check(snaps, "a drop within 3 px of lined up snaps into place: perfect, full width");

  const State right = dropAt(6), left = dropAt(-4);
  check(!right.perfect && top(right).w == BASE_W - 6 && right.cut.w == 6 && right.cut.x == BASE_X + BASE_W &&
        !left.perfect && top(left).w == BASE_W - 4 && left.cut.w == 4 && left.cut.x == BASE_X - 4,
        "a drop further off cuts exactly the overhang (6 px right, 4 px left)");

  reset(s, 0);
  step(s, true);
  check(slideToPerfect(s), "sliding, the block passes close enough for a perfect drop");

  bool stays = true;
  for (int i = 0; i < 3000; i++) {
    step(s, false);
    stays &= s.x >= 0 && s.x <= (SCREEN - s.w) * U;
  }
  check(stays, "the sliding block stays on the screen");

  s = State();  // a narrow tower (a full-width block always overlaps somewhere): slide clear of it, then drop
  s.started = true, s.rows[0] = {100, 20};
  spawn(s);
  int frames = 0;
  while (frames < 1000 && px(s) + s.w > top(s).x && px(s) < top(s).x + top(s).w) step(s, false), frames++;
  check(frames < 1000 && !step(s, true), "a drop that misses the tower ends the round");

  State a = State();
  check(speed(a) == 120 && (a.score = 8, speed(a) == 160) && (a.score = 16, speed(a) == 200) &&
        (a.score = 100, speed(a) == SPEED_MAX), "speed: 1.2 px a frame, then +0.4 every 8 blocks, at most 2.8");

  reset(s, 0);
  step(s, true);
  bool perfectAll = true;
  while (s.score < 300 && perfectAll) perfectAll = slideToPerfect(s) && step(s, true) && s.perfect && top(s).w == BASE_W;
  check(perfectAll, "a perfect player places 300 blocks at full width");

  printf("%s\n", failures ? "SOME CHECKS FAILED" : "all checks passed");
  return failures ? 1 : 0;
}
