#pragma once
#include "input.h"

// Every app is one of these. Keep app state in RTC_DATA_ATTR variables so it survives deep sleep.
// A long never reaches an app: the launcher uses it to go home.
struct App {
  const char *name;
  const char *const *icon;    // 40x40 pixel art from theme.h
  void (*onEnter)();
  bool (*onButton)(Event e);  // return true if the screen needs redrawing
  void (*draw)();             // draw the whole screen into the buffer; the launcher refreshes the panel
  void (*onExit)();
};
