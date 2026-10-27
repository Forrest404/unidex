#pragma once
#include <time.h>

// The next few classes and timed events, for other apps (the Pet). Kept through sleep, as the home screen's line
// is: worked out on each visit to the Timetable, read from the card once after a restart, and again once every
// one of them has ended.
struct TimetableSoon {
  char title[32], room[24];
  time_t start, end;
};

// Up to 3, soonest first (one on now comes first). 0 if there are none, or the clock isn't set.
int timetableSoon(TimetableSoon out[3]);

// When today's last timed class or event ends (or ended), 0 if there's none today (or it isn't known yet).
time_t timetableDayEnds();
