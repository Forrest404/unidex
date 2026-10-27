// Checks what the Pet says about your day on a computer (no board needed):
//   c++ -std=c++17 -O2 -I src/apps/pet tools/pettest/talk_test.cpp -o /tmp/talk_test && /tmp/talk_test
#include <cstdio>
#include "talk_logic.h"

using namespace talk;
static int failures;

static void check(bool ok, const char *what) {
  printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
  failures += !ok;
}

static void says(const Agenda &a, const char *text, Mood mood, bool home, const char *what) {
  const Talk t = say(a);
  const bool ok = t.text == text && t.mood == mood && t.home == home;
  check(ok, what);
  if (!ok) printf("        got \"%s\" mood %d home %d\n", t.text.c_str(), t.mood, t.home);
}

static Agenda at(int hour, int min) {  // a Tuesday, clock set
  Agenda a;
  a.clock = true;
  a.minute = hour * 60 + min;
  a.wday = 2;
  return a;
}

static void add(Agenda &a, const char *title, const char *room, long start, long length) {
  a.events[a.eventCount++] = {title, room, start, start + length};
}

int main() {
  // Each rule
  Agenda a = at(14, 0);
  a.late = 1;
  a.lateTitle = "Call Sam";
  add(a, "Maths", "Room 2.04", 5, 60);
  says(a, "Don't forget: Call Sam", NORMAL, true, "a late reminder comes first, before a class in 5 min");
  a.late = 3;
  says(a, "Don't forget: Call Sam (+2)", NORMAL, true, "more than one late: the last one and how many more");

  a = at(10, 30);
  add(a, "Maths", "Room 2.04", -30, 90);
  add(a, "Lunch", "", 120, 60);
  says(a, "In Maths till 11:30", NORMAL, true, "a class on now, till its end (even in the morning)");

  a = at(14, 0);
  add(a, "Maths", "Room 2.04", 40, 60);
  a.reminderIn = 30;
  a.reminderTitle = "Call Sam";
  says(a, "Maths in 40 min, Room 2.04", NORMAL, true, "a class within the hour, with its room, before a reminder");

  a = at(14, 0);
  add(a, "Maths", "Room 2.04", 15, 60);
  says(a, "Maths in 15 min, Room 2.04", HAPPY, true, "15 min or less: excited");
  a.events[0].start = 16;
  says(a, "Maths in 16 min, Room 2.04", NORMAL, true, "16 min: calm");
  a.events[0].start = 60;
  says(a, "Maths in 60 min, Room 2.04", NORMAL, true, "exactly an hour away still counts");
  a.events[0].start = 61;
  a.events[0].room = "";
  says(a, "Free till 15:01", NORMAL, false, "61 min away: free till then");

  a = at(15, 0);
  add(a, "Gym", "", 0, 60);
  says(a, "In Gym till 16:00", NORMAL, true, "starting this minute is on now; no room");

  a = at(15, 0);
  a.reminderIn = 120;
  a.reminderTitle = "Call Sam";
  says(a, "Call Sam at 5pm", NORMAL, true, "a reminder within 2 hours");
  a.reminderIn = 150;
  says(a, "", NORMAL, false, "2.5 hours away: not yet, and nothing else to say");
  a.owner = "Sam";
  says(a, "Hi Sam!", NORMAL, false, "nothing on: hi, with your name");

  // Night and morning
  a = at(22, 0);
  add(a, "Maths", "", 11 * 60, 60);
  says(a, "Zzz", ASLEEP, false, "22:00: asleep");
  a = at(21, 59);
  add(a, "Maths", "", 11 * 60 + 1, 60);
  says(a, "Free till tomorrow 9:00", NORMAL, false, "21:59: awake, free till tomorrow");
  a = at(6, 59);
  says(a, "Zzz", ASLEEP, false, "06:59: asleep");
  a = at(23, 30);
  add(a, "Party", "", -30, 120);
  says(a, "In Party till 1:00", NORMAL, true, "something on at night wakes it; the end is past midnight");
  a = at(2, 0);
  a.late = 1;
  a.lateTitle = "Bins";
  says(a, "Don't forget: Bins", NORMAL, true, "a late reminder at night too");

  a = at(7, 0);
  a.owner = "Sam";
  add(a, "Maths", "", 120, 60);
  says(a, "Morning Sam! Maths at 9:00", NORMAL, false, "07:00: good morning, and the first thing today");
  a = at(10, 59);
  add(a, "Maths", "", 14 * 60, 60);
  says(a, "Morning!", NORMAL, false, "the next one is tomorrow, no name: just good morning");
  a = at(11, 0);
  add(a, "Maths", "", 3 * 60, 60);
  says(a, "Free till 14:00", NORMAL, false, "11:00: no longer morning, free till later today");

  a = at(18, 0);
  add(a, "Maths", "", 3 * 1440 - 9 * 60, 60);  // Friday 09:00
  says(a, "Free till Fri 9:00", NORMAL, false, "free till a day later in the week");

  // Without the clock
  Agenda none;
  says(none, "", NORMAL, false, "no clock and no name: nothing");
  none.owner = "Sam";
  none.late = 2;
  says(none, "Hi Sam!", NORMAL, false, "no clock: only hi, never the schedule");

  a = at(14, 0);
  add(a, "Digital Cultures and Society -", "Room 414 Marshgate", 10, 120);
  says(a, "Digital Cultures and Society - in 10 min, Room 414 Marshgate", HAPPY, true,
       "a long title is said in full (the screen wraps or cuts it)");

  // Something that just happened in another app
  a = at(14, 0);
  a.notice = "New best: 42!";
  a.noticePose = CHEER;
  a.reminderIn = 30;
  a.reminderTitle = "Call Sam";
  says(a, "New best: 42!", HAPPY, false, "a notice comes before a reminder within 2 hours, and not on the home line");
  check(say(a).pose == CHEER, "with its pose");
  add(a, "Maths", "", 10, 60);
  says(a, "Maths in 10 min", HAPPY, true, "a class within the hour comes first");
  a = at(23, 0);
  a.notice = "Got it!";
  says(a, "Got it!", HAPPY, false, "at night it wakes up for it");

  // Poses
  a = at(9, 0);
  add(a, "Maths", "B12", 10, 60);
  check(say(a).pose == BACKPACK, "within 15 min: the backpack");
  a.events[0].start = 20;
  check(say(a).pose == NO_POSE, "20 min away: no backpack yet");
  a.events[0].start = -5;
  check(say(a).pose == WAVE, "on now: a wave");
  a = at(15, 10);
  a.dayDoneAgo = 10;
  add(a, "Maths", "", 18 * 60, 60);
  const Talk done = say(a);
  check(done.text == "Done for today!" && done.pose == CHEER && done.mood == HAPPY && done.home, "10 min after the last class: done for today");
  a.dayDoneAgo = 31;
  check(say(a).text == "Free till tomorrow 9:10", "31 min after: free till tomorrow");
  a.dayDoneAgo = 10;
  a.reminderIn = 30;
  a.reminderTitle = "Call Sam";
  check(say(a).text == "Call Sam at 3:40pm", "a reminder within 2 hours comes before done for today");

  printf(failures ? "\n%d check(s) failed\n" : "\nall checks passed\n", failures);
  return failures ? 1 : 0;
}
