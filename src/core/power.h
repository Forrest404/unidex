#pragma once
#include <stdint.h>

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
