#pragma once
#include "input.h"

void launcherBegin(bool woke);  // cold boot: splash + home; wake: nothing (the screen holds)
void launcherHandle(Event e);
void launcherPoll();  // call every loop: live home line and clock, app ticks, toasts, a badge arriving over USB
void launcherShowWelcome();  // the first-start screens (the buttons, the website), as on a new device
void launcherToast(const char *text);
const char *launcherToastText();       // the message showing now, or "" (for the test tools)  // a short message on the next redraw, gone after a few seconds
void systemRestart(const char *why);   // "Restarting" screen (with why, if any), then a fresh boot
const char *launcherScreenName();      // "Home", "No card", "Welcome" or the open app's name (for the test tools)
const char *launcherSelectedName();    // the app highlighted on the home screen
