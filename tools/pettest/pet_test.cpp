// Checks Pet's parts and saved look on a computer (no board needed):
//   c++ -std=c++17 -O2 -I src/apps/pet tools/pettest/pet_test.cpp -o /tmp/pet_test && /tmp/pet_test
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "pet_logic.h"

using namespace pet;
static int failures;

static void check(bool ok, const char *what) {
  printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
  failures += !ok;
}

static bool partOk(const PetPart &p) {
  if (!p.name || strlen(p.name) == 0 || strlen(p.name) > 10) return false;
  for (int y = 0; y < SIZE; y++) {
    if (!p.rows[y] || strlen(p.rows[y]) != SIZE) return false;
    for (int x = 0; x < SIZE; x++)
      if (!strchr("#o.", p.rows[y][x])) return false;
  }
  return true;
}

static bool blank(const PetPart &p) {
  for (int y = 0; y < SIZE; y++)
    for (int x = 0; x < SIZE; x++)
      if (p.rows[y][x] != '.') return false;
  return true;
}

// The look drawn into a 32x32 grid (1 = ink).
static void render(const Look &l, bool closed, uint8_t out[SIZE][SIZE]) {
  memset(out, 0, SIZE * SIZE);
  drawLook(l, closed, [&](int x, int y, bool ink) { out[y][x] = ink; });
}

int main() {
  bool partsOk = true, fits = true;
  for (int layer = 0; layer < LAYERS; layer++) {
    const LayerParts lp = partsOf(layer);
    fits &= lp.count >= 1 && lp.count <= 16;  // 4 bits a layer
    for (int i = 0; i < lp.count; i++)
      if (!partOk(lp.parts[i])) {
        printf("       bad part: %s %d (%s)\n", LAYER_NAMES[layer], i, lp.parts[i].name ? lp.parts[i].name : "?");
        partsOk = false;
      }
  }
  check(partsOk && partOk(EYES_CLOSED), "every part: a name of 1-10 characters, 32 rows of 32 '#', 'o' or '.'");
  check(fits, "every layer has 1 to 16 parts, so it fits in 4 bits");
  check(blank(HATS[0]) && blank(EXTRAS[0]), "the first hat and extra are 'None' (nothing drawn)");
  check(!blank(BODIES[0]) && !blank(EYES[0]) && !blank(MOUTHS[0]), "the default body, eyes and mouth draw something");

  bool roundTrip = true;
  for (int layer = 0; layer < LAYERS; layer++)
    for (int i = 0; i < partsOf(layer).count; i++) {
      Look l = DEFAULT_LOOK;
      l.part[layer] = i;
      const Look back = unpack(pack(l));
      roundTrip &= memcmp(back.part, l.part, LAYERS) == 0;
    }
  check(roundTrip, "every part survives saving and loading");

  const Look bad = unpack(0xFFFFFFFF);
  bool fellBack = true;
  for (int layer = 0; layer < LAYERS; layer++)
    fellBack &= bad.part[layer] == (partsOf(layer).count == 16 ? 15 : DEFAULT_LOOK.part[layer]);
  check(fellBack, "a damaged saved value falls back to the default part per layer");
  check(memcmp(unpack(pack(DEFAULT_LOOK)).part, DEFAULT_LOOK.part, LAYERS) == 0, "the default look round-trips");

  Look w = DEFAULT_LOOK;
  stepPart(w, HAT, -1);
  const bool wrapBack = w.part[HAT] == partsOf(HAT).count - 1;
  stepPart(w, HAT, 1);
  check(wrapBack && w.part[HAT] == 0, "stepping past either end wraps around");

  uint32_t seed = 12345;
  bool randOk = true;
  for (int i = 0; i < 1000; i++) {
    const Look r = randomLook([&] { return seed = seed * 1664525u + 1013904223u; });
    for (int layer = 0; layer < LAYERS; layer++) randOk &= r.part[layer] < partsOf(layer).count;
  }
  check(randOk, "random looks only use parts that exist");

  // Layers combine: a hat changes the picture, paper ('o') can clear ink below, closed eyes differ from open.
  uint8_t a[SIZE][SIZE], b[SIZE][SIZE];
  Look withHat = DEFAULT_LOOK;
  withHat.part[HAT] = partsOf(HAT).count > 1 ? 1 : 0;
  render(DEFAULT_LOOK, false, a);
  render(withHat, false, b);
  check(memcmp(a, b, sizeof a) != 0, "a hat changes the picture");
  render(DEFAULT_LOOK, true, b);
  check(memcmp(a, b, sizeof a) != 0, "a blink (closed eyes) changes the picture");

  printf(failures ? "\n%d check(s) failed\n" : "\nall checks passed\n", failures);
  return failures ? 1 : 0;
}
