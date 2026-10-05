#pragma once
#include <stdint.h>

// One game inside the Games app. The Games app owns the list, the frame loop, the game-over screen and the
// best scores; a game only plays. B is its one action (a tap, or held down); hold A leaves it.
struct Game {
  const char *name;
  const char *bestKey;                   // NVS key for the best score (at most 15 characters)
  uint16_t frameMs;                      // time per frame (e-ink: a partial refresh takes ~0.33 s)
  void (*start)(uint32_t seed);          // a new round
  bool (*step)(bool tapped, bool held);  // one frame of play; false once the round is over
  void (*draw)();                        // the whole play screen
  int (*score)();                        // the score so far
};
