#pragma once
// Pet's look: one part per layer, saved as one number (4 bits a layer), and drawn from the 32x32 parts in
// parts.h. No Arduino code, so it can be checked on a computer (tools/pettest).
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "parts.h"

namespace pet {

enum Layer { BODY, EYES_LAYER, MOUTH, HAT, EXTRA, LAYERS };
static const char *const LAYER_NAMES[LAYERS] = {"Body", "Eyes", "Mouth", "Hat", "Extra"};
static const int SIZE = 32;  // each part is SIZE x SIZE pixels
static const int HAPPY_EYES = 2;  // EYES[2], "Happy": shown while it hops

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

// Naming on the device, a letter at a time: the name is MAX_NAME places, blank (space) where unused. A cycles the
// letter in the current place through NAME_CHARS; B moves on, and finishes on a blank with only blanks after it
// (or on the last place).
static const int MAX_NAME = 12;
static const char NAME_CHARS[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-";

inline char stepChar(char c, int step) {
  const int n = sizeof NAME_CHARS - 1;
  const char *at = c ? strchr(NAME_CHARS, c) : nullptr;
  const int i = at ? (int)(at - NAME_CHARS) : 0;  // anything else counts as a blank
  return NAME_CHARS[((i + step) % n + n) % n];
}

inline bool nameDoneAt(const char *places, int pos) {
  if (pos >= MAX_NAME - 1) return true;
  for (int i = pos; i < MAX_NAME; i++)
    if (places[i] != ' ') return false;
  return true;
}

// The places as a name: no blanks at either end.
inline void nameFromPlaces(const char *places, char *out) {
  int start = 0, end = MAX_NAME;
  while (start < end && places[start] == ' ') start++;
  while (end > start && places[end - 1] == ' ') end--;
  memcpy(out, places + start, end - start);
  out[end - start] = 0;
}

// A name into places: printable letters kept, padded with blanks.
inline void placesFromName(const char *name, char *places) {
  int i = 0;
  for (; name[i] && i < MAX_NAME; i++) places[i] = name[i] >= 0x20 && name[i] <= 0x7E ? name[i] : ' ';
  for (; i < MAX_NAME; i++) places[i] = ' ';
  places[MAX_NAME] = 0;
}

}  // namespace pet
