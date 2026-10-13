// No sleep, no battery switch and no watchdog in the browser: the demo stays awake.
#include "../../src/core/power.h"
#include "../../src/core/storage.h"
#include <Arduino.h>
static int sleepSecs = 10;
void powerInit() { sleepSecs = storageGetInt("sleep_s", 10); }
bool powerWokeFromSleep() { return false; }
void powerActivity() {}
void powerSleepIfIdle() {}
void powerSetSleepSeconds(int s) { sleepSecs = s; storagePutInt("sleep_s", s); }
int powerSleepSeconds() { return sleepSecs; }
void powerHold() {}
void powerRelease() {}
#if UNIDEX_DEV
void powerSleepFor(uint32_t) {}
#endif
void powerWakeWithin(uint32_t) {}
void powerNap() { delay(8); }  // lets the page run between passes
void powerOffIfFlat() {}
bool powerFlatShown() { return false; }
void powerWatchdogBegin() {}
void powerAlive() {}
