#include "apps.h"

// Launcher order. Adding an app: one new file in src/apps/<name>/ plus one line here.
extern const App timetableApp, badgeApp, chooserApp, notesApp, remindersApp, gamesApp, petApp, scriptsApp,
    settingsApp;

const App *const APPS[] = {&timetableApp, &notesApp, &remindersApp, &petApp,     &badgeApp,
                           &chooserApp,   &gamesApp, &scriptsApp,   &settingsApp};
const int APP_COUNT = sizeof(APPS) / sizeof(APPS[0]);
