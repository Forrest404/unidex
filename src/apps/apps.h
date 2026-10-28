#pragma once
#include "../core/app.h"

extern const App *const APPS[];
extern const int APP_COUNT;
extern const App badgeApp;     // the launcher opens it when a badge arrives over USB
extern const App settingsApp;
void settingsShowChecks();     // Settings' checks after phone setup, then Exit (the first start's Done; launcherInterrupt it)
#if UNIDEX_DEV
const char *clockSay(const char *words);  // test build: Clock does what the words ask, as if said; what it would toast
#endif
