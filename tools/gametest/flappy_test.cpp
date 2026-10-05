// Checks Flappy's rules on a computer (no board needed):
//   c++ -std=c++17 -I src/apps/games tools/gametest/flappy_test.cpp -o /tmp/flappy_test && /tmp/flappy_test
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include "flappy_logic.h"

using namespace flappy;
static int failures;

static void check(bool ok, const char *what) {
  printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
  failures += !ok;
}

// The autopilot chooses every DECIDE frames (~0.5 s, as slow as a person reacting to the screen): a tap at
// the start of the block, or not, then no taps for the rest of it.
const int DECIDE = 10;

static bool run(State &s, bool tap) {
  for (int i = 0; i < DECIDE; i++)
    if (!step(s, tap && i == 0)) return false;
  return true;
}

// How many blocks the bird survives from `s` with the best choices, looking up to `depth` blocks ahead.
static int survive(const State &s, int depth) {
  if (depth == 0) return 0;
  int best = 0;
  for (bool tap : {false, true}) {
    State t = s;
    if (!run(t, tap)) continue;
    const int n = 1 + survive(t, depth - 1);
    if (n > best) best = n;
    if (best == depth) break;
  }
  return best;
}

// Plays like a careful player: the choice that keeps the bird alive longest over the next 8 blocks (~4 s,
// preferring not to tap). If even this crashes, a gap was out of reach: the game would be unfair.
static bool autopilotTap(const State &s) {
  State a = s, b = s;
  const int noTap = run(a, false) ? 1 + survive(a, 7) : 0;
  const int tap = run(b, true) ? 1 + survive(b, 7) : 0;
  return tap > noTap;
}

int main() {
  State s;

  reset(s, 42);
  const int startY = s.y;
  for (int i = 0; i < 20; i++) step(s, false);
  check(s.y == startY && !s.started, "the bird waits for the first tap");

  reset(s, 42);
  step(s, true);
  int frames = 1, top = s.y;
  while (step(s, false) && frames < 500) frames++, top = s.y < top ? s.y : top;
  printf("      a flap rises %d px; with no more taps the round ends after %d frames\n", (86 * U - top) / U, frames);
  check(frames <= 120, "no taps after the first: the round ends within ~6 s");
  check((86 * U - top) / U >= 20 && (86 * U - top) / U <= 28, "a flap rises about 24 px (as tuned at 2 fps)");

  State a, b;
  reset(a, 7);
  reset(b, 7);
  bool same = true;
  for (int i = 0; i < 200; i++) {
    const bool tap = autopilotTap(a);
    const bool ra = run(a, tap), rb = run(b, tap);
    same &= ra == rb && a.y == b.y && a.score == b.score;
    if (!ra) break;
  }
  check(same, "the same seed plays the same round");

  bool fair = true, inRange = true;
  int crashed = 0;
  for (uint32_t seed = 1; seed <= 50; seed++) {
    reset(s, seed);
    step(s, true);
    int lastScore = 0, n = 0;
    bool alive = true;
    while (s.score < 100 && n < 20000) {
      alive = run(s, autopilotTap(s));
      if (!alive) break;
      if (s.score - lastScore > 1) fair = false;  // never more than one point at a time
      for (const Pipe &p : s.pipes) inRange &= p.gapY >= GAP_MIN && p.gapY <= GAP_MAX;
      lastScore = s.score;
      n++;
    }
    if (!alive || s.score < 100) {
      printf("      seed %u: crashed at score %d after %d frames\n", seed, s.score, n);
      crashed++;
    }
  }
  check(fair && !crashed, "an autopilot looking 4 s ahead passes 100 pipes on 50 seeds (every gap is fair)");

  check(inRange, "every gap centre stays in range");

  printf("%s\n", failures ? "SOME CHECKS FAILED" : "all checks passed");
  return failures ? 1 : 0;
}
