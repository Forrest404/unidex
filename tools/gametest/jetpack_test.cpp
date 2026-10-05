// Checks Jetpack's rules on a computer (no board needed):
//   c++ -std=c++17 -O2 -I src/apps/games tools/gametest/jetpack_test.cpp -o /tmp/jetpack_test && /tmp/jetpack_test
#include <cstdio>
#include <initializer_list>
#include "jetpack_logic.h"

using namespace jetpack;
static int failures;

static void check(bool ok, const char *what) {
  printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
  failures += !ok;
}

// The e-ink shows each frame late, so a person can't switch B every frame. The autopilot is held to the
// same: it picks B down or up for HOLD frames at a time, looking `depth` choices ahead.
const int HOLD = 2;

static bool run(State &s, bool down) {
  for (int i = 0; i < HOLD; i++)
    if (!step(s, down)) return false;
  return true;
}

static int survive(const State &s, int depth) {
  if (depth == 0) return 0;
  int best = 0;
  for (bool down : {false, true}) {
    State t = s;
    if (!run(t, down)) continue;
    const int n = 1 + survive(t, depth - 1);
    if (n > best) best = n;
    if (best == depth) break;
  }
  return best;
}

static bool autopilotDown(const State &s, int depth) {
  State a = s, b = s;
  const int up = run(a, false) ? 1 + survive(a, depth - 1) : 0;
  const int down = run(b, true) ? 1 + survive(b, depth - 1) : 0;
  return down > up;
}

int main() {
  State s;
  reset(s, 42);
  for (int i = 0; i < 20; i++) step(s, false);
  check(waiting(s) && s.frames == 0 && onFloor(s), "the pilot waits on the floor for the first press");

  reset(s, 42);
  step(s, true);
  check(!waiting(s) && s.y < LOWEST, "the first press lifts off");

  reset(s, 7);
  step(s, true);
  bool floorSafe = true;
  int frames = 1;
  while (frames < 400 && step(s, false)) frames++, floorSafe &= s.y <= LOWEST;
  bool hitFloorZapper = false;
  for (const Zapper &z : s.zappers) hitFloorZapper |= touches(s, z.x, z.y, ZAP_W, z.len) && z.y + z.len == FLOOR;
  check(frames < 400 && onFloor(s) && hitFloorZapper && floorSafe,
        "no presses: runs along the floor (safe) until a floor zapper");

  reset(s, 7);
  frames = 0;
  while (frames < 400 && step(s, true)) frames++;
  bool hitCeilingZapper = false;
  for (const Zapper &z : s.zappers) hitCeilingZapper |= touches(s, z.x, z.y, ZAP_W, z.len) && z.y == CEILING;
  check(frames < 400 && s.y == CEILING && hitCeilingZapper, "B held: rides the ceiling (safe) until a ceiling zapper");

  bool limits = true;
  reset(s, 3);
  for (int i = 0; i < 200; i++) {
    const int before = s.y;
    if (!step(s, (i / 3) % 2)) break;
    const int moved = s.y - before;
    limits &= moved >= -MAX_V && moved <= MAX_V && s.y >= CEILING && s.y <= LOWEST;
  }
  check(limits, "speed stays within the limit and the pilot stays on screen");

  State c;  // a coin straight ahead of the pilot: +5 once
  reset(c, 5);
  c.started = true;
  for (Zapper &z : c.zappers) z.x = 10000, z.coins = 0;
  c.zappers[0].coinX = PILOT_X + SPEED, c.zappers[0].coinY = LOWEST + 4, c.zappers[0].coins = 1;
  step(c, false);
  const int afterOne = score(c);
  step(c, false);
  check(c.coins == 1 && afterOne == 1 + COIN_POINTS && score(c) == 2 + COIN_POINTS, "a coin adds 5, once");

  State a, b;
  reset(a, 9);
  reset(b, 9);
  step(a, true), step(b, true);
  bool same = true;
  for (int i = 0; i < 300; i++) {
    const bool down = autopilotDown(a, 5);
    const bool ra = run(a, down), rb = run(b, down);
    same &= ra == rb && a.y == b.y && score(a) == score(b);
    if (!ra) break;
  }
  check(same, "the same seed plays the same round");

  int crashed = 0, coins = 0;
  for (uint32_t seed = 1; seed <= 50; seed++) {
    reset(s, seed);
    step(s, true);  // lift off (until the first press the pilot just stands there)
    bool alive = true;
    while (s.frames < 1000 && alive) alive = run(s, autopilotDown(s, 6));
    if (!alive) {
      printf("      seed %u: hit a zapper at frame %d\n", seed, s.frames);
      crashed++;
    }
    coins += s.coins;
  }
  printf("      (the autopilot picked up %d coins in all, without trying)\n", coins);
  check(!crashed, "an autopilot changing B only once a second runs 1000 frames on 50 seeds (always a way through)");

  printf("%s\n", failures ? "SOME CHECKS FAILED" : "all checks passed");
  return failures ? 1 : 0;
}
