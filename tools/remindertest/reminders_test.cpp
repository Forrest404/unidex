// Checks Reminders' file lines, order and due tags on a computer (no board needed):
//   c++ -std=c++17 -O2 -I src/apps/reminders tools/remindertest/reminders_test.cpp -o /tmp/reminders_test && /tmp/reminders_test
#include <cstdio>
#include "reminders_logic.h"

using namespace reminders;
static int failures;

static void check(bool ok, const char *what) {
  printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
  failures += !ok;
}

static Reminder rem(const char *due, const char *title, bool done = false) {
  Reminder r;
  r.due = due;
  r.title = title;
  r.done = done;
  return r;
}

int main() {
  // Monday 19 October 2026, 10:30
  const long now = (long)daysFromCivil(2026, 10, 19) * 1440 + 10 * 60 + 30;
  int y, m, d;
  civilFromDays(daysFromCivil(2028, 2, 29), y, m, d);
  check(y == 2028 && m == 2 && d == 29, "dates round-trip (a leap day)");
  check(weekday(daysFromCivil(2026, 10, 19)) == 1, "19 Oct 2026 is a Monday");
  check(formatDue(daysFromCivil(2026, 12, 31) + 1, 9 * 60 + 5) == "2027-01-01 09:05", "a day past the year's end");

  Reminder r;
  check(fromLine("2026-10-20 17:00,0,Call Sam, then Jo", r) && r.due == "2026-10-20 17:00" && !r.done &&
            r.title == "Call Sam, then Jo" && r.hasTime(),
        "a line reads back, commas in the title kept");
  check(toLine(r) == "2026-10-20 17:00,0,Call Sam, then Jo", "and writes the same line");
  check(fromLine(",1,Buy milk\r", r) && r.due.empty() && r.done && r.title == "Buy milk", "undated, done, CRLF");
  check(fromLine("2026-13-40,0,Odd", r) && r.due.empty() && r.title == "Odd", "a damaged date is dropped, not the reminder");
  check(!fromLine("no commas", r) && !fromLine("2026-10-20,0,", r), "broken lines are skipped");

  std::vector<Reminder> list = {rem("", "Undated new"),  rem("2026-10-21", "Wed"),     rem("", "Undated old"),
                                rem("2026-10-19 09:00", "Late"), rem("2026-10-20 08:00", "Done", true),
                                rem("2026-10-19 17:00", "Today")};
  sortList(list);
  check(list[0].title == "Late" && list[1].title == "Today" && list[2].title == "Wed" && list[3].title == "Undated new" &&
            list[4].title == "Undated old" && list[5].title == "Done",
        "order: late, by time, undated newest first, done last");

  check(dueTag(rem("2026-10-19 09:00", ""), now) == "late", "a time already passed is late");
  check(dueTag(rem("2026-10-19", ""), now) == "today", "today with no time isn't late");
  check(dueTag(rem("2026-10-18", ""), now) == "late", "yesterday with no time is late");
  check(dueTag(rem("2026-10-19 17:00", ""), now) == "5pm", "later today: the time");
  check(dueTag(rem("2026-10-19 17:30", ""), now) == "5:30pm", "with minutes");
  check(dueTag(rem("2026-10-20 08:00", ""), now) == "tmrw 8am", "tomorrow");
  check(dueTag(rem("2026-10-23 12:00", ""), now) == "Fri 12pm", "this week: the day");
  check(dueTag(rem("2026-11-03", ""), now) == "3 Nov", "further: the date");
  check(dueTag(rem("2026-10-19 09:00", "", true), now) == "" && dueTag(rem("", ""), now) == "", "done or undated: no tag");
  check(dueWords(rem("2026-10-20 17:00", ""), now) == "tomorrow 5pm" && dueWords(rem("2026-11-03", ""), now) == "Tue 3 Nov",
        "said in full");

  std::vector<Reminder> due = {rem("2026-10-19 09:00", "Late"), rem("2026-10-21", "Wed"), rem("2026-10-19 17:00", "Today"),
                               rem("2026-10-19 11:00", "Done", true), rem("", "None")};
  check(nextDue(due, now) == 2, "the next one due skips late, done and undated ones");
  check(nextDue({rem("", "x")}, now) == -1, "nothing due");

  printf(failures ? "\n%d failed\n" : "\nall passed\n", failures);
  return failures != 0;
}
