#pragma once
#include <stddef.h>
#include <string>
#include <vector>
#include "reminders_logic.h"

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

// For the agent. The reminders in the list's order: the ones not done yet, or (all) every one.
std::vector<reminders::Reminder> remindersList(bool all = false);
// Adds one (due as reminders_logic.h has it: "YYYY-MM-DD HH:MM", "YYYY-MM-DD" or ""). nullptr, or why not.
const char *remindersAdd(const std::string &title, const std::string &due);
// Ticks off the open reminder with exactly this title, or (remove) deletes it, done or not. nullptr, or why not.
const char *remindersFinish(const std::string &title, bool remove);
