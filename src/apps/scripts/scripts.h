#pragma once
#include <Arduino.h>
// The Apps app (Home > Apps): the third-party apps on the SD card, opened in the Lua host (luahost.h).
bool scriptsRunning(const char *id);  // that app is open on the device (the store mustn't replace it now)
void scriptsListChanged();            // the website changed /apps: read the list again
// For Games (its games-category apps): open one here, or offer to offload or delete it; back to Games after.
void scriptsOpenForGames(const String &id);
void scriptsManageForGames(const String &id);
