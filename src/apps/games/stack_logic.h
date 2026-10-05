#pragma once
#include <stdint.h>

// Stack's rules, with no screen or Arduino in them (tools/gametest checks them on a computer).
// A frame is ~48 ms; the sliding block moves in 1/100 px (U), the tower is in pixels. Rows count up from
// the base (row 0).
namespace stack {

const int U = 100;
const int SCREEN = 200, BASE_X = 48, BASE_W = 104, KEEP = 12;  // KEEP: placed rows remembered (fills the screen)
const int SPEED = 120, SPEED_UP = 40, SPEED_EVERY = 8, SPEED_MAX = 280;  // U per frame (1.2 px to 2.8 px)
const int PERFECT = 3;  // a drop this close (px) to lined up snaps into place and keeps the full width

struct Row {
  int x, w;  // px
};

struct State {
  Row rows[KEEP];  // the top rows of the tower, by row number % KEEP
  int score;       // blocks placed (the top row is row `score`)
  int x, w, dir;   // the sliding block: x in U, w in px
  bool started, perfect;  // perfect: the last drop snapped into place
  Row cut;               // the piece the last drop cut off (w 0: none)
  int since;             // frames since the last drop (the screen shows "Perfect!" and the cut piece briefly)
};

inline const Row &top(const State &s) { return s.rows[s.score % KEEP]; }
inline int speed(const State &s) {
  const int v = SPEED + SPEED_UP * (s.score / SPEED_EVERY);
  return v < SPEED_MAX ? v : SPEED_MAX;
}

// A new block, the width of the top row, coming in from the left and the right in turn.
inline void spawn(State &s) {
  s.w = top(s).w;
  if (s.score % 2 == 0) s.x = 0, s.dir = 1;
  else s.x = (SCREEN - s.w) * U, s.dir = -1;
}

inline void reset(State &s, uint32_t) {  // no randomness: every round starts the same
  s = State();
  s.rows[0] = {BASE_X, BASE_W};
  spawn(s);
}

inline bool waiting(const State &s) { return !s.started; }

// One frame: `tapped` = B went down since the last frame. A tap drops the block where it is, otherwise it
// slides on, turning round at the screen edges. Returns false when a drop misses the tower.
inline bool step(State &s, bool tapped) {
  if (!s.started) {
    if (!tapped) return true;
    s.started = true, tapped = false;  // the first tap only starts the sliding
  }
  if (tapped) {
    const Row t = top(s);
    s.perfect = false, s.cut = {0, 0}, s.since = 0;
    int x = (s.x + U / 2) / U;  // to the nearest pixel
    if (x - t.x <= PERFECT && t.x - x <= PERFECT) x = t.x, s.perfect = true;
    const int l = x > t.x ? x : t.x, r = x + s.w < t.x + t.w ? x + s.w : t.x + t.w;
    if (r <= l) return false;
    if (x < l) s.cut = {x, l - x};
    else if (x + s.w > r) s.cut = {r, x + s.w - r};
    s.score++;
    s.rows[s.score % KEEP] = {l, r - l};
    spawn(s);
    return true;
  }
  s.since++;
  s.x += s.dir * speed(s);
  const int right = (SCREEN - s.w) * U;
  if (s.x < 0) s.x = -s.x, s.dir = 1;  // bounces off the edge
  if (s.x > right) s.x = 2 * right - s.x, s.dir = -1;
  if (s.x < 0 || s.x > right) s.x = s.x < 0 ? 0 : right;  // (a block nearly as wide as the screen)
  return true;
}

}  // namespace stack
