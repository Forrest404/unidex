#pragma once
#include "input.h"

// Partial for small changes; Full when the whole image changes (a partial would leave a ghost).
enum class Redraw { None, Partial, Full };

// Every app is one of these. Keep app state in RTC_DATA_ATTR variables so it survives deep sleep.
// A long never reaches an app: the launcher uses it to go home.
struct App {
  const char *name;
  const char *const *icon;    // 40x40 pixel art from theme.h
  void (*onEnter)();
  Redraw (*onButton)(Event e);
  void (*draw)();             // draw the whole screen into the buffer; the launcher refreshes the panel
  void (*onExit)();
};
