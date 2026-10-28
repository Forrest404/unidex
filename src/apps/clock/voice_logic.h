#pragma once
// Clock by voice: what a spoken command asks for ("set a timer for 8 minutes", "wake me up at 7 every weekday"), read
// from the words the transcription heard. Rules, not a model, and English only, like the reminders' (whose word
// helpers this uses). No Arduino here, so tools/clocktest checks it on a computer.
#include <string>
#include <vector>
#include "clock_logic.h"
#include "../reminders/reminders_logic.h"

namespace clockvoice {

enum class Kind { NONE, TIMER, TIMER_STOP, ALARM, ALARMS_OFF, STOPWATCH_START, STOPWATCH_STOP };

struct Command {
  Kind kind = Kind::NONE;
  long seconds = 0;           // TIMER: how long
  int hour = -1, minute = 0;  // ALARM: when, local time
  clocks::Repeat repeat = clocks::ONCE;
};

static const long MAX_TIMER = 24 * 3600L;

using reminders::Word;

inline bool any(const std::vector<Word> &ws, std::initializer_list<const char *> names) {
  for (const Word &w : ws)
    for (const char *n : names)
      if (w.w == n) return true;
  return false;
}

// 0-99 from the words at i ("8", "twenty", "twenty five", "twenty-five", "a"); how many words it took in `used`.
inline int numberAt(const std::vector<Word> &ws, size_t i, int &used) {
  static const char *const TEENS[] = {"thirteen", "fourteen", "fifteen", "sixteen", "seventeen", "eighteen", "nineteen"};
  static const char *const TENS[] = {"twenty", "thirty", "forty", "fifty", "sixty", "seventy", "eighty", "ninety"};
  used = 0;
  if (i >= ws.size()) return -1;
  std::string w = ws[i].w;
  int tens = -1, unit = -1;
  const size_t dash = w.find('-');
  if (dash != std::string::npos) unit = reminders::numberWord(w.substr(dash + 1)), w = w.substr(0, dash);
  for (int t = 0; t < 8; t++)
    if (w == TENS[t]) tens = 20 + 10 * t;
  if (tens >= 0) {
    if (dash != std::string::npos) {
      if (unit < 1 || unit > 9) return -1;
      used = 1;
      return tens + unit;
    }
    const int next = i + 1 < ws.size() ? reminders::numberWord(ws[i + 1].w) : -1;
    used = next >= 1 && next <= 9 && ws[i + 1].w != "a" && ws[i + 1].w != "an" ? 2 : 1;
    return tens + (used == 2 ? next : 0);
  }
  for (int t = 0; t < 7; t++)
    if (w == TEENS[t]) return used = 1, 13 + t;
  const int n = reminders::numberWord(w);
  if (n >= 0) used = 1;
  return n;
}

// Seconds in a unit word ("minutes", "mins", "hr"...), 0 if it isn't one.
inline long unitSeconds(const std::string &w) {
  if (w == "hour" || w == "hours" || w == "hr" || w == "hrs" || w == "h") return 3600;
  if (w == "minute" || w == "minutes" || w == "min" || w == "mins" || w == "m") return 60;
  if (w == "second" || w == "seconds" || w == "sec" || w == "secs" || w == "s") return 1;
  return 0;
}

// A length said in the words: "8 minutes", "1 hour 30 minutes", "an hour and a half", "half an hour", "1.5 hours",
// "two and a half minutes", "90 seconds". 0 if none.
inline long duration(const std::vector<Word> &ws) {
  long total = 0;
  for (size_t i = 0; i < ws.size(); i++) {
    if (ws[i].w == "half" && i + 2 < ws.size() && (ws[i + 1].w == "an" || ws[i + 1].w == "a") && unitSeconds(ws[i + 2].w)) {
      total += unitSeconds(ws[i + 2].w) / 2;  // "half an hour"
      i += 2;
      continue;
    }
    if (ws[i].w == "quarter" && i + 3 < ws.size() && ws[i + 1].w == "of" && unitSeconds(ws[i + 3].w) == 3600) {
      total += 900;  // "a quarter of an hour"
      i += 3;
      continue;
    }
    double n = -1;
    int used = 0;
    const std::string &w = ws[i].w;
    const size_t dot = w.find('.');
    if (dot != std::string::npos && dot > 0 && dot + 1 < w.size() &&
        w.find_first_not_of("0123456789.") == std::string::npos) {
      n = atof(w.c_str()), used = 1;  // "1.5"
    } else {
      n = numberAt(ws, i, used);
    }
    if (n < 0 || !used) continue;
    size_t j = i + used;
    bool half = false;  // "two and a half minutes"
    if (j + 2 < ws.size() && ws[j].w == "and" && ws[j + 1].w == "a" && ws[j + 2].w == "half") half = true, j += 3;
    if (j >= ws.size() || !unitSeconds(ws[j].w)) continue;
    const long unit = unitSeconds(ws[j].w);
    total += (long)(n * unit) + (half ? unit / 2 : 0);
    // "an hour and a half"
    if (j + 3 < ws.size() && ws[j + 1].w == "and" && ws[j + 2].w == "a" && ws[j + 3].w == "half") total += unit / 2, j += 3;
    i = j;
  }
  return total;
}

// A time of day in the words: "5pm", "5:30 pm", "17:45", "7", "seven thirty", "half past 6", "quarter to 7", "noon",
// "midnight". Without am or pm, morning/afternoon/evening/tonight decide, and failing that, of h:mm and h+12:mm, the
// one that comes next after nowMin (minutes since midnight; -1 when the clock isn't set: as said).
inline bool timeOfDay(const std::vector<Word> &ws, int nowMin, int &h, int &m) {
  int found = -1;
  h = -1, m = 0;
  for (size_t i = 0; i < ws.size() && found < 0; i++) {
    const std::string &w = ws[i].w;
    if (w == "noon" || w == "midday") return h = 12, m = 0, true;
    if (w == "midnight") return h = 0, m = 0, true;
    int used, hh, mm;
    if ((w == "half" || w == "quarter") && i + 2 < ws.size() && (ws[i + 1].w == "past" || ws[i + 1].w == "to") &&
        reminders::clockWord(ws[i + 2].w, hh, mm) && mm == 0 && hh >= 1 && hh <= 12) {
      const int off = w == "half" ? 30 : 15;
      if (ws[i + 1].w == "past") h = hh, m = off;
      else h = hh == 1 ? 12 : hh - 1, m = 60 - off;  // "quarter to 7": 6:45
      found = (int)i;
      break;
    }
    // a bare number only counts as a time after "at" or "for", or with am/pm/o'clock after it, or a colon
    const bool colon = w.find_first_of(":.") != std::string::npos;
    const std::string next = i + 1 < ws.size() ? ws[i + 1].w : "";
    const std::string prev = i ? ws[i - 1].w : "";
    const bool marked = next == "am" || next == "pm" || next == "o'clock" || next == "oclock";
    if (!(colon || marked || prev == "at" || prev == "for")) continue;
    if (reminders::numberWord(w) >= 0 && !colon && unitSeconds(next)) continue;  // "for 8 minutes"
    if (!reminders::clockWord(w, hh, mm) || w == "a" || w == "an") continue;
    if (!colon) {  // the minutes said after it: "seven thirty", "seven forty five", "6 45"
      int extra = reminders::minuteWord(next);
      if (extra < 0) {
        const int n = numberAt(ws, i + 1, used);
        const std::string after = i + 1 + used < ws.size() ? ws[i + 1 + used].w : "";
        if (n >= 10 && n <= 59 && !unitSeconds(after)) extra = n;
      }
      if (extra >= 0) mm = extra;
    }
    h = hh, m = mm, found = (int)i;
  }
  if (found < 0 || h > 24 || h < 0) return false;
  if (h == 24) h = 0;
  const bool pm = any(ws, {"pm", "afternoon", "evening", "tonight", "night"}), am = any(ws, {"am", "morning"});
  if (h <= 12 && (pm || am)) {
    if (pm && h < 12) h += 12;
    if (am && h == 12) h = 0;
  } else if (h >= 1 && h <= 12 && nowMin >= 0) {
    const int a = (h % 12) * 60 + m, b = a + 12 * 60;  // h:mm am and pm
    const int untilA = (a - nowMin + 1439) % 1440 + 1, untilB = (b - nowMin + 1439) % 1440 + 1;  // now: a day away
    h = (untilA <= untilB ? a : b) / 60;
  }
  return true;
}

// What the words ask Clock to do. nowMin: local minutes since midnight (-1: the clock isn't set).
inline Command parse(const std::string &text, int nowMin) {
  Command c;
  const std::vector<Word> ws = reminders::words(text);
  const bool stop = any(ws, {"stop", "cancel", "clear", "reset", "end"});
  const bool off = stop || any(ws, {"off", "delete", "remove", "disable"});
  const bool alarm = any(ws, {"alarm", "alarms", "wake", "waking"});
  const bool timer = any(ws, {"timer", "countdown"});
  if (any(ws, {"stopwatch"})) {
    c.kind = stop || any(ws, {"pause"}) ? Kind::STOPWATCH_STOP : Kind::STOPWATCH_START;
    return c;
  }
  if (timer && stop) return c.kind = Kind::TIMER_STOP, c;
  if (alarm && off) return c.kind = Kind::ALARMS_OFF, c;
  const long d = duration(ws);
  if (alarm) {
    int h, m;
    if (timeOfDay(ws, nowMin, h, m)) c.hour = h, c.minute = m;
    else if (d > 0 && nowMin >= 0) c.hour = (int)((nowMin + (d + 59) / 60) / 60 % 24), c.minute = (int)((nowMin + (d + 59) / 60) % 60);
    else return c;  // an alarm, but no time: nothing to set
    c.kind = Kind::ALARM;
    if (any(ws, {"weekday", "weekdays"})) c.repeat = clocks::WEEKDAYS;
    else if (any(ws, {"weekend", "weekends"})) c.repeat = clocks::WEEKENDS;
    else if (any(ws, {"daily", "everyday"}) || (any(ws, {"every", "each"}) && any(ws, {"day", "morning", "night"})))
      c.repeat = clocks::DAILY;
    return c;
  }
  if (d > 0) {  // "8 minutes", "timer for 1 hour": a timer (said without "timer" too)
    c.kind = Kind::TIMER;
    c.seconds = d > MAX_TIMER ? MAX_TIMER : d;
  }
  return c;
}

}  // namespace clockvoice
