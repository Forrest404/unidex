#pragma once
#include <Adafruit_GFX.h>

// The two colours drawing calls take (Adafruit GFX colours are 16-bit).
constexpr uint16_t BLACK = 0x0000, WHITE = 0xFFFF;

// 1.54" 200x200 e-paper panel with an SSD1681 controller. Drawing goes into a buffer in RAM (Adafruit GFX does
// the text, lines and bitmaps); the functions below send it to the panel. Inverting swaps black and white in
// the two calls every drawing goes through (text, lines, rects and bitmaps all end in drawPixel), so apps need
// no changes.
class Display : public Adafruit_GFX {
 public:
  static const int WIDTH = 200, HEIGHT = 200;
  Display() : Adafruit_GFX(WIDTH, HEIGHT) {}
  void drawPixel(int16_t x, int16_t y, uint16_t color) override;
  void fillScreen(uint16_t color) override;
  bool inverted = false;
  // The screen as the controller takes it: 1 bit a pixel, 1 = white, rows top to bottom, leftmost pixel in
  // the top bit.
  uint8_t buf[WIDTH * HEIGHT / 8];

 private:
  uint16_t ink(uint16_t c) const { return inverted ? (c == WHITE ? BLACK : WHITE) : c; }
};
extern Display display;

// initial=false after a deep-sleep wake: the panel kept its image, so skip the clearing full refresh.
void displayInit(bool initial);
void displaySetInverted(bool on);  // saved in NVS; redraw with a full refresh after changing it

// Draws a whole screen with draw() and refreshes the panel, then puts it to sleep.
// Partial refresh normally; full refresh when asked (app switches) or every 10 partials,
// to clear e-ink ghosting.
void displayShow(void (*draw)(), bool full);

// Animation frame (the Chooser spin and the games): partial refresh, not counted toward the rule above and
// no sleep. Always finish an animation with a full displayShow() to clear the ghosting.
void displayFrame(void (*draw)());

// Game frames use the fast waveform (true) or the panel's own (false). The next displayShow() puts the
// panel back to its own settings by itself.
void displayFastFrames(bool on);
#if UNIDEX_DEV
void displayFastWave(uint8_t frames, uint8_t rate);  // test build: try another waveform (X FTEST)
#endif
void displaySetSpiHz(uint32_t hz);  // SPI clock to the panel

// Clock tick: a partial refresh that doesn't count toward the every-10 rule (only the minute changes,
// so there's nothing to ghost and no full flash every 10 minutes), then sleep.
void displayTick(void (*draw)());

// The last refresh: 'F' full, 'P' partial, 'T' tick, 'f' animation frame, and how long it took (test build).
struct DisplayRefresh {
  char kind;
  uint32_t ms;
  uint32_t count;
  uint32_t busyMs;  // of which the panel was busy refreshing
};
DisplayRefresh displayLastRefresh();
