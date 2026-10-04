#pragma once
#include <stddef.h>
#include "input.h"

// Partial for small changes; Full when the whole image changes (a partial would leave a ghost).
// Tick: a partial that doesn't count toward the periodic full refresh (a clock or progress moving on).
// Exit: from onBack only, leave the app for the home screen.
enum class Redraw { None, Partial, Full, Tick, Exit };

// Every app is one of these. Keep app state in RTC_DATA_ATTR variables so it survives deep sleep.
// The buttons work the same everywhere: A = next, B = select, hold A = back one step (onBack), hold B = the
// screen's extra. Every screen shows its hints (drawHints in theme.h).
struct App {
  const char *name;
  const char *const *icon;    // 40x40 pixel art from theme.h
  void (*onEnter)();
  Redraw (*onButton)(Event e);  // A short, B short and B long; A long goes to onBack
  void (*draw)();             // draw the whole screen into the buffer; the launcher refreshes the panel
  void (*onExit)();
  Redraw (*onBack)();         // A long: go up one level, or return Exit at the top (nullptr = always Exit)
  void (*status)(char *out, size_t len);  // home screen line under the name; cached state only (no SD, no WiFi)
  Redraw (*tick)();           // called every loop while open, for live content; None when nothing changed
  bool needsCard;             // shows "No SD card" instead of opening without one
};
