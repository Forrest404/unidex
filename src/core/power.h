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
void powerNap();           // call at the end of loop(): light sleep until a button or the idle deadline
