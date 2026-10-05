// Checks Dino's rules on a computer (no board needed):
//   c++ -std=c++17 -O2 -I src/apps/games tools/gametest/dino_test.cpp -o /tmp/dino_test && /tmp/dino_test
#include <cstdio>
#include <initializer_list>
#include "dino_logic.h"

using namespace dino;
static int failures;

static void check(bool ok, const char *what) {
  printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
  failures += !ok;
}

// The highest the dino gets from a standing jump: tapped only, or held all the way up.
static int apex(bool held) {
  State s;
  reset(s, 1);
  s.started = true;
  for (Cactus &c : s.cacti) c.x = 10000;  // nothing in the way
  int top = 0;
  step(s, true, held);
  for (int i = 0; i < 40 && s.h > 0; i++) {
    top = s.h > top ? s.h : top;
    step(s, false, held);
  }
  return top;
}

// Frames survived from `s` with the best presses, looking `depth` frames ahead. The press is B being down
// or up each frame; a tap is B going down.
static int survive(const State &s, bool wasDown, int depth) {
  if (depth == 0) return 0;
  int best = 0;
  for (bool down : {false, true}) {
    State t = s;
    if (!step(t, down && !wasDown, down)) continue;
    const int n = 1 + survive(t, down, depth - 1);
    if (n > best) best = n;
    if (best == depth) break;
  }
  return best;
}

static bool autopilotDown(const State &s, bool wasDown, int depth) {
  State a = s, b = s;
  const int up = step(a, false, false) ? 1 + survive(a, false, depth - 1) : 0;
  const int down = step(b, !wasDown, true) ? 1 + survive(b, true, depth - 1) : 0;
  return down > up;
}

int main() {
  State s;
  reset(s, 42);
  for (int i = 0; i < 20; i++) step(s, false, false);
  check(waiting(s) && s.score == 0, "the dino waits for the first tap");

  reset(s, 42);
  step(s, true, false);
  int frames = 1;
  while (step(s, false, false) && frames < 200) frames++;
  check(frames < 200, "no presses after the first: the dino hits a cactus");

  const int tapTop = apex(false), heldTop = apex(true);
  printf("      tap jump reaches %d px, held jump %d px (small cactus %d, tall %d)\n", tapTop, heldTop, SMALL_H, TALL_H);
  check(tapTop >= SMALL_H - INSET && tapTop < TALL_H - INSET, "a tap clears a small cactus but not a tall one");
  check(heldTop >= TALL_H - INSET, "a held jump clears a tall cactus");

  State a, b;
  reset(a, 9);
  reset(b, 9);
  bool same = true, wasDown = false;
  for (int i = 0; i < 300; i++) {
    const bool down = autopilotDown(a, wasDown, 8);
    const bool ra = step(a, down && !wasDown, down), rb = step(b, down && !wasDown, down);
    wasDown = down;
    same &= ra == rb && a.h == b.h && a.score == b.score;
    if (!ra) break;
  }
  check(same, "the same seed plays the same round");

  int crashed = 0;
  for (uint32_t seed = 1; seed <= 50; seed++) {
    reset(s, seed);
    step(s, true, false);
    wasDown = false;
    bool alive = true;
    while (s.score < 1000 && alive) {
      const bool down = autopilotDown(s, wasDown, 5);
      alive = step(s, down && !wasDown, down);
      wasDown = down;
    }
    if (!alive) {
      printf("      seed %u: hit a cactus at score %d\n", seed, s.score);
      crashed++;
    }
  }
  check(!crashed, "an autopilot looking 2.5 s ahead runs 1000 frames on 50 seeds (every cactus is clearable)");

  printf("%s\n", failures ? "SOME CHECKS FAILED" : "all checks passed");
  return failures ? 1 : 0;
}
