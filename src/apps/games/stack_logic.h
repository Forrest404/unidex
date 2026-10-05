#pragma once
#include <stdint.h>

// Stack's rules, with no screen or Arduino in them (tools/gametest checks them on a computer).
// Units are pixels and frames (about half a second each). Rows count up from the base (row 0).
namespace stack {

const int SCREEN = 200, BASE_X = 48, BASE_W = 104, KEEP = 12;  // KEEP: placed rows remembered (fills the screen)
const int SPEED = 12, SPEED_UP = 4, SPEED_EVERY = 8, SPEED_MAX = 28;

struct Row {
  int x, w;
};

struct State {
  Row rows[KEEP];  // the top rows of the tower, by row number % KEEP
  int score;       // blocks placed (the top row is row `score`)
  int x, w, dir;   // the sliding block
  int seen;        // where it was one step ago: what the screen showed while you pressed (see step)
  bool started, perfect;  // perfect: the last drop lined up exactly
  Row cut;               // the piece the last drop cut off (w 0: none)
};

inline const Row &top(const State &s) { return s.rows[s.score % KEEP]; }
inline int speed(const State &s) {
  const int v = SPEED + SPEED_UP * (s.score / SPEED_EVERY);
  return v < SPEED_MAX ? v : SPEED_MAX;
}

// A new block, the width of the top row, on a grid of `speed` steps through the top row's x, so one of its
// positions always lines up exactly (at 2 frames a second a block can't stop just anywhere). It comes in
// from the left and the right in turn, as far out as fits on the screen.
inline void spawn(State &s) {
  const Row &t = top(s);
  const int v = speed(s);
  s.w = t.w;
  if (s.score % 2 == 0) s.x = t.x % v, s.dir = 1;
  else s.x = t.x + (SCREEN - t.w - t.x) / v * v, s.dir = -1;
  s.seen = s.x;
}

inline void reset(State &s, uint32_t) {  // no randomness: every round starts the same
  s = State();
  s.rows[0] = {BASE_X, BASE_W};
  spawn(s);
}

inline bool waiting(const State &s) { return !s.started; }

// One frame: `tapped` = B went down since the last frame. A tap drops the block where it was one step ago:
// a frame's picture takes most of the frame (~0.43 of 0.48 s) to appear on e-ink, so a press is nearly always
// a reaction to the picture before the newest one. Otherwise the block slides one step, turning round on its
// grid at the screen edges. Returns false when a drop misses the tower.
inline bool step(State &s, bool tapped) {
  if (!s.started) {
    if (!tapped) return true;
    s.started = true, tapped = false;  // the first tap only starts the sliding
  }
  s.perfect = false, s.cut = {0, 0};
  if (tapped) {
    s.x = s.seen;
    const Row t = top(s);
    const int l = s.x > t.x ? s.x : t.x, r = s.x + s.w < t.x + t.w ? s.x + s.w : t.x + t.w;
    if (r <= l) return false;
    s.perfect = s.x == t.x;
    if (s.x < l) s.cut = {s.x, l - s.x};
    else if (s.x + s.w > r) s.cut = {r, s.x + s.w - r};
    s.score++;
    s.rows[s.score % KEEP] = {l, r - l};
    spawn(s);
    return true;
  }
  const int v = speed(s);
  s.seen = s.x;
  int nx = s.x + s.dir * v;
  if (nx < 0 || nx + s.w > SCREEN) s.dir = -s.dir, nx = s.x + s.dir * v;
  if (nx >= 0 && nx + s.w <= SCREEN) s.x = nx;  // (a block with room for only one spot stays put)
  return true;
}

}  // namespace stack
