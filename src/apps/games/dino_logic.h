#pragma once
#include <stdint.h>

// Dino's rules, with no screen or Arduino in them (tools/gametest checks them on a computer).
// Units are pixels and frames (about half a second each). Heights are measured up from the ground.
namespace dino {

const int GROUND = 160, DINO_X = 24, DINO_W = 18, DINO_H = 20, INSET = 3;  // INSET: a forgiving hitbox
const int JUMP = 16, GRAVITY = 6, HELD_GRAVITY = 3;  // holding B while rising: weaker gravity, a higher jump
const int SPEED = 16, CACTI = 3, GAP_MIN = 144, GAP_MAX = 224;  // a held jump lands before the next cactus
const int SMALL_W = 12, SMALL_H = 22, TALL_W = 14, TALL_H = 36;  // a tap clears small; tall needs a held jump

struct Cactus {
  int x;
  bool tall;
};

struct State {
  int h, v;  // dino height above the ground, and upward speed
  Cactus cacti[CACTI];
  int score;  // frames run
  bool started;
  uint32_t rng;
};

inline uint32_t next(State &s) {  // xorshift32: the same seed plays the same round
  s.rng ^= s.rng << 13;
  s.rng ^= s.rng >> 17;
  s.rng ^= s.rng << 5;
  return s.rng;
}

inline int width(const Cactus &c) { return c.tall ? TALL_W : SMALL_W; }
inline int height(const Cactus &c) { return c.tall ? TALL_H : SMALL_H; }

inline Cactus nextCactus(State &s, int afterX) {
  const int gap = GAP_MIN + (int)(next(s) % (uint32_t)(GAP_MAX - GAP_MIN + 1));
  return {afterX + gap, next(s) % 3 == 0};  // about one in three is tall
}

inline void reset(State &s, uint32_t seed) {
  s = State();
  s.rng = seed ? seed : 1;
  int x = 200;
  for (Cactus &c : s.cacti) {
    c = nextCactus(s, x);
    x = c.x;
  }
}

inline bool waiting(const State &s) { return !s.started; }
inline bool onGround(const State &s) { return s.h == 0; }

// One frame: `tapped` = B went down since the last frame, `held` = B is down now.
// Returns false when the dino hits a cactus.
inline bool step(State &s, bool tapped, bool held) {
  if (!s.started) {
    if (!tapped) return true;
    s.started = true;
  }
  if (tapped && onGround(s)) s.v = JUMP;
  if (s.h > 0 || s.v > 0) {
    s.h += s.v;
    s.v -= held && s.v > 0 ? HELD_GRAVITY : GRAVITY;
    if (s.h <= 0) s.h = 0, s.v = 0;  // landed
  }
  for (Cactus &c : s.cacti) c.x -= SPEED;
  for (Cactus &c : s.cacti) {
    if (c.x + width(c) >= 0) continue;
    const Cactus *far = &s.cacti[0];  // gone off the left: comes back after the farthest one
    for (const Cactus &d : s.cacti)
      if (d.x > far->x) far = &d;
    c = nextCactus(s, far->x);
  }
  for (const Cactus &c : s.cacti) {
    const bool sideBySide = c.x < DINO_X + DINO_W - INSET && DINO_X + INSET < c.x + width(c);
    if (sideBySide && s.h < height(c) - INSET) return false;
  }
  s.score++;
  return true;
}

}  // namespace dino
