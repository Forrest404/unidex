// Checks how the Timetable reads timetable.csv and events.csv rows (src/apps/timetable/timetable_logic.h), on a
// computer:
//   c++ -std=c++11 -O2 -Wall -I src/apps/timetable tools/timetabletest/timetable_test.cpp -o /tmp/timetable_test && /tmp/timetable_test
#include <cstdio>
#include "timetable_logic.h"

using namespace timetable;
static int failures;

static void check(bool ok, const char *what) {
  printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
  failures += !ok;
}

static bool row(const char *line, bool dated, Row &r) { return parseRow(line, dated, r); }

int main() {
  Row r;
  // Times
  check(parseTime("09:05") == 545 && parseTime("9:05") == 545, "09:05 and 9:05");
  check(parseTime("24:00") == 1440, "24:00 is the end of the day");
  check(parseTime("ab:cd") < 0 && parseTime("9:5") < 0 && parseTime("25:00") < 0 && parseTime("12:60") < 0 &&
          parseTime("") < 0 && parseTime("12") < 0 && parseTime("1x:00") < 0,
        "not a time: ab:cd, 9:5, 25:00, 12:60, empty, 12, 1x:00");

  // Weekly rows
  check(row("Mon,09:00,10:00,Maths,B12", false, r) && r.wday == 1 && r.start == 540 && r.end == 600 &&
          r.module == "Maths" && r.room == "B12",
        "a plain weekly row");
  check(!row("day,start,end,module,room", false, r), "the header row is skipped");
  check(!row("Mon,ab:cd,10:00,Maths,B12", false, r), "a bad time is skipped, not read as 00:00");
  check(row("Fri,22:00,00:00,Late lab,Lab 1", false, r) && r.start == 1320 && r.end == 1440,
        "ending at 00:00: cut at midnight");
  check(row("Fri,23:00,01:00,Night shift,Hall", false, r) && r.start == 1380 && r.end == 1440,
        "crossing midnight: cut at midnight");
  check(row("Fri,22:00,24:00,Late,Hall", false, r) && r.end == 1440, "24:00 as the end");
  check(!row("Fri,12:00,12:00,Nothing,Hall", false, r), "no length is skipped");
  check(row("Tue,13:00,14:00,\"Maths, advanced\",\"Room 2, east\"", false, r) && r.module == "Maths, advanced" &&
          r.room == "Room 2, east",
        "quoted fields keep their commas");
  check(row("Tue,13:00,14:00,\"The \"\"Big\"\" one\",A1", false, r) && r.module == "The \"Big\" one",
        "a doubled quote inside quotes");
  check(row("  wednesday , 9:00 , 10:30 , Physics , Lab 3 ", false, r) && r.wday == 3 && r.start == 540 &&
          r.end == 630 && r.module == "Physics" && r.room == "Lab 3",
        "spaces and a full day name");

  // Dated rows (events.csv)
  check(row("2026-10-28,14:00,15:30,Maths,Room 4,Bring a calculator", true, r) && r.date >= 0 && r.wday == 3 &&
          r.notes == "Bring a calculator",
        "an event, 28 Oct 2026 is a Wednesday");
  check(row("2026-10-27,,,Inset day,,", true, r) && r.allDay && r.start == 0 && r.end == 1440, "an all-day event");
  check(row("2026-10-28,09:00,10:00,Talk,Hall,Notes, with a comma / and more", true, r) &&
          r.notes == "Notes, with a comma / and more",
        "the notes are the rest of the line");
  check(!row("2026-13-01,09:00,10:00,Bad,Hall,", true, r) && !row("20x6-10-01,09:00,10:00,Bad,Hall,", true, r),
        "a bad date is skipped");
  check(!row("Mon,09:00,10:00,Maths,B12", true, r), "a weekly row isn't an event");
  return failures ? 1 : 0;
}
