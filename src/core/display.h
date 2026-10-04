#pragma once
#include <GxEPD2_BW.h>

// 1.54" 200x200 SSD1681 panel. Inverting swaps black and white in the two calls every drawing goes
// through (text, lines, rects and bitmaps all end in drawPixel), so apps need no changes.
class Display : public GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT> {
 public:
  using GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT>::GxEPD2_BW;
  bool inverted = false;
#if UNIDEX_DEV
  // Test build: a copy of what the panel shows (1 = black), for the USB screenshot command.
  uint8_t shadow[200 * 200 / 8];
  void drawPixel(int16_t x, int16_t y, uint16_t color) override {
    color = ink(color);
    GxEPD2_BW::drawPixel(x, y, color);
    if (x < 0 || y < 0 || x >= 200 || y >= 200) return;
    const uint8_t bit = 0x80 >> (x & 7);
    if (color == GxEPD_BLACK) shadow[(y * 200 + x) >> 3] |= bit;
    else shadow[(y * 200 + x) >> 3] &= ~bit;
  }
  void fillScreen(uint16_t color) override {
    color = ink(color);
    GxEPD2_BW::fillScreen(color);
    memset(shadow, color == GxEPD_BLACK ? 0xFF : 0, sizeof shadow);
  }
#else
  void drawPixel(int16_t x, int16_t y, uint16_t color) override { GxEPD2_BW::drawPixel(x, y, ink(color)); }
  void fillScreen(uint16_t color) override { GxEPD2_BW::fillScreen(ink(color)); }
#endif

 private:
  uint16_t ink(uint16_t c) const { return inverted ? (c == GxEPD_WHITE ? GxEPD_BLACK : GxEPD_WHITE) : c; }
};
extern Display display;

// initial=false after a deep-sleep wake: the panel kept its image, so skip the clearing full refresh.
void displayInit(bool initial);
void displaySetInverted(bool on);  // saved in NVS; redraw with a full refresh after changing it

// Draws a whole screen with draw() and refreshes the panel, then hibernates it.
// Partial refresh normally; full refresh when asked (app switches) or every 10 partials,
// to clear e-ink ghosting.
void displayShow(void (*draw)(), bool full);

// Animation frame (the Chooser spin only): partial refresh, not counted toward the rule above and
// no hibernate. Always finish an animation with a full displayShow() to clear the ghosting.
void displayFrame(void (*draw)());

// Clock tick: a partial refresh that doesn't count toward the every-10 rule (only the minute changes,
// so there's nothing to ghost and no full flash every 10 minutes), then hibernate.
void displayTick(void (*draw)());

// The last refresh: 'F' full, 'P' partial, 'T' tick, 'f' animation frame, and how long it took (test build).
struct DisplayRefresh {
  char kind;
  uint32_t ms;
  uint32_t count;
};
DisplayRefresh displayLastRefresh();
