// Checks how Reminders understands what was said ("call Sam tomorrow at 5pm") on a computer (no board needed):
//   c++ -std=c++11 -O2 -Wall -I src/apps/reminders tools/remindertest/parse_test.cpp -o /tmp/parse_test && /tmp/parse_test
//   /tmp/parse_test "remind me to ..."   (shows what one phrase becomes, said on Monday 19 Oct 2026 at 10:30)
#include <cstdio>
#include "reminders_logic.h"

using namespace reminders;
static int failures;

static long at(int y, int m, int d, int h, int mi) { return (long)daysFromCivil(y, m, d) * 1440 + h * 60 + mi; }

static void expect(long now, const char *said, const char *title, const char *due) {
  const Heard h = parseReminder(said, now);
  const bool ok = h.title == title && h.due == due;
  printf("%s  \"%s\" -> \"%s\" %s", ok ? "ok  " : "FAIL", said, h.title.c_str(), h.due.empty() ? "(no time)" : h.due.c_str());
  if (!ok) printf("   wanted \"%s\" %s", title, *due ? due : "(no time)");
  printf("\n");
  failures += !ok;
}

int main(int argc, char **argv) {
  const long mon = at(2026, 10, 19, 10, 30);  // Monday 19 October 2026, 10:30
  if (argc > 1) {
    const Heard h = parseReminder(argv[1], mon);
    printf("\"%s\" %s\n", h.title.c_str(), h.due.empty() ? "(no time)" : h.due.c_str());
    return 0;
  }

  // The opening and the title
  expect(mon, "Remind me to call Sam tomorrow at 5pm.", "Call Sam", "2026-10-20 17:00");
  expect(mon, "Buy milk.", "Buy milk", "");
  expect(mon, "remember to water the plants", "Water the plants", "");
  expect(mon, "I need to email Mr Jones", "Email Mr Jones", "");
  expect(mon, "Don't forget to bring the permission slip", "Bring the permission slip", "");
  expect(mon, "Call Sam, tomorrow at 5.", "Call Sam", "2026-10-20 17:00");
  expect(mon, "On Friday, hand in the essay.", "Hand in the essay", "2026-10-23");
  expect(mon, "Buy 3 apples and 2 pears", "Buy 3 apples and 2 pears", "");
  expect(mon, "Remind me tomorrow", "Reminder", "2026-10-20");
  expect(mon, "", "Reminder", "");

  // Times
  expect(mon, "call Sam at 5", "Call Sam", "2026-10-19 17:00");
  expect(mon, "call Sam at five", "Call Sam", "2026-10-19 17:00");
  expect(mon, "call Sam at 5 p.m.", "Call Sam", "2026-10-19 17:00");
  expect(mon, "call Sam at 5 P.M.", "Call Sam", "2026-10-19 17:00");
  expect(mon, "call Sam at 5:30pm", "Call Sam", "2026-10-19 17:30");
  expect(mon, "call Sam at 5.30", "Call Sam", "2026-10-19 17:30");
  expect(mon, "call Sam at five thirty", "Call Sam", "2026-10-19 17:30");
  expect(mon, "call Sam 17:45", "Call Sam", "2026-10-19 17:45");
  expect(mon, "call Sam at 08:15", "Call Sam", "2026-10-20 08:15");
  expect(mon, "call Sam at 11", "Call Sam", "2026-10-19 11:00");
  expect(mon, "call Sam at 9", "Call Sam", "2026-10-19 21:00");  // 9am has gone: tonight
  expect(mon, "call Sam at 9am", "Call Sam", "2026-10-20 09:00");  // said am: tomorrow's
  expect(mon, "call Sam at 6 o'clock", "Call Sam", "2026-10-19 18:00");
  expect(mon, "call Sam at noon", "Call Sam", "2026-10-19 12:00");
  expect(mon, "call Sam at midnight", "Call Sam", "2026-10-20 00:00");
  expect(mon, "call Sam at half past 5", "Call Sam", "2026-10-19 17:30");
  expect(mon, "call Sam at quarter to six", "Call Sam", "2026-10-19 17:45");
  expect(mon, "call Sam at a quarter past 4", "Call Sam", "2026-10-19 16:15");
  expect(mon, "call Sam around 3", "Call Sam", "2026-10-19 15:00");
  expect(mon, "leave by 4pm", "Leave", "2026-10-19 16:00");
  expect(mon, "pay Jo 5.30", "Pay Jo 5.30", "");  // a number on its own isn't a time

  // Parts of the day
  expect(mon, "take the bins out tonight", "Take the bins out", "2026-10-19 20:00");
  expect(mon, "take the bins out tonight at 8", "Take the bins out", "2026-10-19 20:00");
  expect(mon, "call Sam this evening", "Call Sam", "2026-10-19 19:00");
  expect(mon, "call Sam tomorrow morning", "Call Sam", "2026-10-20 09:00");
  expect(mon, "call Sam tomorrow afternoon at 2", "Call Sam", "2026-10-20 14:00");
  expect(mon, "call Sam at 7 in the evening", "Call Sam", "2026-10-19 19:00");
  expect(mon, "call Sam at 7 in the morning", "Call Sam", "2026-10-20 07:00");
  expect(mon, "morning run", "Morning run", "");

  // Days
  expect(mon, "dentist today", "Dentist", "2026-10-19");
  expect(mon, "dentist tomorrow at 3", "Dentist", "2026-10-20 15:00");
  expect(mon, "dentist on Wednesday", "Dentist", "2026-10-21");
  expect(mon, "dentist next Wednesday at 10am", "Dentist", "2026-10-21 10:00");
  expect(mon, "dentist on Monday", "Dentist", "2026-10-26");  // said on a Monday: next week's
  expect(mon, "dentist Friday evening", "Dentist", "2026-10-23 19:00");
  expect(mon, "bins out every Monday", "Bins out every Monday", "");
  expect(mon, "dentist on the 3rd", "Dentist", "2026-11-03");  // the 3rd has gone this month
  expect(mon, "dentist on the 25th", "Dentist", "2026-10-25");
  expect(mon, "dentist on 3 November", "Dentist", "2026-11-03");
  expect(mon, "dentist on the 3rd of November at 4", "Dentist", "2026-11-03 16:00");
  expect(mon, "dentist November 3rd", "Dentist", "2026-11-03");
  expect(mon, "dentist on May 2nd", "Dentist", "2027-05-02");
  expect(mon, "it may 2 rain", "It may 2 rain", "");
  expect(mon, "dentist on the 1st of January", "Dentist", "2027-01-01");
  expect(mon, "clean the car at the weekend", "Clean the car", "2026-10-24");
  expect(mon, "book flights next week", "Book flights", "2026-10-26");

  // From now
  expect(mon, "take the cake out in 20 minutes", "Take the cake out", "2026-10-19 10:50");
  expect(mon, "check the oven in an hour", "Check the oven", "2026-10-19 11:30");
  expect(mon, "check the oven in half an hour", "Check the oven", "2026-10-19 11:00");
  expect(mon, "move the car in 2 hours time", "Move the car", "2026-10-19 12:30");
  expect(mon, "move the car in an hour and a half", "Move the car", "2026-10-19 12:00");
  expect(mon, "water the plants in 3 days", "Water the plants", "2026-10-22");
  expect(mon, "put it in the bin", "Put it in the bin", "");

  // Edges of the week, month and year
  const long friLate = at(2026, 10, 23, 23, 50), monthEnd = at(2026, 10, 31, 12, 0), yearEnd = at(2026, 12, 31, 18, 0);
  expect(friLate, "call Sam at 9", "Call Sam", "2026-10-24 09:00");  // both 9s have gone: tomorrow morning's
  expect(friLate, "call Sam tomorrow", "Call Sam", "2026-10-24");
  expect(friLate, "call Sam on Monday", "Call Sam", "2026-10-26");
  expect(friLate, "call Sam in 20 minutes", "Call Sam", "2026-10-24 00:10");
  expect(monthEnd, "call Sam tomorrow", "Call Sam", "2026-11-01");
  expect(monthEnd, "call Sam on the 2nd", "Call Sam", "2026-11-02");
  expect(yearEnd, "call Sam tomorrow at 10am", "Call Sam", "2027-01-01 10:00");
  expect(yearEnd, "call Sam on the 5th", "Call Sam", "2027-01-05");
  expect(yearEnd, "call Sam in 3 days", "Call Sam", "2027-01-03");

  // No clock set: the words are kept, the time can't be worked out
  expect(-1, "call Sam tomorrow at 5", "Call Sam", "");

  printf(failures ? "\n%d failed\n" : "\nall passed\n", failures);
  return failures != 0;
}
