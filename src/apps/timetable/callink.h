#pragma once
#include <stddef.h>

// Settings > Sync: the clock from the internet, then the Timetable's events from the calendar's private link
// (cal_url, saved on the Setup page) instead of over USB. The download goes to the SD card first, so a big
// calendar never has to fit in memory.

struct SyncResult {
  const char *clock;     // nullptr = set, else why not (then the calendar isn't tried)
  bool link;             // a calendar link is saved
  const char *calendar;  // nullptr = saved, else why not
  int events;            // this week's events, when saved
};

// Connects, sets the clock, and fetches the calendar if a link is saved. Blocks (tens of seconds for a big
// calendar).
SyncResult calendarSync();

// The link's calendar read without saving (the Setup page's Test). WiFi must be up. nullptr or the reason.
const char *calendarCheck(int &events);
