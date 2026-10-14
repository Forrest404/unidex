#pragma once
// The Apps app (Home > Apps): the third-party apps on the SD card, opened in the Lua host (luahost.h).
bool scriptsRunning(const char *id);  // that app is open on the device (the store mustn't replace it now)
void scriptsListChanged();            // the website changed /apps: read the list again
