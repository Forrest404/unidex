#pragma once
#include <stdint.h>

// Flappy's rules, with no screen or Arduino in them (so tools/gametest can check them on a computer).
// Units are screen pixels and frames; the screen is 200x200 and a frame is about half a second.
namespace flappy {

const int BIRD_X = 40, BIRD_W = 14, BIRD_H = 12, GROUND = 184;
const int GRAVITY = 6, MAX_FALL = 18, FLAP = -14;  // tuned so a player planning ~2.5 s ahead always gets through
const int PIPE_W = 28, LIP = 4, GAP = 80, PIPE_SPACING = 110, SPEED = 16, PIPES = 3;
const int GAP_MIN = 52, GAP_MAX = 140, GAP_STEP = 48;  // gap centres, and the most one moves from the last

struct Pipe {
  int x, gapY;  // left edge; centre of the gap
  bool scored;
};

struct State {
  int y, vy;  // bird top and speed
  Pipe pipes[PIPES];
  int score;
  bool started;  // waiting for the first tap until then
  uint32_t rng;
};

inline uint32_t next(State &s) {  // xorshift32: the same seed plays the same round
  s.rng ^= s.rng << 13;
  s.rng ^= s.rng >> 17;
  s.rng ^= s.rng << 5;
  return s.rng;
}

inline int nextGap(State &s, int from) {
  int lo = from - GAP_STEP < GAP_MIN ? GAP_MIN : from - GAP_STEP;
  int hi = from + GAP_STEP > GAP_MAX ? GAP_MAX : from + GAP_STEP;
  return lo + (int)(next(s) % (uint32_t)(hi - lo + 1));
}

inline void reset(State &s, uint32_t seed) {
  s = State();
  s.rng = seed ? seed : 1;
  s.y = 86;
  int gap = 96;
  for (int i = 0; i < PIPES; i++) {
    gap = nextGap(s, gap);
    s.pipes[i] = {200 + 40 + i * PIPE_SPACING, gap, false};
  }
}

inline bool overlaps(int ax, int ay, int aw, int ah, int bx, int by, int bw, int bh) {
  return ax < bx + bw && bx < ax + aw && ay < by + bh && by < ay + ah;
}

// One frame. Returns false when the round is over (the bird hit a pipe or the ground).
inline bool step(State &s, bool tapped) {
  if (!s.started) {
    if (!tapped) return true;  // the bird waits for the first flap
    s.started = true;
  }
  s.vy = tapped ? FLAP : (s.vy + GRAVITY > MAX_FALL ? MAX_FALL : s.vy + GRAVITY);
  s.y += s.vy;
  if (s.y < 0) s.y = 0, s.vy = 0;  // the top of the screen stops it
  if (s.y + BIRD_H >= GROUND) return false;

  for (Pipe &p : s.pipes) p.x -= SPEED;
  for (Pipe &p : s.pipes) {
    if (p.x + PIPE_W >= 0) continue;
    const Pipe *far = &s.pipes[0];  // gone off the left: it comes back after the farthest one
    for (const Pipe &q : s.pipes)
      if (q.x > far->x) far = &q;
    p = {far->x + PIPE_SPACING, nextGap(s, far->gapY), false};
  }
  for (Pipe &p : s.pipes) {
    const int top = p.gapY - GAP / 2, bottom = p.gapY + GAP / 2;
    if (overlaps(BIRD_X, s.y, BIRD_W, BIRD_H, p.x, 0, PIPE_W, top) ||
        overlaps(BIRD_X, s.y, BIRD_W, BIRD_H, p.x, bottom, PIPE_W, GROUND - bottom))
      return false;
    if (!p.scored && p.x + PIPE_W < BIRD_X) {
      p.scored = true;
      s.score++;
    }
  }
  return true;
}

}  // namespace flappy
