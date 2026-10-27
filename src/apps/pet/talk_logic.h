#pragma once
// What the Pet says about your day, from your timetable, calendar and reminders: one thing at a time, the first
// that applies of: a late reminder, a class on now, one within the hour, a reminder within 2 hours, the day's
// classes just over, asleep at night, good morning, then when you're free. Some come with a little picture beside
// it (a pose): its backpack just before a class, a wave during one, confetti when they're over. No Arduino code, so
// it can be checked on a computer (tools/pettest).
#include <stdio.h>
#include <string>
#include "../reminders/reminders_logic.h"

namespace talk {

enum Mood { NORMAL, HAPPY, ASLEEP };
enum Pose { NO_POSE, BACKPACK, WAVE, CHEER };

struct Event {
  std::string title, room;
  long start, end;  // minutes from now; a start of 0 or less is on now
};

struct Agenda {
  bool clock = false;  // the clock is set; without it, nothing about the day
  int minute = 0;      // now, in minutes since local midnight
  int wday = 0;        // today, 0 = Sunday
  Event events[3];     // the next ones, soonest first (one on now comes first)
  int eventCount = 0;
  int late = 0;               // reminders past due
  std::string lateTitle;      // the one that came due last
  long reminderIn = -1;       // minutes from now to the next reminder with a time (-1: none)
  std::string reminderTitle;
  long dayDoneAgo = -1;  // minutes since today's last class or event ended (-1: none today, or not yet)
  std::string owner;  // your name ("" if not set)
};

struct Talk {
  std::string text;  // "" = nothing to say
  Mood mood = NORMAL;
  Pose pose = NO_POSE;
  bool home = false;  // worth the home screen's line (it's about now)
};

static const long SOON = 60, VERY_SOON = 15, REMINDER_SOON = 120, DAY_DONE_FOR = 30;
static const int NIGHT_FROM = 22 * 60, MORNING_FROM = 7 * 60, MORNING_TO = 11 * 60;

inline std::string clockText24(long minutes) {  // "9:00", "14:30"
  minutes = ((minutes % 1440) + 1440) % 1440;
  char out[8];
  snprintf(out, sizeof out, "%ld:%02ld", minutes / 60, minutes % 60);
  return out;
}

inline std::string hello(const char *word, const std::string &owner) {  // "Hi Sam!", "Morning!"
  return std::string(word) + (owner.empty() ? "" : " " + owner) + "!";
}

inline Talk say(const Agenda &a) {
  Talk t;
  if (!a.clock) {
    t.text = a.owner.empty() ? "" : hello("Hi", a.owner);
    return t;
  }
  t.home = true;
  if (a.late > 0) {
    t.text = "Don't forget: " + a.lateTitle;
    if (a.late > 1) t.text += " (+" + std::to_string(a.late - 1) + ")";
    return t;
  }
  const Event *next = a.eventCount ? &a.events[0] : nullptr;
  if (next && next->start <= 0) {
    t.text = "In " + next->title + " till " + clockText24(a.minute + next->end);
    t.pose = WAVE;  // see you after
    return t;
  }
  if (next && next->start <= SOON) {
    t.text = next->title + " in " + std::to_string(next->start) + " min";
    if (!next->room.empty()) t.text += ", " + next->room;
    if (next->start <= VERY_SOON) t.mood = HAPPY, t.pose = BACKPACK;  // ready to go
    return t;
  }
  if (a.reminderIn >= 0 && a.reminderIn <= REMINDER_SOON) {
    t.text = a.reminderTitle + " at " + reminders::clockText((int)((a.minute + a.reminderIn) % 1440));
    return t;
  }
  if (a.dayDoneAgo >= 0 && a.dayDoneAgo <= DAY_DONE_FOR) {
    t.text = "Done for today!";
    t.mood = HAPPY;
    t.pose = CHEER;
    return t;
  }
  t.home = false;
  if (a.minute >= NIGHT_FROM || a.minute < MORNING_FROM) {
    t.text = "Zzz";
    t.mood = ASLEEP;
    return t;
  }
  const long laterToday = next ? (a.minute + next->start) : -1;  // minutes since midnight today
  if (a.minute < MORNING_TO) {
    t.text = hello("Morning", a.owner);
    if (next && laterToday < 1440) t.text += " " + next->title + " at " + clockText24(laterToday);
    return t;
  }
  if (next) {
    const long days = laterToday / 1440;
    t.text = "Free till ";
    if (days == 1) t.text += "tomorrow ";
    else if (days > 1) t.text += std::string(reminders::DAY_SHORT[(a.wday + days) % 7]) + " ";
    t.text += clockText24(laterToday);
    return t;
  }
  t.text = a.owner.empty() ? "" : hello("Hi", a.owner);
  return t;
}

}  // namespace talk
