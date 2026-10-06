#pragma once
// Pet's look: one part per layer, saved as one number (4 bits a layer), and drawn from the 32x32 parts in
// parts.h. No Arduino code, so it can be checked on a computer (tools/pettest).
#include <stddef.h>
#include <stdint.h>
#include "parts.h"

namespace pet {

enum Layer { BODY, EYES_LAYER, MOUTH, HAT, EXTRA, LAYERS };
static const char *const LAYER_NAMES[LAYERS] = {"Body", "Eyes", "Mouth", "Hat", "Extra"};
static const int SIZE = 32;  // each part is SIZE x SIZE pixels

template <size_t N> constexpr int countOf(const PetPart (&)[N]) { return N; }

struct LayerParts {
  const PetPart *parts;
  int count;
};

inline LayerParts partsOf(int layer) {
  switch (layer) {
    case BODY: return {BODIES, countOf(BODIES)};
    case EYES_LAYER: return {EYES, countOf(EYES)};
    case MOUTH: return {MOUTHS, countOf(MOUTHS)};
    case HAT: return {HATS, countOf(HATS)};
    default: return {EXTRAS, countOf(EXTRAS)};
  }
}

struct Look {
  uint8_t part[LAYERS];
};

// The look a new Pet starts with: the first body, eyes and mouth, no hat, blush.
static const Look DEFAULT_LOOK = {{0, 0, 0, 0, 1}};

inline uint32_t pack(const Look &l) {
  uint32_t v = 0;
  for (int i = 0; i < LAYERS; i++) v |= (uint32_t)(l.part[i] & 0x0F) << (4 * i);
  return v;
}

// A saved number back to a look. A part that doesn't exist (a damaged value, or a part removed in a later
// version) falls back to the default for that layer.
inline Look unpack(uint32_t v) {
  Look l;
  for (int i = 0; i < LAYERS; i++) {
    const uint8_t p = (v >> (4 * i)) & 0x0F;
    l.part[i] = p < partsOf(i).count ? p : DEFAULT_LOOK.part[i];
  }
  return l;
}

// Option `step` places along in a layer (+1 next, -1 previous), wrapping around.
inline void stepPart(Look &l, int layer, int step) {
  const int n = partsOf(layer).count;
  l.part[layer] = (uint8_t)(((l.part[layer] + step) % n + n) % n);
}

// A random look from any random number source (one call per layer).
template <typename Rand> Look randomLook(Rand rand) {
  Look l;
  for (int i = 0; i < LAYERS; i++) l.part[i] = (uint8_t)(rand() % (uint32_t)partsOf(i).count);
  return l;
}

// Draws the look, layer by layer: pixel(x, y, ink) for every pixel a layer sets ('#' ink, 'o' paper), with x
// and y from 0 to SIZE - 1. eyesClosed draws EYES_CLOSED instead of the chosen eyes (a blink).
template <typename Pixel> void drawLook(const Look &l, bool eyesClosed, Pixel pixel) {
  for (int layer = 0; layer < LAYERS; layer++) {
    const PetPart &p = layer == EYES_LAYER && eyesClosed ? EYES_CLOSED : partsOf(layer).parts[l.part[layer]];
    for (int y = 0; y < SIZE; y++)
      for (int x = 0; x < SIZE; x++) {
        const char c = p.rows[y][x];
        if (c == '#') pixel(x, y, true);
        else if (c == 'o') pixel(x, y, false);
      }
  }
}

}  // namespace pet
