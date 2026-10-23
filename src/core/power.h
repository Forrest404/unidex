#pragma once
#include <stdint.h>
#include <time.h>

void powerInit();          // call first in setup()
bool powerWokeFromSleep();
void powerActivity();      // call on every input event to restart the idle timer
void powerSleepIfIdle();   // call from loop(); never returns if it sleeps
void powerSetSleepSeconds(int s);  // saved in NVS
int powerSleepSeconds();
void powerHold();     // stay awake (no light or deep sleep) until the matching powerRelease(): background work
void powerRelease();
#if UNIDEX_DEV
void powerSleepFor(uint32_t ms);  // test build: real deep sleep now, woken by the timer (RAM is lost, as in use)
#endif
// Deep sleep ends by `utc` (a reminder falling due); 0 = nothing to wake for. A long wait wakes early, as the sleep
// timer drifts: the launcher then finds nothing due yet and sleeps again at once, without touching the screen.
void powerWakeAt(time_t utc);
bool powerTimerWake();  // this start is a wake by that timer, not by a button
void powerSleepSoon();  // nothing to do on this wake: sleep at the next idle check, without the usual wait
void powerWakeWithin(uint32_t ms);  // the next light sleep ends within ms (an app's timed redraw, e.g. a blink)
void powerNap();
// A flat battery: "Charge me" on the screen (e-ink keeps it), then the power is cut so the cell isn't drained
// further; PWR turns it on again (and it switches straight off if still flat). Call often: it checks every 30 s.
void powerOffIfFlat();
bool powerFlatShown();  // "Charge me" is on the screen (the test build stays on after it)
// The loop watchdog: a main loop stuck for 30 s restarts the device. Long waits in the main loop (WiFi, recording,
// a Pet's visit) call powerAlive() as they go; from any other task it does nothing.
void powerWatchdogBegin();
void powerAlive();           // call at the end of loop(): light sleep until a button or the idle deadline
