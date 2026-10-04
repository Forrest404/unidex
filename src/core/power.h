#pragma once

void powerInit();          // call first in setup()
bool powerWokeFromSleep();
void powerActivity();      // call on every input event to restart the idle timer
void powerSleepIfIdle();   // call from loop(); never returns if it sleeps
void powerSetSleepSeconds(int s);  // saved in NVS
int powerSleepSeconds();
void powerNap();           // call at the end of loop(): light sleep until a button or the idle deadline
