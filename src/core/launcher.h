#pragma once
#include "input.h"

void launcherBegin(bool woke);  // cold boot: splash + home; wake: nothing (the screen holds)
void launcherHandle(Event e);
void launcherPoll();  // call every loop: live home line and clock, app ticks, toasts, a badge arriving over USB
void launcherToast(const char *text);  // a short message on the next redraw, gone after a few seconds
void systemRestart(const char *why);   // "Restarting" screen (with why, if any), then a fresh boot
const char *launcherScreenName();      // "Home", "No card" or the open app's name (for the test tools)
const char *launcherSelectedName();    // the app highlighted on the home screen
