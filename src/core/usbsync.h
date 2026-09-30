#pragma once
#include <stdint.h>
#include <time.h>

// Receives the time and calendar events from the Mac agent (tools/calsync) over USB serial.
// Only works while the board is awake and plugged in.
void usbSyncPoll();               // call every loop
uint32_t usbSyncGeneration();     // goes up whenever /events.csv changes
time_t usbSyncLastTime();         // last complete sync, 0 if none since power-up
