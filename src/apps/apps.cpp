#include "apps.h"

// Launcher order. Adding an app: one new file in src/apps/<name>/ plus one line here.
extern const App timetableApp, badgeApp, dexApp, chooserApp, notesApp, gamesApp, settingsApp;

const App *const APPS[] = {&timetableApp, &notesApp, &badgeApp, &dexApp, &chooserApp, &gamesApp, &settingsApp};
const int APP_COUNT = sizeof(APPS) / sizeof(APPS[0]);
