#pragma once
#include "../core/app.h"

extern const App *const APPS[];
extern const int APP_COUNT;
extern const App badgeApp;     // the launcher opens it when a badge arrives over USB
extern const App timetableApp, dexApp;  // with badgeApp: need the SD card
extern const App settingsApp;  // opened by holding A on home; not on the grid
