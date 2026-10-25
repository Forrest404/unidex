// A battery that always reads about 3/4 full, on USB.
#include "../../src/core/battery.h"
static int fakeMv;
int batteryMillivolts() { return fakeMv ? fakeMv : 3950; }
int batteryPercent() { return 75; }
bool batteryCharging() { return true; }
const char *batteryTooLow() { return nullptr; }
bool batteryEmpty() { return false; }
#if UNIDEX_DEV
void batteryFake(int mv) { fakeMv = mv; }
bool batteryKept() { return false; }
void batteryPrintCalls() {}
#endif
