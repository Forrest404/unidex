#pragma once
// The agent (hold B on the home screen, say what you want) without the screen or the network: the instructions the
// model gets, what the device tells it about itself, and the actions it asks for in its answer, each checked before
// anything is done (a bad time, an unknown setting or app, too many: skipped). Small, easy-to-undo actions happen
// at once; changing a setting, deleting or turning things off asks first. No Arduino code, so tools/agenttest checks
// it on a computer.
//
// The model answers with JSON only: {"say": "Timer set for 10 min", "actions": [{"do": "timer", "seconds": 600}]}.
// The device reads the JSON (ArduinoJson) into Raw actions, every value as text, and validate() turns each into an
// Action.
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>
#include "../../core/ringtones.h"
#include "../clock/clock_logic.h"
#include "../reminders/reminders_logic.h"

namespace agent {

enum Kind {
  TIMER, TIMER_STOP, ALARM, ALARMS_OFF, STOPWATCH, REMINDER, REMINDER_DONE, REMINDER_DELETE, EVENT, OPEN, SETTING,
  KIND_COUNT
};
static const char *const KIND_NAMES[] = {"timer",         "timer_stop",      "alarm", "alarms_off", "stopwatch", "reminder",
                                         "reminder_done", "reminder_delete", "event", "open",       "setting"};
enum Setting { VOLUME, ALERT_SOUND, RINGTONE, DARK_SCREEN, SLEEP_AFTER, SETTING_COUNT };
static const char *const SETTING_NAMES[] = {"volume", "alert_sound", "ringtone", "dark_screen", "sleep_after"};
static const char *const APP_NAMES[] = {"timetable", "notes", "reminders", "clock", "pet",
                                        "badge",     "games", "apps",      "settings"};
static const int APP_COUNT = sizeof APP_NAMES / sizeof *APP_NAMES;
static const int MAX_ACTIONS = 5, MAX_SAY = 200, MAX_TITLE = 120, MAX_EVENT_TITLE = 60;
static const long MAX_TIMER = 24 * 3600L;

// An action as the model wrote it: its "do", then its other fields, every value as text.
struct Raw {
  std::string kind;
  std::vector<std::pair<std::string, std::string>> fields;
  std::string get(const char *name) const {
    for (const auto &f : fields)
      if (f.first == name) return f.second;
    return "";
  }
};

struct Action {
  Kind kind = TIMER;
  long seconds = 0;                 // TIMER
  int hour = -1, minute = 0;        // ALARM
  clocks::Repeat repeat = clocks::ONCE;
  std::string text;                 // a title (REMINDER*, EVENT), the app (OPEN), start/stop/reset (STOPWATCH)
  std::string due;                  // REMINDER: "YYYY-MM-DD HH:MM", "YYYY-MM-DD" or ""
  std::string date, location;       // EVENT: "YYYY-MM-DD"
  int start = -1, end = -1;         // EVENT: minutes from midnight (-1, -1: all day)
  Setting setting = VOLUME;         // SETTING
  int value = 0;                    // SETTING: volume 0-10, 0/1, a ringtone, seconds
};

inline std::string lower(std::string s) {
  for (char &c : s) c = (char)tolower((unsigned char)c);
  return s;
}

inline std::string trim(const std::string &s) {
  const size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
  return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}

// One line of text, at most `max` characters: newlines and tabs become spaces.
inline std::string oneLine(const std::string &s, size_t max) {
  std::string out;
  for (char c : s) out += c == '\n' || c == '\r' || c == '\t' ? ' ' : c;
  out = trim(out);
  return out.size() > max ? trim(out.substr(0, max)) : out;
}

// A number, or false (with junk after it, or none).
inline bool real(const std::string &s, double &out) {
  const std::string t = trim(s);
  if (t.empty()) return false;
  char *end;
  out = strtod(t.c_str(), &end);
  return !*end && out == out;
}
inline bool number(const std::string &s, long &out) {  // rounded to a whole one
  double v;
  if (!real(s, v) || v > 1e9 || v < -1e9) return false;
  out = (long)(v < 0 ? v - 0.5 : v + 0.5);
  return true;
}

// "7:05", "07:05", "19:30" -> minutes from midnight; -1 if it isn't one.
inline int clockTime(const std::string &s) {
  int h, m;
  char extra;
  if (sscanf(trim(s).c_str(), "%d:%d%c", &h, &m, &extra) != 2) return -1;
  return h >= 0 && h < 24 && m >= 0 && m < 60 ? h * 60 + m : -1;
}

inline bool yesNo(const std::string &s, int &out) {
  const std::string v = lower(trim(s));
  if (v == "on" || v == "true" || v == "yes" || v == "1") return out = 1, true;
  if (v == "off" || v == "false" || v == "no" || v == "0") return out = 0, true;
  return false;
}

inline bool isDate(const std::string &s) { return s.size() == 10 && reminders::dueMinutes(s) >= 0; }

inline std::string hhmm(int minutes) {
  char out[8];
  snprintf(out, sizeof out, "%d:%02d", minutes / 60, minutes % 60);
  return out;
}

// Checks one action and fills `a`. False with `why` (short, for the screen) for one that can't be done.
inline bool validate(const Raw &r, Action &a, std::string &why) {
  a = Action();
  const std::string kind = lower(trim(r.kind));
  int k = 0;
  while (k < KIND_COUNT && kind != KIND_NAMES[k]) k++;
  if (k == KIND_COUNT) return why = "Can't do \"" + oneLine(r.kind, 20) + "\"", false;
  a.kind = (Kind)k;
  switch (a.kind) {
    case TIMER: {
      long s = 0;
      double v;
      if (real(r.get("seconds"), v) && v >= 0 && v < 1e9) s = (long)(v + 0.5);
      else if (real(r.get("minutes"), v) && v >= 0 && v < 1e7) s = (long)(v * 60 + 0.5);
      if (s < 1 || s > MAX_TIMER) return why = "A timer is 1 s to 24 h", false;
      a.seconds = s;
      return true;
    }
    case ALARM: {
      const int t = clockTime(r.get("time"));
      if (t < 0) return why = "Alarm needs a time", false;
      a.hour = t / 60, a.minute = t % 60;
      const std::string rep = lower(trim(r.get("repeat")));
      if (rep.empty() || rep == "once") a.repeat = clocks::ONCE;
      else if (rep == "daily" || rep == "every day") a.repeat = clocks::DAILY;
      else if (rep == "weekdays") a.repeat = clocks::WEEKDAYS;
      else if (rep == "weekends") a.repeat = clocks::WEEKENDS;
      else return why = "Alarm repeat?", false;
      return true;
    }
    case STOPWATCH:
      a.text = lower(trim(r.get("action")));
      if (a.text.empty()) a.text = lower(trim(r.get("command")));
      if (a.text != "start" && a.text != "stop" && a.text != "reset") return why = "Stopwatch: start or stop", false;
      return true;
    case REMINDER:
      a.text = oneLine(r.get("title"), MAX_TITLE);
      a.due = trim(r.get("due"));
      if (a.text.empty()) return why = "Reminder needs words", false;
      if (!a.due.empty() && reminders::dueMinutes(a.due) < 0) return why = "Reminder date?", false;
      return true;
    case REMINDER_DONE:
    case REMINDER_DELETE:
      a.text = oneLine(r.get("title"), MAX_TITLE);
      if (a.text.empty()) return why = "Which reminder?", false;
      return true;
    case EVENT: {
      a.text = oneLine(r.get("title"), MAX_EVENT_TITLE);
      a.date = trim(r.get("date"));
      a.location = oneLine(r.get("location"), 40);
      if (a.text.empty()) return why = "Event needs a title", false;
      if (!isDate(a.date)) return why = "Event needs a date", false;
      const std::string s = trim(r.get("start")), e = trim(r.get("end"));
      if (s.empty()) return true;  // all day
      a.start = clockTime(s);
      if (a.start < 0) return why = "Event time?", false;
      a.end = e.empty() ? -1 : clockTime(e);
      if (a.end <= a.start) a.end = a.start + 60 < 1440 ? a.start + 60 : 1440;  // an hour, unless it said
      return true;
    }
    case OPEN:
      a.text = lower(trim(r.get("app")));
      for (const char *n : APP_NAMES)
        if (a.text == n) return true;
      return why = "No app \"" + oneLine(r.get("app"), 16) + "\"", false;
    case SETTING: {
      const std::string name = lower(trim(r.get("name"))), value = r.get("value");
      int s = 0;
      while (s < SETTING_COUNT && name != SETTING_NAMES[s]) s++;
      if (s == SETTING_COUNT) return why = "No setting \"" + oneLine(r.get("name"), 16) + "\"", false;
      a.setting = (Setting)s;
      long n;
      switch (a.setting) {
        case VOLUME:
          if (!number(value, n) || n < 0 || n > 10) return why = "Volume is 0 to 10", false;
          a.value = n;
          return true;
        case ALERT_SOUND:
        case DARK_SCREEN:
          if (!yesNo(value, a.value)) return why = "On or off?", false;
          return true;
        case RINGTONE:
          for (int i = 0; i < ringtones::COUNT; i++)
            if (lower(trim(value)) == lower(ringtones::TUNES[i].name)) return a.value = i, true;
          return why = "No ringtone \"" + oneLine(value, 12) + "\"", false;
        case SLEEP_AFTER:
          if (!number(value, n) || (n != 10 && n != 20 && n != 30 && n != 60)) return why = "Sleep after 10-60 s", false;
          a.value = n;
          return true;
        default:
          return false;
      }
    }
    default:  // TIMER_STOP, ALARMS_OFF: nothing more to check
      return true;
  }
}

// Changing a setting, deleting or turning things off asks first; the rest is easy to undo and just happens.
inline bool needsAsking(const Action &a) {
  return a.kind == ALARMS_OFF || a.kind == REMINDER_DELETE || a.kind == SETTING;
}

inline std::string settingText(const Action &a) {
  switch (a.setting) {
    case VOLUME: return "Volume " + std::to_string(a.value);
    case ALERT_SOUND: return std::string("Alert sound ") + (a.value ? "on" : "off");
    case RINGTONE: return std::string("Ringtone ") + ringtones::TUNES[a.value].name;
    case DARK_SCREEN: return std::string("Dark screen ") + (a.value ? "on" : "off");
    case SLEEP_AFTER: return "Sleep after " + std::to_string(a.value) + " s";
    default: return "";
  }
}

// The question for one that asks first ("Volume 3?", "Delete reminder?"), and a line under it (which one).
inline std::string confirmText(const Action &a) {
  if (a.kind == ALARMS_OFF) return "Turn all alarms off?";
  if (a.kind == REMINDER_DELETE) return "Delete reminder?";
  return settingText(a) + "?";
}
inline std::string confirmLine(const Action &a) { return a.kind == REMINDER_DELETE ? a.text : ""; }

// What was done, for the list under the answer: "Timer 10 min", "Alarm 7:00 weekdays".
inline std::string doneText(const Action &a) {
  switch (a.kind) {
    case TIMER: return "Timer " + clocks::duration(a.seconds);
    case TIMER_STOP: return "Timer stopped";
    case ALARM: {
      std::string t = "Alarm " + clocks::hhmm(a.hour, a.minute);
      if (a.repeat != clocks::ONCE) t += std::string(" ") + clocks::REPEAT_NAMES[a.repeat];
      return t;
    }
    case ALARMS_OFF: return "Alarms off";
    case STOPWATCH: return a.text == "start" ? "Stopwatch going" : a.text == "stop" ? "Stopwatch stopped" : "Stopwatch reset";
    case REMINDER: return "Reminder: " + oneLine(a.text, 30);
    case REMINDER_DONE: return "Done: " + oneLine(a.text, 30);
    case REMINDER_DELETE: return "Deleted: " + oneLine(a.text, 30);
    case EVENT: {
      std::string t = "Event: " + oneLine(a.text, 24) + " " + a.date.substr(5);
      if (a.start >= 0) t += " " + hhmm(a.start);
      return t;
    }
    case OPEN: return "Open " + a.text;
    case SETTING: return settingText(a);
    default: return "";
  }
}

// The reminder the words mean: the one sharing the most words with them (any order, any case; little words like
// "the" don't count), as long as that's at least half of its words (one word in common, "buy", isn't enough to
// tick off "Buy bread" for "bought milk"), or -1.
inline std::vector<std::string> words(const std::string &s) {
  static const char *const LITTLE[] = {"the", "an", "and", "to", "in", "on", "at", "of", "for", "my", "me", "it", "is"};
  std::vector<std::string> out;
  std::string w;
  for (char c : s + " ") {
    if (isalnum((unsigned char)c)) {
      w += (char)tolower((unsigned char)c);
      continue;
    }
    bool little = w.size() < 2 && !(w.size() == 1 && isdigit((unsigned char)w[0]));
    for (const char *l : LITTLE) little |= w == l;
    if (!little) out.push_back(w);
    w.clear();
  }
  return out;
}
inline int matchTitle(const std::vector<std::string> &titles, const std::string &said) {
  const std::vector<std::string> want = words(said);
  int best = -1, bestScore = 0;
  for (size_t i = 0; i < titles.size(); i++) {
    if (lower(trim(titles[i])) == lower(trim(said))) return (int)i;
    int score = 0;
    const std::vector<std::string> has = words(titles[i]);
    for (const std::string &w : has)
      for (const std::string &x : want)
        if (w == x) score++;
    if (score * 2 >= (int)has.size() && score > bestScore) best = (int)i, bestScore = score;
  }
  return best;
}

// A line of the events file for one made here: "YYYY-MM-DD,start,end,title,location" (empty times: all day), the
// title and place in quotes when they hold a comma or a quote (timetable_logic.h splitCsv).
inline std::string csvField(const std::string &s) {
  if (s.find_first_of(",\"") == std::string::npos) return s;
  std::string out = "\"";
  for (char c : s) out += c == '"' ? std::string("\"\"") : std::string(1, c);
  return out + "\"";
}
inline std::string eventLine(const Action &a) {
  const std::string s = a.start >= 0 ? hhmm(a.start) : "", e = a.start >= 0 ? hhmm(a.end) : "";
  return a.date + "," + s + "," + e + "," + csvField(a.text) + "," + csvField(a.location);
}

// What the device tells the model about itself, as plain lines.
struct State {
  std::string now;                     // "Thursday 2026-10-29 14:05" ("" if the clock isn't set)
  std::string owner;                   // the owner's name, if given
  std::vector<std::string> soon;       // the next classes and events: "Maths, today 15:00-16:00, B12"
  std::vector<std::string> reminders;  // open ones: "Call Sam (due 2026-10-29 17:00)"
  std::string clock;                   // "timer 4 min left; alarm 7:00 weekdays on; stopwatch stopped"
  std::string settings;                // "volume 5, alert_sound on, ringtone Chime, dark_screen off, sleep_after 20"
};

inline std::string context(const State &s) {
  std::string t = "Now: " + (s.now.empty() ? std::string("unknown (the clock isn't set)") : s.now) + "\n";
  if (!s.owner.empty()) t += "The owner's name: " + s.owner + "\n";
  t += "Coming up:";
  if (s.soon.empty()) t += " nothing";
  for (const std::string &x : s.soon) t += "\n- " + x;
  t += "\nOpen reminders:";
  if (s.reminders.empty()) t += " none";
  for (const std::string &x : s.reminders) t += "\n- " + x;
  t += "\nClock: " + s.clock + "\nSettings: " + s.settings + "\n";
  return t;
}

// The instructions: what it can do and how to answer. The device's state follows them (context()).
static const char *const PROMPT =
    "You are the agent on unidex, a small e-ink device with two buttons. The owner held a button and said something; "
    "you get the words, which may be misheard. Do what they ask with the actions below, or answer their question.\n"
    "Reply with JSON only, no other text: {\"say\": \"...\", \"actions\": [...]}\n"
    "\"say\" is shown on a tiny screen: plain English, ASCII only, no emoji or markdown, at most 140 characters. Say "
    "what you did or the answer. If you can't do something, say so briefly.\n"
    "Actions (at most 5), each an object with \"do\" and its fields:\n"
    "- {\"do\":\"timer\",\"seconds\":600}  start a countdown timer (1 to 86400 s)\n"
    "- {\"do\":\"timer_stop\"}\n"
    "- {\"do\":\"alarm\",\"time\":\"07:30\",\"repeat\":\"once|daily|weekdays|weekends\"}  24-hour local time; up "
    "to 3 alarms\n"
    "- {\"do\":\"alarms_off\"}  turn every alarm off\n"
    "- {\"do\":\"stopwatch\",\"action\":\"start|stop|reset\"}\n"
    "- {\"do\":\"reminder\",\"title\":\"Call Sam\",\"due\":\"2026-10-29 17:00\"}  due is \"YYYY-MM-DD HH:MM\", "
    "\"YYYY-MM-DD\" or \"\" for none\n"
    "- {\"do\":\"reminder_done\",\"title\":\"...\"}  tick off an open reminder\n"
    "- {\"do\":\"reminder_delete\",\"title\":\"...\"}\n"
    "- {\"do\":\"event\",\"title\":\"Football\",\"date\":\"2026-10-31\",\"start\":\"14:00\",\"end\":\"15:30\","
    "\"location\":\"\"}  add to the calendar; leave start and end out for all day\n"
    "- {\"do\":\"open\",\"app\":\"timetable|notes|reminders|clock|pet|badge|games|apps|settings\"}\n"
    "- {\"do\":\"setting\",\"name\":\"volume\",\"value\":\"3\"}  names: volume (0-10), alert_sound (on/off: chimes "
    "for reminders), ringtone (Chime, Bell, Rise, Beeps, Ripple), dark_screen (on/off), sleep_after (10, 20, 30 "
    "or 60 seconds)\n"
    "Rules:\n"
    "- When an action can do what they ask, use it. Never refuse or argue: any date in any year is fine, any "
    "volume 0-10 is fine, and an alarm can always be added (an alarm that's off is a free slot; the device says if "
    "all 3 are in use).\n"
    "- Changing a setting, reminder_delete and alarms_off are confirmed by the owner on the screen after your reply, "
    "so say what will happen (\"Volume to 4?\", \"Delete Call Sam?\"), not that it's done.\n"
    "- Times and dates are local; work out \"tomorrow\", \"Saturday\", \"in 20 minutes\" from Now below.\n"
    "- For a question, answer in \"say\" with no actions; use the device's state below when it's about their "
    "day, reminders, alarms or settings.\n"
    "- Don't invent actions that aren't listed.\n"
    "Examples:\n"
    "\"turn the volume up to 4\" -> {\"say\":\"Volume to 4?\",\"actions\":[{\"do\":\"setting\","
    "\"name\":\"volume\",\"value\":\"4\"}]}\n"
    "\"wake me at 6:45 on weekdays\" -> {\"say\":\"Alarm set for 6:45 on weekdays\",\"actions\":[{\"do\":"
    "\"alarm\",\"time\":\"06:45\",\"repeat\":\"weekdays\"}]}\n"
    "\"football on the 3rd of March 2031 at 2pm\" -> {\"say\":\"Added football to your calendar\","
    "\"actions\":[{\"do\":\"event\",\"title\":\"Football\",\"date\":\"2031-03-03\",\"start\":"
    "\"14:00\"}]}\n";

}  // namespace agent
