#pragma once
#include <Arduino.h>

// Badge files: 1-bit BMPs in /badges on the SD card, shared by the Badge app, the USB upload (usbsync.cpp) and
// the Pet sending badges to friends (meet.cpp).
static const char *const BADGE_DIR = "/badges";
static const size_t BADGE_MAX_BYTES = 16384;  // a 200x200 1-bit BMP is 5662

// A 1-bit BMP in memory: from a file (it owns the bytes) or from bytes received (it doesn't).
struct Bmp {
  const uint8_t *data = nullptr;
  size_t size = 0;
  int32_t w = 0, h = 0, x0 = 0, y0 = 0;  // size, and the offset that centres it in the 200x200 frame
  uint32_t pixels = 0, stride = 0;
  bool topDown = false, zeroIsBlack = true;
  uint8_t *owned = nullptr;
  Bmp() = default;
  Bmp(const Bmp &) = delete;
  ~Bmp() { free(owned); }
  bool black(int32_t x, int32_t y) const;  // black at (x, y) of the 200x200 frame?
};

int badgeList(String *names, int max);  // the badges' file names, sorted (not macOS "._" files); 0 with no card
bool badgeNameOk(const char *name);     // lower-case letters, digits and dashes, ending in .bmp, under 32 chars
bool bmpCheck(const uint8_t *data, size_t size, Bmp &b);  // an uncompressed 1-bit BMP up to 200x200 (either palette)
bool badgeLoad(const char *name, Bmp &b);                 // read from /badges and checked
void badgeDraw(const Bmp &b);                              // the whole 200x200 frame
String badgeFreeName(const char *name);  // `name`, or "name-2.bmp", "name-3.bmp"... if it's taken
bool badgeSave(const char *name, const uint8_t *data, size_t size);  // written to a temp file, then renamed
