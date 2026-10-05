#pragma once
#include <stdint.h>

// Jetpack's rules, with no screen or Arduino in them (tools/gametest checks them on a computer).
// Units are pixels and frames (about half a second each). y grows downwards, like the screen.
namespace jetpack {

const int CEILING = 28, FLOOR = 184, PILOT_X = 32, PILOT_W = 16, PILOT_H = 18, INSET = 3;  // INSET: forgiving
const int LOWEST = FLOOR - PILOT_H;                    // the pilot's y standing on the floor
const int THRUST = 6, GRAVITY = 4, MAX_V = 12;         // B down: speed up upwards; B up: fall
// Zapper sizes and gaps: an autopilot changing B only once a second always gets through (tools/gametest);
// with these lengths even one changing it every 2 s does (checked while tuning).
const int SPEED = 16, ZAPPERS = 3, ZAP_W = 8, LEN_MIN = 36, LEN_MAX = 56, GAP_MIN = 112, GAP_MAX = 176;
const int COINS = 4, COIN = 10, COIN_STEP = 14, COIN_POINTS = 5;  // a row of coins halfway to each zapper

enum Kind : uint8_t { ON_FLOOR, ON_CEILING, FLOATING };

struct Zapper {
  int x, y, len;  // y: the top end
  int coinX, coinY;
  uint8_t coins;  // bit i: coin i still there
};

struct State {
  int y, v;  // the pilot's top, and speed (negative = up)
  Zapper zappers[ZAPPERS];
  int frames, coins;
  bool started;
  uint32_t rng;
};

inline uint32_t next(State &s) {  // xorshift32: the same seed plays the same round
  s.rng ^= s.rng << 13;
  s.rng ^= s.rng >> 17;
  s.rng ^= s.rng << 5;
  return s.rng;
}

inline int between(State &s, int lo, int hi) { return lo + (int)(next(s) % (uint32_t)(hi - lo + 1)); }

// The next zapper a random gap past `afterX`: standing on the floor, hanging from the ceiling or floating,
// with a row of coins halfway along the gap.
inline Zapper nextZapper(State &s, int afterX) {
  Zapper z;
  const int gap = between(s, GAP_MIN, GAP_MAX);
  z.x = afterX + gap;
  z.len = between(s, LEN_MIN, LEN_MAX);
  const Kind kind = (Kind)(next(s) % 3);
  if (kind == ON_FLOOR) z.y = FLOOR - z.len;
  else if (kind == ON_CEILING) z.y = CEILING;
  else z.y = between(s, CEILING + PILOT_H + 8, FLOOR - PILOT_H - 8 - z.len);  // room to pass above and below
  z.coinX = z.x - gap / 2 - COINS * COIN_STEP / 2;
  z.coinY = between(s, CEILING + 2, FLOOR - 2 - COIN);
  z.coins = (1 << COINS) - 1;
  return z;
}

inline void reset(State &s, uint32_t seed) {
  s = State();
  s.rng = seed ? seed : 1;
  s.y = LOWEST;
  int x = 120;  // the first zapper comes in from past the right edge
  for (Zapper &z : s.zappers) {
    z = nextZapper(s, x);
    x = z.x;
  }
}

inline bool waiting(const State &s) { return !s.started; }
inline bool onFloor(const State &s) { return s.y == LOWEST; }
inline int score(const State &s) { return s.frames + COIN_POINTS * s.coins; }

inline bool touches(const State &s, int x, int y, int w, int h) {  // the pilot's box, a little inside the sprite
  return x < PILOT_X + PILOT_W - INSET && PILOT_X + INSET < x + w && y < s.y + PILOT_H - INSET && s.y + INSET < y + h;
}

// One frame: `thrust` = B is down (or was tapped since the last frame). The floor and the ceiling are safe;
// returns false when the pilot touches a zapper.
inline bool step(State &s, bool thrust) {
  if (!s.started) {
    if (!thrust) return true;
    s.started = true;
  }
  s.v += thrust ? -THRUST : GRAVITY;
  s.v = s.v < -MAX_V ? -MAX_V : s.v > MAX_V ? MAX_V : s.v;
  s.y += s.v;
  if (s.y >= LOWEST) s.y = LOWEST, s.v = 0;
  if (s.y <= CEILING) s.y = CEILING, s.v = 0;

  for (Zapper &z : s.zappers) z.x -= SPEED, z.coinX -= SPEED;
  for (Zapper &z : s.zappers) {
    if (z.x + ZAP_W >= 0) continue;
    const Zapper *far = &s.zappers[0];  // gone off the left: comes back after the farthest one
    for (const Zapper &d : s.zappers)
      if (d.x > far->x) far = &d;
    z = nextZapper(s, far->x);
  }
  for (Zapper &z : s.zappers) {
    for (int i = 0; i < COINS; i++)
      if (z.coins & (1 << i) && touches(s, z.coinX + i * COIN_STEP, z.coinY, COIN, COIN)) {
        z.coins &= ~(1 << i);
        s.coins++;
      }
    if (touches(s, z.x, z.y, ZAP_W, z.len)) return false;
  }
  s.frames++;
  return true;
}

}  // namespace jetpack
