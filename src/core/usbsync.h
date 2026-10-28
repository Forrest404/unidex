#pragma once
#include <stdint.h>
#include <time.h>
#include <string>
#include <vector>

// Receives the time and calendar events from the Mac agent (tools/calsync) over USB serial.
// Only works while the board is awake and plugged in.
void usbSyncPoll();               // call every loop
uint32_t usbSyncGeneration();     // goes up whenever /events.csv changes
time_t usbSyncLastTime();         // last complete sync, 0 if none since power-up
// Saves events lines (from the calendar link, Settings > Sync) as an E transfer would. False if the card can't.
bool usbSyncSaveEvents(const std::vector<std::string> &lines);
#include <stddef.h>
bool usbSyncTakeNewBadge(char *out, size_t len);  // a badge just arrived over USB (reported once)
