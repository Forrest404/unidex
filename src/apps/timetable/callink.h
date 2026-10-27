#pragma once
#include <stddef.h>

// Settings > Sync: the clock from the internet, then the Timetable's events from the calendar's private link
// (cal_url, saved on the Setup page) instead of over USB. The download goes to the SD card first, so a big
// calendar never has to fit in memory.

// Connects, sets the clock, and fetches the calendar if a link is saved. Blocks (tens of seconds for a big
// calendar). `result` gets a line for the screen ("Clock set, 12 events"); nullptr on success, else the reason.
const char *calendarSync(char *result, size_t len);

// The link's calendar read without saving (the Setup page's Test). WiFi must be up. nullptr or the reason.
const char *calendarCheck(int &events);
