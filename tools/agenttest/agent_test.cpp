// Checks the agent's rules on a computer: the actions a model's answer asks for, which are refused, which ask first,
// the words shown, and what the device tells the model.
//   c++ -std=c++17 -O2 -I src tools/agenttest/agent_test.cpp -o /tmp/agent_test && /tmp/agent_test
#include <cstdio>
#include "apps/agent/agent_logic.h"
#include "apps/timetable/timetable_logic.h"

using namespace agent;
static int failures;

static void check(bool ok, const char *what) {
  printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
  failures += !ok;
}

static Raw raw(const char *kind, std::vector<std::pair<std::string, std::string>> fields = {}) {
  return Raw{kind, fields};
}

static bool ok(const Raw &r, Action &a) {
  std::string why;
  return validate(r, a, why);
}

static std::string refused(const Raw &r) {
  Action a;
  std::string why;
  return validate(r, a, why) ? "" : why;
}

int main() {
  Action a;
  std::string why;

  // --- timers, alarms, the stopwatch ---
  check(ok(raw("timer", {{"seconds", "600"}}), a) && a.kind == TIMER && a.seconds == 600, "a timer in seconds");
  check(ok(raw("Timer", {{"minutes", "1.5"}}), a) && a.seconds == 90,
        "or in minutes (any case)");
  check(!refused(raw("timer", {{"seconds", "0"}})).empty() && !refused(raw("timer", {{"seconds", "90000"}})).empty() &&
            !refused(raw("timer", {{"seconds", "ten"}})).empty() && !refused(raw("timer")).empty(),
        "a timer of nothing, over a day, or not a number is refused");
  check(ok(raw("alarm", {{"time", "7:30"}, {"repeat", "weekdays"}}), a) && a.hour == 7 && a.minute == 30 &&
            a.repeat == clocks::WEEKDAYS,
        "an alarm with a repeat");
  check(ok(raw("alarm", {{"time", "18:05"}}), a) && a.repeat == clocks::ONCE && a.hour == 18, "once by default");
  check(!refused(raw("alarm", {{"time", "25:00"}})).empty() && !refused(raw("alarm", {{"time", "7pm"}})).empty() &&
            !refused(raw("alarm", {{"time", "7:00"}, {"repeat", "fortnightly"}})).empty(),
        "a bad alarm time or repeat is refused");
  check(ok(raw("stopwatch", {{"action", "Start"}}), a) && a.text == "start" &&
            !refused(raw("stopwatch", {{"action", "lap"}})).empty(),
        "the stopwatch starts, stops and resets, nothing else");
  check(ok(raw("timer_stop"), a) && a.kind == TIMER_STOP && ok(raw("alarms_off"), a) && a.kind == ALARMS_OFF,
        "stopping the timer and all alarms need nothing more");

  // --- reminders and events ---
  check(ok(raw("reminder", {{"title", " Call Sam\n"}, {"due", "2026-10-29 17:00"}}), a) && a.text == "Call Sam" &&
            a.due == "2026-10-29 17:00",
        "a reminder with a time");
  check(ok(raw("reminder", {{"title", "Buy milk"}, {"due", ""}}), a) && a.due.empty() &&
            ok(raw("reminder", {{"title", "Bins"}, {"due", "2026-11-02"}}), a),
        "with no date, or a date only");
  check(!refused(raw("reminder", {{"title", ""}})).empty() &&
            !refused(raw("reminder", {{"title", "x"}, {"due", "tomorrow"}})).empty(),
        "a reminder with no words or a date that doesn't read is refused");
  check(ok(raw("event", {{"title", "Football"}, {"date", "2026-10-31"}, {"start", "14:00"}, {"end", "15:30"}}), a) &&
            a.start == 840 && a.end == 930,
        "an event with times");
  check(ok(raw("event", {{"title", "Gig"}, {"date", "2026-11-01"}, {"start", "20:00"}}), a) && a.end == 21 * 60,
        "with no end: an hour");
  check(ok(raw("event", {{"title", "Late"}, {"date", "2026-11-01"}, {"start", "23:30"}}), a) && a.end == 1440,
        "but not past midnight");
  check(ok(raw("event", {{"title", "Holiday"}, {"date", "2026-12-24"}}), a) && a.start < 0,
        "with no times: all day");
  check(!refused(raw("event", {{"title", "X"}, {"date", "31/10/2026"}})).empty() &&
            !refused(raw("event", {{"date", "2026-10-31"}})).empty(),
        "an event without a proper date or a title is refused");
  check(eventLine(a) == "2026-12-24,,,Holiday,", "an all-day event's line");
  timetable::Row row;
  check(timetable::parseRow(eventLine(a), true, row) && row.allDay && row.module == "Holiday",
        "which the timetable reads as all day");
  ok(raw("event", {{"title", "Football"}, {"date", "2026-10-31"}, {"start", "14:00"}, {"end", "15:30"}}), a);
  check(timetable::parseRow(eventLine(a), true, row) && row.start == 840 && row.end == 930 && row.wday == 6,
        "and a timed one, on its day (a Saturday)");
  int y, m, d;
  timetable::civilFromDays(row.date, y, m, d);
  check(y == 2026 && m == 10 && d == 31, "a day count back to its date");
  ok(raw("event", {{"title", "Tea, cake"}, {"date", "2026-10-31"}, {"start", "9:00"}, {"location", "Sam's \"flat\""}}),
     a);
  check(eventLine(a) == "2026-10-31,9:00,10:00,\"Tea, cake\",\"Sam's \"\"flat\"\"\"", "commas and quotes are quoted");
  check(timetable::parseRow(eventLine(a), true, row) && row.module == "Tea, cake" && row.room == "Sam's \"flat\"",
        "and read back as they were");

  // --- apps and settings ---
  check(ok(raw("open", {{"app", "Games"}}), a) && a.text == "games" && !refused(raw("open", {{"app", "Mail"}})).empty(),
        "an app on the device opens; one that isn't there is refused");
  check(ok(raw("setting", {{"name", "volume"}, {"value", "3"}}), a) && a.setting == VOLUME && a.value == 3 &&
            !refused(raw("setting", {{"name", "volume"}, {"value", "11"}})).empty(),
        "volume 0-10");
  check(ok(raw("setting", {{"name", "alert_sound"}, {"value", "off"}}), a) && a.value == 0 &&
            ok(raw("setting", {{"name", "dark_screen"}, {"value", "true"}}), a) && a.value == 1,
        "on and off settings");
  check(ok(raw("setting", {{"name", "ringtone"}, {"value", "bell"}}), a) && a.value == 1 &&
            !refused(raw("setting", {{"name", "ringtone"}, {"value", "Siren"}})).empty(),
        "a ringtone by name");
  check(ok(raw("setting", {{"name", "sleep_after"}, {"value", "30"}}), a) &&
            !refused(raw("setting", {{"name", "sleep_after"}, {"value", "45"}})).empty(),
        "sleep after 10, 20, 30 or 60 s only");
  check(!refused(raw("setting", {{"name", "wifi"}, {"value", "off"}})).empty() &&
            !refused(raw("send_email")).empty(),
        "a setting or an action it doesn't have is refused");

  // --- asking first, and the words ---
  int asks = 0;
  for (int k = 0; k < KIND_COUNT; k++) {
    Action x;
    x.kind = (Kind)k;
    asks += needsAsking(x);
  }
  ok(raw("setting", {{"name", "volume"}, {"value", "3"}}), a);
  check(asks == 3 && needsAsking(a), "only settings, deleting a reminder and turning alarms off ask first");
  check(confirmText(a) == "Volume 3?", "the question for a setting");
  ok(raw("reminder_delete", {{"title", "Call Sam"}}), a);
  check(confirmText(a) == "Delete reminder?" && confirmLine(a) == "Call Sam", "and for deleting, with which one");
  ok(raw("alarm", {{"time", "7:00"}, {"repeat", "weekdays"}}), a);
  check(doneText(a) == "Alarm 7:00 Weekdays", "what an alarm says it did");
  ok(raw("timer", {{"seconds", "600"}}), a);
  check(doneText(a) == "Timer 10:00", "and a timer");

  // --- which reminder ---
  const std::vector<std::string> titles = {"Call Sam", "Hand in the art project", "Buy milk and eggs"};
  check(matchTitle(titles, "the art project") == 1 && matchTitle(titles, "buy milk") == 2 &&
            matchTitle(titles, "call sam") == 0,
        "a reminder is found by its words");
  check(matchTitle(titles, "walk the dog") == -1, "and none when no words match");
  check(matchTitle({"Buy bread"}, "bought milk") == -1 && matchTitle({"Buy bread and eggs"}, "buy milk") == -1,
        "one word in common isn't enough");
  check(!refused(raw("timer", {{"seconds", "-1e12"}})).empty(), "a negative timer is refused");

  // --- what the model is told ---
  State s;
  s.now = "Thursday 2026-10-29 14:05";
  s.owner = "Sam";
  s.soon = {"Maths, today 15:00-16:00, B12"};
  s.clock = "no timer; alarm 1 7:00 Weekdays on";
  s.settings = "volume 5";
  const std::string c = context(s);
  check(c.find("Now: Thursday 2026-10-29 14:05") == 0 && c.find("- Maths") != std::string::npos &&
            c.find("Open reminders: none") != std::string::npos && c.find("name: Sam") != std::string::npos,
        "the device's state, as lines");
  State none;
  check(context(none).find("clock isn't set") != std::string::npos, "an unset clock is said");
  int listed = 0;
  for (const char *k : KIND_NAMES) listed += std::string(PROMPT).find(std::string("\"do\":\"") + k + "\"") != std::string::npos;
  check(listed == KIND_COUNT, "the instructions show every action");

  printf(failures ? "\n%d FAILED\n" : "\nall ok\n", failures);
  return failures ? 1 : 0;
}
