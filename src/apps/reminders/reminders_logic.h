#pragma once
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// Reminders without the screen: one reminder per line of /reminders.csv, the list's order and the short due tags.
// No Arduino in here, so tools/remindertest checks it on a computer.
namespace reminders {

const int MAX_REMINDERS = 40;

// due is local time: "YYYY-MM-DD HH:MM", "YYYY-MM-DD" (a day, no time) or "" (no date).
struct Reminder {
  std::string due;
  bool done;
  std::string title;
  bool hasTime() const { return due.size() == 16; }
};

// A day as a count from 1970-01-01 (Howard Hinnant's days_from_civil), so dates can be added and compared.
inline int daysFromCivil(int y, int m, int d) {
  y -= m <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const int yoe = y - era * 400;
  const int doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}

inline void civilFromDays(int z, int &y, int &m, int &d) {
  z += 719468;
  const int era = (z >= 0 ? z : z - 146096) / 146097;
  const int doe = z - era * 146097;
  const int yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const int doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const int mp = (5 * doy + 2) / 153;
  d = doy - (153 * mp + 2) / 5 + 1;
  m = mp + (mp < 10 ? 3 : -9);
  y = yoe + era * 400 + (m <= 2);
}

inline int weekday(int days) { return ((days % 7) + 11) % 7; }  // 0 = Sunday (1970-01-01 was a Thursday)

// The moment a reminder is set for, as minutes from 1970 in local time. A day with no time counts from 09:00.
// -1 for no date or a date that doesn't read.
inline long dueMinutes(const std::string &due) {
  int y, mo, d, h = 9, mi = 0;
  if (due.size() == 10 && sscanf(due.c_str(), "%4d-%2d-%2d", &y, &mo, &d) == 3) {
  } else if (due.size() == 16 && sscanf(due.c_str(), "%4d-%2d-%2d %2d:%2d", &y, &mo, &d, &h, &mi) == 5) {
  } else {
    return -1;
  }
  if (mo < 1 || mo > 12 || d < 1 || d > 31 || h > 23 || mi > 59) return -1;
  return (long)daysFromCivil(y, mo, d) * 1440 + h * 60 + mi;
}

inline std::string formatDue(int days, int minutes) {  // minutes < 0: a day only
  int y, m, d;
  civilFromDays(days, y, m, d);
  char out[20];
  if (minutes < 0) snprintf(out, sizeof out, "%04d-%02d-%02d", y, m, d);
  else snprintf(out, sizeof out, "%04d-%02d-%02d %02d:%02d", y, m, d, minutes / 60, minutes % 60);
  return out;
}

// --- the file ---

inline std::string toLine(const Reminder &r) { return r.due + "," + (r.done ? "1" : "0") + "," + r.title; }

// "due,done,title": the title is last, so commas in it are kept. False for a line that isn't one.
inline bool fromLine(const std::string &line, Reminder &r) {
  const size_t a = line.find(','), b = a == std::string::npos ? a : line.find(',', a + 1);
  if (b == std::string::npos) return false;
  r.due = line.substr(0, a);
  r.done = line.compare(a + 1, b - a - 1, "1") == 0;
  r.title = line.substr(b + 1);
  while (!r.title.empty() && (r.title.back() == '\r' || r.title.back() == ' ')) r.title.pop_back();
  if (!r.due.empty() && dueMinutes(r.due) < 0) r.due = "";  // a damaged date: keep the reminder, without it
  return !r.title.empty();
}

// --- the list's order ---

// Open ones first; of those, dated ones by time (so late ones come first), then undated ones; done ones last.
// Stable: a new reminder is put at the front, so among undated (and done) ones the newest comes first.
inline void sortList(std::vector<Reminder> &list) {
  std::stable_sort(list.begin(), list.end(), [](const Reminder &a, const Reminder &b) {
    if (a.done != b.done) return b.done;
    if (a.done) return false;
    const long da = dueMinutes(a.due), db = dueMinutes(b.due);
    if ((da < 0) != (db < 0)) return db < 0;
    return da < db;
  });
}

inline bool isLate(const Reminder &r, long nowMin) {
  const long due = dueMinutes(r.due);
  return !r.done && due >= 0 && (r.hasTime() ? due <= nowMin : due / 1440 < nowMin / 1440);
}

// --- tags ---

inline std::string clockText(int minutes) {  // "5pm", "5:30pm", "12am"
  const int h = minutes / 60, m = minutes % 60, h12 = h % 12 ? h % 12 : 12;
  char out[10];
  if (m) snprintf(out, sizeof out, "%d:%02d%s", h12, m, h < 12 ? "am" : "pm");
  else snprintf(out, sizeof out, "%d%s", h12, h < 12 ? "am" : "pm");
  return out;
}

static const char *const DAY_SHORT[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
static const char *const MONTH_SHORT[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                          "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

// When it's due, as short as a list row can take: "late", "5pm", "tmrw 5pm", "Fri 5pm", "3 Nov". "" if undated.
inline std::string dueTag(const Reminder &r, long nowMin) {
  const long due = dueMinutes(r.due);
  if (due < 0 || r.done) return "";
  if (isLate(r, nowMin)) return "late";
  const int day = due / 1440, today = nowMin / 1440;
  const std::string time = r.hasTime() ? clockText(due % 1440) : "";
  if (day == today) return r.hasTime() ? time : "today";
  if (day == today + 1) return r.hasTime() ? "tmrw " + time : "tmrw";
  if (day < today + 7) return r.hasTime() ? std::string(DAY_SHORT[weekday(day)]) + " " + time : DAY_SHORT[weekday(day)];
  int y, m, d;
  civilFromDays(day, y, m, d);
  return std::to_string(d) + " " + MONTH_SHORT[m - 1];
}

// The same, said in full for the toast and the alert: "today 5pm", "tomorrow", "Fri 3 Nov 5pm".
inline std::string dueWords(const Reminder &r, long nowMin) {
  const long due = dueMinutes(r.due);
  if (due < 0) return "";
  const int day = due / 1440, today = nowMin / 1440;
  std::string when;
  if (day == today) when = "today";
  else if (day == today + 1) when = "tomorrow";
  else {
    int y, m, d;
    civilFromDays(day, y, m, d);
    when = std::string(DAY_SHORT[weekday(day)]) + " " + std::to_string(d) + " " + MONTH_SHORT[m - 1];
  }
  return r.hasTime() ? when + " " + clockText(due % 1440) : when;
}

// The soonest open reminder still to come (with a time, or a day's 09:00), as an index; -1 if none.
inline int nextDue(const std::vector<Reminder> &list, long nowMin) {
  int best = -1;
  for (size_t i = 0; i < list.size(); i++) {
    const long due = dueMinutes(list[i].due);
    if (list[i].done || due < 0 || due <= nowMin) continue;
    if (best < 0 || due < dueMinutes(list[best].due)) best = i;
  }
  return best;
}

// --- understanding what was said ---
// "Remind me to call Sam tomorrow at 5pm" -> "Call Sam", tomorrow 17:00. Rules, not a model: the words that say
// when are found and taken out, and whatever is left is the reminder.

struct Word {
  std::string raw;  // as said, for the title
  std::string w;    // lower case, outer punctuation off: what the rules read
  bool used;
};

inline std::vector<Word> words(const std::string &text) {
  std::vector<Word> out;
  size_t i = 0;
  while (i < text.size()) {
    while (i < text.size() && isspace((unsigned char)text[i])) i++;
    const size_t start = i;
    while (i < text.size() && !isspace((unsigned char)text[i])) i++;
    if (i == start) break;
    Word word = {text.substr(start, i - start), "", false};
    std::string w;
    for (char c : word.raw) w += (char)tolower((unsigned char)c);
    while (!w.empty() && !isalnum((unsigned char)w.back())) w.pop_back();
    while (!w.empty() && !isalnum((unsigned char)w[0])) w.erase(0, 1);
    if (w == "p.m" || w == "a.m") w = w == "p.m" ? "pm" : "am";
    // "5pm", "5:30p.m." -> "5:30" and "pm"
    size_t digits = 0;
    while (digits < w.size() && (isdigit((unsigned char)w[digits]) || w[digits] == ':' || w[digits] == '.')) digits++;
    std::string rest = w.substr(digits);
    rest.erase(std::remove(rest.begin(), rest.end(), '.'), rest.end());
    if (digits && (rest == "pm" || rest == "am")) {
      word.w = w.substr(0, digits);
      out.push_back(word);
      out.push_back({"", rest, false});
      continue;
    }
    word.w = w;
    out.push_back(word);
  }
  return out;
}

inline int numberWord(const std::string &w) {  // "5" or "five" -> 5; -1 if not a whole number
  static const char *const NAMES[] = {"zero", "one", "two",   "three",  "four",   "five",   "six",
                                      "seven", "eight", "nine", "ten", "eleven", "twelve"};
  for (int i = 0; i <= 12; i++)
    if (w == NAMES[i]) return i;
  if (w == "a" || w == "an") return 1;
  if (w.empty() || w.size() > 3) return -1;
  for (char c : w)
    if (!isdigit((unsigned char)c)) return -1;
  return atoi(w.c_str());
}

inline int ordinalWord(const std::string &w) {  // "3rd", "third", "3" -> 3; -1 if not a day of the month
  static const char *const NAMES[] = {"",        "first",  "second", "third",    "fourth",  "fifth",  "sixth",
                                      "seventh", "eighth", "ninth",  "tenth",    "eleventh", "twelfth"};
  for (int i = 1; i <= 12; i++)
    if (w == NAMES[i]) return i;
  size_t n = 0;
  while (n < w.size() && isdigit((unsigned char)w[n])) n++;
  const std::string suffix = w.substr(n);
  if (!n || n > 2 || !(suffix.empty() || suffix == "st" || suffix == "nd" || suffix == "rd" || suffix == "th")) return -1;
  const int d = atoi(w.substr(0, n).c_str());
  return d >= 1 && d <= 31 ? d : -1;
}

inline int monthWord(const std::string &w) {  // 1-12, -1 if not a month
  static const char *const NAMES[] = {"january", "february", "march",     "april",   "may",      "june",
                                      "july",    "august",   "september", "october", "november", "december"};
  for (int i = 0; i < 12; i++) {
    const std::string full = NAMES[i];
    if (w == full || (w.size() >= 3 && w.size() <= 4 && full.compare(0, w.size(), w) == 0 && w != "marc" && w != "ma"))
      return i + 1;
  }
  return -1;
}

inline int weekdayWord(const std::string &w) {  // 0 = Sunday, -1 if not a day
  static const char *const NAMES[] = {"sunday", "monday", "tuesday", "wednesday", "thursday", "friday", "saturday"};
  for (int i = 0; i < 7; i++)
    if (w == NAMES[i] || w == std::string(NAMES[i]) + "s") return i;
  return -1;
}

// "5", "5:30", "5.30", "17:30", "five" -> hour and minute; false if it isn't a clock time.
inline bool clockWord(const std::string &w, int &h, int &m) {
  const size_t sep = w.find_first_of(":.");
  m = 0;
  if (sep == std::string::npos) {
    h = numberWord(w);
    return w != "a" && w != "an" && h >= 0 && h <= 24;
  }
  const std::string hs = w.substr(0, sep), ms = w.substr(sep + 1);
  if (hs.empty() || hs.size() > 2 || ms.size() != 2) return false;
  for (char c : hs + ms)
    if (!isdigit((unsigned char)c)) return false;
  h = atoi(hs.c_str());
  m = atoi(ms.c_str());
  return h <= 24 && m <= 59;
}

inline int minuteWord(const std::string &w) {  // "five thirty": the minutes said as a word
  if (w == "fifteen") return 15;
  if (w == "thirty") return 30;
  if (w == "forty-five" || w == "fortyfive") return 45;
  return -1;
}

struct Heard {
  std::string title;
  std::string due;  // as in Reminder: "" when no time was said
};

inline Heard parseReminder(const std::string &text, long nowMin) {
  std::vector<Word> ws = words(text);
  const int n = ws.size(), today = nowMin / 1440, nowClock = nowMin % 1440;
  auto at = [&](int i) -> const std::string & {
    static const std::string none;
    return i >= 0 && i < n ? ws[i].w : none;
  };
  auto use = [&](int from, int to) {  // [from, to), with a word before that only points at the time ("at", "on")
    for (int i = from; i < to; i++) ws[i].used = true;
    if (from > 0 && !ws[from - 1].used) {
      const std::string &p = ws[from - 1].w;
      if (p == "at" || p == "on" || p == "by" || p == "for" || p == "this" || p == "next" || p == "the" || p == "from" ||
          p == "around")
        ws[from - 1].used = true;
      if (p == "the" && from > 1 && (at(from - 2) == "on" || at(from - 2) == "in")) ws[from - 2].used = true;
    }
  };

  int day = -1;              // the day said, as days from 1970
  int clock = -1;            // minutes after midnight said
  int part = -1;             // the part of the day said ("evening"): its usual time
  bool ampm = false;         // the time said had am or pm (or was a 24-hour time)
  long relative = -1;        // "in 20 minutes": minutes from now
  for (int i = 0; i < n; i++) {
    if (ws[i].used) continue;
    const std::string &w = ws[i].w;
    int a, b;

    // "in 20 minutes", "in an hour", "in half an hour", "in 3 days", "in a week"
    if (w == "in" && i + 2 < n) {
      int k = i + 1;
      double count = -1;
      if (at(k) == "half" && (at(k + 1) == "an" || at(k + 1) == "a")) {
        count = 0.5;
        k += 2;
      } else if ((count = numberWord(at(k))) >= 0) {
        k++;
      }
      const std::string &unit = at(k);
      long per = 0;
      if (unit == "minute" || unit == "minutes" || unit == "min" || unit == "mins") per = 1;
      else if (unit == "hour" || unit == "hours") per = 60;
      else if (unit == "day" || unit == "days") per = 1440;
      else if (unit == "week" || unit == "weeks") per = 7 * 1440;
      if (count > 0 && per) {
        if (at(k + 1) == "and" && at(k + 2) == "a" && at(k + 3) == "half") count += 0.5, k += 3;  // "an hour and a half"
        if (per >= 1440) day = today + (int)(count * per / 1440);
        else relative = (long)(count * per + 0.5);
        for (int j = i; j <= k; j++) ws[j].used = true;
        if (at(k + 1) == "time") ws[k + 1].used = true;  // "in 2 hours time"
        continue;
      }
    }

    if (w == "today") day = today, use(i, i + 1);
    else if (w == "tonight") day = today, part = 20 * 60, use(i, i + 1);
    else if (w == "tomorrow" || w == "tmrw") day = today + 1, use(i, i + 1);
    else if (w == "week" && at(i - 1) == "next") day = today + 7, use(i, i + 1);
    else if (w == "weekend" && at(i - 1) != "every") day = today + (6 - weekday(today) + 7) % 7, use(i, i + 1);
    else if (w == "noon" || w == "midday") clock = 12 * 60, ampm = true, use(i, i + 1);
    else if (w == "midnight") clock = 0, ampm = true, day = (day < 0 ? today : day) + 1, use(i, i + 1);
    else if ((w == "morning" || w == "afternoon" || w == "evening" || w == "night") &&
             (at(i - 1) == "this" || at(i - 1) == "the" || at(i - 1) == "tomorrow" || ws[i > 0 ? i - 1 : 0].used ||
              weekdayWord(at(i - 1)) >= 0)) {
      part = w == "morning" ? 9 * 60 : w == "afternoon" ? 15 * 60 : w == "evening" ? 19 * 60 : 20 * 60;
      use(i, i + 1);
      if (at(i - 1) == "the" && at(i - 2) == "in") ws[i - 2].used = ws[i - 1].used = true;
      if (day < 0 && at(i - 1) == "this") day = today;
    }
    // "Friday", "on Friday", "next Friday"; not "every Friday" (that's part of what to do)
    else if ((a = weekdayWord(w)) >= 0 && at(i - 1) != "every") {
      int ahead = (a - weekday(today) + 7) % 7;
      if (ahead == 0) ahead = 7;
      day = today + ahead;
      use(i, i + 1);
    }
    // "3 November", "3rd of November", "November 3rd"
    else if ((a = ordinalWord(w)) > 0 && (b = monthWord(at(i + 1) == "of" ? at(i + 2) : at(i + 1))) > 0) {
      const int len = at(i + 1) == "of" ? 3 : 2;
      int y, mo, d;
      civilFromDays(today, y, mo, d);
      day = daysFromCivil(y, b, a);
      if (day < today) day = daysFromCivil(y + 1, b, a);
      use(i, i + len);
    } else if ((b = monthWord(w)) > 0 && (a = ordinalWord(at(i + 1))) > 0 && (at(i) != "may" || at(i - 1) == "on")) {
      int y, mo, d;
      civilFromDays(today, y, mo, d);
      day = daysFromCivil(y, b, a);
      if (day < today) day = daysFromCivil(y + 1, b, a);
      use(i, i + 2);
    }
    // "on the 3rd": this month's, or next month's if that has gone
    else if (w == "the" && at(i - 1) == "on" && (a = ordinalWord(at(i + 1))) > 0 && at(i + 2) != "of" &&
             monthWord(at(i + 2)) < 0) {
      int y, mo, d;
      civilFromDays(today, y, mo, d);
      day = daysFromCivil(y, mo, a);
      if (a < d) day = mo == 12 ? daysFromCivil(y + 1, 1, a) : daysFromCivil(y, mo + 1, a);
      use(i, i + 2);
    }
    // "half past 5", "quarter past 5", "quarter to 6"
    else if ((w == "half" || w == "quarter") && (at(i + 1) == "past" || at(i + 1) == "to") && clockWord(at(i + 2), a, b) &&
             b == 0 && a >= 1 && a <= 12) {
      const int offset = w == "half" ? 30 : 15;
      clock = at(i + 1) == "past" ? a * 60 + offset : (a - 1) * 60 + 60 - offset;
      use(at(i - 1) == "a" ? i - 1 : i, i + 3);  // "at a quarter past 4"
    }
    // "at 5", "at 5:30", "5pm", "17:30", "at five thirty", "5 o'clock"
    else if (clockWord(w, a, b)) {
      const bool pointed = at(i - 1) == "at" || at(i - 1) == "by" || at(i - 1) == "around";
      const bool marked = at(i + 1) == "am" || at(i + 1) == "pm" || at(i + 1) == "o'clock" || at(i + 1) == "oclock";
      const bool twentyFour = w.find_first_of(":.") != std::string::npos && (a > 12 || w[0] == '0');
      if (!(pointed || marked || (w.find(':') != std::string::npos))) continue;
      int len = 1;
      if (w.find_first_of(":.") == std::string::npos && minuteWord(at(i + 1)) >= 0) b = minuteWord(at(i + 1)), len++;
      if (at(i + len) == "am" || at(i + len) == "pm") {
        if (a > 12) continue;
        ampm = true;
        a = a % 12 + (at(i + len) == "pm" ? 12 : 0);
        len++;
      } else if (at(i + len) == "o'clock" || at(i + len) == "oclock") {
        len++;
      }
      if (twentyFour || a > 12) ampm = true;
      if (a == 24) a = 0;
      clock = a * 60 + b;
      use(i, i + len);
    }
  }

  Heard out;
  // What's left is the reminder, without the opening that only says it is one.
  std::vector<std::string> left, leftLow;
  for (const Word &w : ws)
    if (!w.used && !w.raw.empty()) left.push_back(w.raw), leftLow.push_back(w.w);
  static const char *const OPENINGS[] = {"remind me to", "remind me", "reminder to", "reminder", "remember to",
                                         "i need to", "i have to", "i've got to", "i must", "don't forget to",
                                         "dont forget to", "please", "to"};
  for (bool again = true; again;) {
    again = false;
    for (const char *o : OPENINGS) {
      std::vector<Word> ow = words(o);
      if (ow.size() > leftLow.size()) continue;
      bool match = true;
      for (size_t k = 0; k < ow.size() && match; k++) match = leftLow[k] == ow[k].w;
      if (!match) continue;
      left.erase(left.begin(), left.begin() + ow.size());
      leftLow.erase(leftLow.begin(), leftLow.begin() + ow.size());
      again = true;
      break;
    }
  }
  // A word left dangling at the end once the time is gone ("Call Sam on", "Call Sam and").
  while (!leftLow.empty() && (leftLow.back() == "at" || leftLow.back() == "on" || leftLow.back() == "by" ||
                              leftLow.back() == "and" || leftLow.back() == "for" || leftLow.back().empty()))
    left.pop_back(), leftLow.pop_back();
  for (size_t k = 0; k < left.size(); k++) out.title += (k ? " " : "") + left[k];
  while (!out.title.empty() && strchr(" .,;:!?-", out.title.back())) out.title.pop_back();
  while (!out.title.empty() && strchr(" .,;:!?-", out.title[0])) out.title.erase(0, 1);
  if (!out.title.empty()) out.title[0] = toupper((unsigned char)out.title[0]);
  if (out.title.empty()) out.title = "Reminder";

  // When: "in 20 minutes" wins; otherwise the day and the time said, with the gaps filled in.
  if (nowMin < 0) return out;  // the clock isn't set: keep the words, no time
  if (relative > 0) {
    const long due = nowMin + relative;
    out.due = formatDue(due / 1440, due % 1440);
    return out;
  }
  if (clock < 0 && part >= 0) clock = part, ampm = true;
  if (clock >= 0 && !ampm) {
    int h = clock / 60;
    const int m = clock % 60;
    if (part >= 12 * 60 && h < 12) h += 12;                // "at 7 this evening"
    else if (part < 0 && h >= 1 && h <= 6) h += 12;        // "at 5": 5pm, nobody means 5am
    else if (part < 0 && day < 0 && h < 12 && h * 60 + m <= nowClock && (h + 12) * 60 + m > nowClock)
      h += 12;                                              // "at 8" said at 10am: tonight, not tomorrow morning
    clock = h * 60 + m;
  }
  if (day < 0 && clock < 0) return out;
  if (day < 0) day = clock > nowClock ? today : today + 1;  // a time on its own: the next time it comes round
  out.due = formatDue(day, clock);
  return out;
}

}  // namespace reminders
