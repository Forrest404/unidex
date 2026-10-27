#pragma once
#include <stddef.h>

// The reminders, for other apps (the Pet): what the home screen's line knows, kept through sleep. Times are local
// minutes from 1970, as in reminders_logic.h.
struct RemindersNext {
  int late;          // how many are past due and not done
  char lateTitle[32];  // the one that came due last ("" if none is late)
  long nextMin;      // the next one due (-1: none)
  bool nextHasTime;  // false: a date only (counts as 09:00)
  char nextTitle[32];
};

// False until the list has been read (from the card, once after a restart).
bool remindersNext(RemindersNext *out);
