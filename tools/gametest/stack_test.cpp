// Checks Stack's rules on a computer (no board needed):
//   c++ -std=c++17 -O2 -I src/apps/games tools/gametest/stack_test.cpp -o /tmp/stack_test && /tmp/stack_test
#include <cstdio>
#include "stack_logic.h"

using namespace stack;
static int failures;

static void check(bool ok, const char *what) {
  printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
  failures += !ok;
}

// Slides (no drop) until a drop would line up with the top row; false if it doesn't within two passes.
static bool slideToLineUp(State &s) {
  for (int i = 0; i < 2 * SCREEN; i++) {
    if (s.seen == top(s).x) return true;
    step(s, false);
  }
  return false;
}

int main() {
  State s;
  reset(s, 0);
  const int x0 = s.x;
  for (int i = 0; i < 20; i++) step(s, false);
  check(waiting(s) && s.x == x0 && s.score == 0, "the block waits for the first press");
  step(s, true);
  check(!waiting(s) && s.score == 0, "the first press only starts the sliding (nothing dropped)");

  slideToLineUp(s);
  step(s, true);
  check(s.score == 1 && s.perfect && top(s).w == BASE_W && !s.cut.w, "a lined-up drop is perfect: full width");

  slideToLineUp(s);
  step(s, false);  // one frame late
  const int v = speed(s), w = top(s).w;
  step(s, true);
  check(s.score == 2 && !s.perfect && top(s).w == w - v && s.cut.w == v, "a drop one frame off cuts one step");

  reset(s, 0);
  step(s, true);
  while (s.seen <= BASE_X) step(s, false);  // one step right of lined up: the block is now two steps right
  const int shown = s.seen, now = s.x;
  step(s, true);
  check(now != shown && top(s).x == shown, "a drop lands where the block was one step ago (on the last picture)");

  bool stays = true;
  for (int i = 0; i < 100; i++) {
    step(s, false);
    stays &= s.x >= 0 && s.x + s.w <= SCREEN;
  }
  check(stays, "the sliding block stays on the screen");

  s = State();  // a narrow tower (a full-width block always overlaps somewhere): slide clear of it, then drop
  s.started = true, s.rows[0] = {100, 20};
  spawn(s);
  int frames = 0;
  while (frames < 100 && s.seen + s.w > top(s).x && s.seen < top(s).x + top(s).w) step(s, false), frames++;
  check(frames < 100 && !step(s, true), "a drop that misses the tower ends the round");

  // Every pass lines up exactly, for any top row, width and speed (the grid through the top row's x).
  bool linesUp = true;
  for (int score = 0; score < 40; score++)
    for (int tw = 1; tw <= BASE_W; tw++)
      for (int tx = 0; tx + tw <= SCREEN; tx++) {
        State t = State();
        t.started = true, t.score = score;
        t.rows[score % KEEP] = {tx, tw};
        spawn(t);
        linesUp &= t.x >= 0 && t.x + t.w <= SCREEN && slideToLineUp(t);
      }
  check(linesUp, "every block lines up exactly at some frame (all widths, positions and speeds)");

  State a = State();
  check(speed(a) == 12 && (a.score = 8, speed(a) == 16) && (a.score = 16, speed(a) == 20) &&
        (a.score = 100, speed(a) == SPEED_MAX), "speed: 12, then +4 every 8 blocks, at most 28");

  reset(s, 0);
  step(s, true);
  bool perfectAll = true;
  while (s.score < 300 && perfectAll) {
    perfectAll = slideToLineUp(s) && step(s, true) && s.perfect && top(s).w == BASE_W;
  }
  check(perfectAll, "a perfect player places 300 blocks at full width");

  printf("%s\n", failures ? "SOME CHECKS FAILED" : "all checks passed");
  return failures ? 1 : 0;
}
