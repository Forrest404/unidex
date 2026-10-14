#pragma once
#include <Arduino.h>
#include "../../core/app.h"

// Runs one third-party app's Lua code (docs/APPS.md) in a sandbox: only the safe parts of Lua plus the `unidex`
// table, in its own 512 KB of memory, and every call stopped after about 2 s. Any error (in the code, or a limit)
// closes the app; hostRunning() then turns false and hostError() says why, for the screen.
bool hostStart(const String &id, uint8_t permissions);  // loads main.lua and calls onEnter
void hostStop();                    // onExit, then closes it (WiFi off if it used it)
bool hostRunning();
const String &hostId();
const String &hostError();          // "main.lua:12: attempt to call a nil value", "took too long"...
void hostDraw(const char *fn = "draw");  // the app's draw() (a badge plugin's badge()) on a blank screen
Redraw hostEvent(const char *fn);   // onA, onB, onBHold, tick: what it asks for (None if it isn't defined)
bool hostHas(const char *fn);
Redraw hostBack();                  // onBack: Exit unless the app handles it
String hostStatus();                // status(), or ""
size_t hostMemoryUsed();
