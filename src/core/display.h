#pragma once
#include <GxEPD2_BW.h>

// 1.54" 200x200 SSD1681 panel. Inverting swaps black and white in the two calls every drawing goes
// through (text, lines, rects and bitmaps all end in drawPixel), so apps need no changes.
class Display : public GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT> {
 public:
  using GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT>::GxEPD2_BW;
  bool inverted = false;
  void drawPixel(int16_t x, int16_t y, uint16_t color) override { GxEPD2_BW::drawPixel(x, y, ink(color)); }
  void fillScreen(uint16_t color) override { GxEPD2_BW::fillScreen(ink(color)); }

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
