#pragma once
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

// A calendar file (.ics) to the Timetable's event lines, for Settings > Sync's calendar link. It gives the same
// lines as the Tools page (site/calendar.js, which uses ical.js): London time, today and the next 7 days, repeating
// events, moved and cancelled ones. Fed one line at a time, so a big calendar never has to fit in memory; only the
// events that can reach the window are kept. No Arduino in here, so tools/caltest checks it on a computer.
namespace ics {

const int DAYS_AHEAD = 7, MAX_EVENTS = 64;
const size_t MAX_LINE = 4096, MAX_TEXT = 2000;  // longer lines (attachments) and text are cut

typedef long long Secs;  // seconds since 1970-01-01

inline Secs floorDiv(Secs a, Secs b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }

// A day as a count from 1970-01-01 (Howard Hinnant's days_from_civil and civil_from_days).
inline Secs daysFromCivil(Secs y, int m, int d) {
  y -= m <= 2;
  const Secs era = (y >= 0 ? y : y - 399) / 400;
  const Secs yoe = y - era * 400;
  const Secs doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const Secs doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}

inline void civilFromDays(Secs z, int &y, int &m, int &d) {
  z += 719468;
  const Secs era = (z >= 0 ? z : z - 146096) / 146097;
  const Secs doe = z - era * 146097;
  const Secs yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const Secs doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const Secs mp = (5 * doy + 2) / 153;
  d = (int)(doy - (153 * mp + 2) / 5 + 1);
  m = (int)(mp < 10 ? mp + 3 : mp - 9);
  y = (int)(yoe + era * 400 + (m <= 2));
}

inline int weekday(Secs day) { return (int)(((day % 7) + 11) % 7); }  // 0 = Sunday (1970-01-01 was a Thursday)

inline int daysInMonth(int y, int m) {
  static const int DAYS[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  return DAYS[m - 1] + (m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0));
}

// The nth `wday` of a month (nth < 0 counts from the end), or a day outside the month if there isn't one.
inline Secs nthWeekday(int y, int m, int nth, int wday) {
  if (nth > 0) {
    const Secs first = daysFromCivil(y, m, 1);
    return first + (wday - weekday(first) + 7) % 7 + (nth - 1) * 7;
  }
  const Secs last = daysFromCivil(y, m, daysInMonth(y, m));
  return last - (weekday(last) - wday + 7) % 7 + (nth + 1) * 7;
}

// A time zone's yearly rule, from a VTIMEZONE (or built in: London, UTC). Offsets in seconds east of UTC.
struct Change {
  int month, nth, wday;
  Secs at;  // local time of day the change happens, before it
};
struct Zone {
  Secs std = 0, dst = 0;
  bool hasDst = false;
  Change toDst = Change(), toStd = Change();
  bool inDst(Secs utc) const {
    int y, m, d;
    civilFromDays(floorDiv(utc, 86400), y, m, d);
    const Secs start = nthWeekday(y, toDst.month, toDst.nth, toDst.wday) * 86400 + toDst.at - std;
    const Secs end = nthWeekday(y, toStd.month, toStd.nth, toStd.wday) * 86400 + toStd.at - dst;
    return start < end ? utc >= start && utc < end : utc >= start || utc < end;  // the south: summer over new year
  }
  Secs offsetAt(Secs utc) const { return hasDst && inDst(utc) ? dst : std; }
  Secs toUtc(Secs local) const {
    if (hasDst && inDst(local - dst)) return local - dst;
    return local - std;
  }
};

inline Zone london() {
  Zone z;
  z.dst = 3600;
  z.hasDst = true;
  z.toDst = {3, -1, 0, 3600};      // last Sunday in March, 01:00
  z.toStd = {10, -1, 0, 2 * 3600};  // last Sunday in October, 02:00 (BST)
  return z;
}

// A date or date-time value. `local` is the wall clock in seconds (UTC itself when `utc`).
struct When {
  bool set = false, date = false, utc = false;
  Secs local = 0;
  std::string tzid;
  Secs day() const { return floorDiv(local, 86400); }
};

inline bool digits(const std::string &s, size_t at, size_t n) {
  if (at + n > s.size()) return false;
  for (size_t i = at; i < at + n; i++)
    if (s[i] < '0' || s[i] > '9') return false;
  return true;
}

inline int num(const std::string &s, size_t at, size_t n) { return atoi(s.substr(at, n).c_str()); }

// "20261023", "20261023T143000" or "20261023T143000Z".
inline When parseWhen(const std::string &v, const std::string &tzid, bool dateParam) {
  When w;
  if (!digits(v, 0, 8)) return w;
  const Secs day = daysFromCivil(num(v, 0, 4), num(v, 4, 2), num(v, 6, 2));
  w.set = true;
  if (dateParam || v.size() < 15 || v[8] != 'T' || !digits(v, 9, 6)) {
    w.date = true;
    w.local = day * 86400;
    return w;
  }
  w.local = day * 86400 + num(v, 9, 2) * 3600 + num(v, 11, 2) * 60 + num(v, 13, 2);
  w.utc = v.size() > 15 && v[15] == 'Z';
  if (!w.utc) w.tzid = tzid;
  return w;
}

// "P1D", "PT1H30M", "-P1W": seconds.
inline Secs parseDuration(const std::string &v) {
  Secs total = 0, n = 0;
  const bool negative = !v.empty() && v[0] == '-';
  for (char c : v) {
    if (c >= '0' && c <= '9') n = n * 10 + (c - '0');
    else {
      if (c == 'W') total += n * 7 * 86400;
      else if (c == 'D') total += n * 86400;
      else if (c == 'H') total += n * 3600;
      else if (c == 'M') total += n * 60;
      else if (c == 'S') total += n;
      n = 0;
    }
  }
  return negative ? -total : total;
}

// Text values: \n, \, \; and \\ escapes.
inline std::string unescape(const std::string &v) {
  std::string out;
  for (size_t i = 0; i < v.size(); i++) {
    if (v[i] == '\\' && i + 1 < v.size()) {
      const char c = v[++i];
      out += c == 'n' || c == 'N' ? '\n' : c;
    } else {
      out += v[i];
    }
  }
  return out;
}

// UTF-8 to code points (a bad byte becomes U+FFFD).
inline std::vector<unsigned> codePoints(const std::string &s) {
  std::vector<unsigned> out;
  for (size_t i = 0; i < s.size();) {
    const unsigned char c = s[i];
    const int n = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : (c >> 3) == 30 ? 4 : 0;
    if (!n || i + n > s.size()) {
      out.push_back(0xfffd);
      i++;
      continue;
    }
    unsigned cp = n == 1 ? c : c & (0x7f >> n);
    for (int k = 1; k < n; k++) cp = cp << 6 | (s[i + k] & 0x3f);
    out.push_back(cp);
    i += n;
  }
  return out;
}

// JavaScript's \s.
inline bool jsSpace(unsigned c) {
  return c == ' ' || (c >= 9 && c <= 13) || c == 0xa0 || c == 0x1680 || (c >= 0x2000 && c <= 0x200a) ||
         c == 0x2028 || c == 0x2029 || c == 0x202f || c == 0x205f || c == 0x3000 || c == 0xfeff;
}

// What NFKD and dropping the accents leaves of a character, for the letters and spaces in European text; 0 for one
// that becomes nothing printable (kept as a gap marker until the end, as the website drops them last).
inline std::string plain(unsigned c) {
  static const char *LATIN1 = "AAAAAA\x01" "CEEEEIIII\x01NOOOOO\x01\x01UUUUY\x01\x01" "aaaaaa\x01" "ceeeeiiii\x01nooooo\x01\x01uuuuy\x01y";
  static const char *EXTENDED_A =
    "AaAaAaCcCcCcCcDd\x01\x01" "EeEeEeEeEeGgGgGgGgHh\x01\x01IiIiIiIiI\x01\x02\x02JjKk\x01LlLlLlLl\x01\x01NnNnNnn\x01\x01"
    "OoOoOo\x01\x01RrRrRrSsSsSsSsTtTt\x01\x01UuUuUuUuUuUuWwYyYZzZzZzs";
  if (c < 0x80) return std::string(1, (char)c);
  if (c >= 0x300 && c <= 0x36f) return "";  // accents on their own
  if (c >= 0xc0 && c <= 0xff) return LATIN1[c - 0xc0] == 1 ? "\x01" : std::string(1, LATIN1[c - 0xc0]);
  if (c >= 0x100 && c <= 0x17f) {
    const char m = EXTENDED_A[c - 0x100];
    if (m == 2) return c == 0x132 ? "IJ" : "ij";
    return m == 1 ? "\x01" : std::string(1, m);
  }
  if (c == 0xa0 || (c >= 0x2000 && c <= 0x200a) || c == 0x202f || c == 0x205f || c == 0x3000 || c == 0xa8 ||
      c == 0xaf || c == 0xb4 || c == 0xb8)
    return " ";
  if (c == 0xaa) return "a";
  if (c == 0xba) return "o";
  if (c == 0xb9) return "1";
  if (c == 0xb2) return "2";
  if (c == 0xb3) return "3";
  if (c == 0xbc) return "1\x01" "4";
  if (c == 0xbd) return "1\x01" "2";
  if (c == 0xbe) return "3\x01" "4";
  if (c == 0x2024) return ".";
  if (c == 0x2025) return "..";
  if (c == 0x2026) return "...";
  if (c == 0x2122) return "TM";
  if (c == 0xfb00) return "ff";
  if (c == 0xfb01) return "fi";
  if (c == 0xfb02) return "fl";
  if (c >= 0xff01 && c <= 0xff5e) return std::string(1, (char)(c - 0xfee0));
  if (jsSpace(c)) return "\t";  // still a space to collapse, not one NFKD turns into ' '
  return "\x01";
}

// The website's clean(): ASCII only, runs of spaces and commas become one space (fields are comma-separated and
// an event is one line), trimmed, at most `max` characters.
inline std::string clean(const std::string &s, size_t max) {
  std::string mapped;
  for (unsigned c : codePoints(s)) mapped += plain(c);
  std::string collapsed;
  for (size_t i = 0; i < mapped.size();) {
    const char c = mapped[i];
    if (c == ',' || c == ' ' || (c >= 9 && c <= 13)) {
      while (i < mapped.size() && (mapped[i] == ',' || mapped[i] == ' ' || (mapped[i] >= 9 && mapped[i] <= 13))) i++;
      collapsed += ' ';
    } else {
      collapsed += c;
      i++;
    }
  }
  std::string out;
  for (char c : collapsed)
    if (c >= 0x20 && c <= 0x7e) out += c;
  const size_t a = out.find_first_not_of(' '), b = out.find_last_not_of(' ');
  out = a == std::string::npos ? "" : out.substr(a, b - a + 1);
  return out.substr(0, max);
}

// Notes: every run of spaces with a line break in it becomes " / " (as the website does), then clean().
inline std::string cleanNotes(const std::string &s) {
  const std::vector<unsigned> cps = codePoints(s);
  std::string joined;
  for (size_t i = 0; i < cps.size();) {
    if (!jsSpace(cps[i])) {
      const unsigned c = cps[i++];
      if (c < 0x80) joined += (char)c;
      else if (c < 0x800) joined += (char)(0xc0 | c >> 6), joined += (char)(0x80 | (c & 0x3f));
      else if (c < 0x10000)
        joined += (char)(0xe0 | c >> 12), joined += (char)(0x80 | (c >> 6 & 0x3f)), joined += (char)(0x80 | (c & 0x3f));
      else
        joined += (char)(0xf0 | c >> 18), joined += (char)(0x80 | (c >> 12 & 0x3f)),
          joined += (char)(0x80 | (c >> 6 & 0x3f)), joined += (char)(0x80 | (c & 0x3f));
      continue;
    }
    size_t j = i;
    bool newline = false;
    std::string run;
    for (; j < cps.size() && jsSpace(cps[j]); j++) {
      newline |= cps[j] == '\n';
      run += cps[j] < 0x80 ? std::string(1, (char)cps[j]) : std::string("\xc2\xa0");  // any space: the same later
    }
    joined += newline ? " / " : run;
    i = j;
  }
  return clean(joined, 160);
}

// A repeat rule (RRULE), the parts the big calendar apps write.
struct Rule {
  enum Freq { NONE, DAILY, WEEKLY, MONTHLY, YEARLY } freq = NONE;
  int interval = 1, count = 0, wkst = 1;  // week starts Monday unless WKST says
  When until;
  std::vector<std::pair<int, int>> byDay;  // (nth or 0, weekday)
  std::vector<int> byMonthDay, byMonth, bySetPos;
};

inline int dayCode(const std::string &s) {
  static const char *CODES[] = {"SU", "MO", "TU", "WE", "TH", "FR", "SA"};
  for (int i = 0; i < 7; i++)
    if (s == CODES[i]) return i;
  return -1;
}

inline std::vector<std::string> split(const std::string &s, char sep) {
  std::vector<std::string> out;
  size_t start = 0;
  for (size_t i = 0; i <= s.size(); i++)
    if (i == s.size() || s[i] == sep) {
      out.push_back(s.substr(start, i - start));
      start = i + 1;
    }
  return out;
}

inline Rule parseRule(const std::string &v, const std::string &tzid) {
  Rule r;
  for (const std::string &part : split(v, ';')) {
    const size_t eq = part.find('=');
    if (eq == std::string::npos) continue;
    const std::string key = part.substr(0, eq), val = part.substr(eq + 1);
    if (key == "FREQ")
      r.freq = val == "DAILY" ? Rule::DAILY : val == "WEEKLY" ? Rule::WEEKLY : val == "MONTHLY" ? Rule::MONTHLY
             : val == "YEARLY" ? Rule::YEARLY : Rule::NONE;
    else if (key == "INTERVAL") r.interval = std::max(1, atoi(val.c_str()));
    else if (key == "COUNT") r.count = atoi(val.c_str());
    else if (key == "UNTIL") r.until = parseWhen(val, tzid, false);
    else if (key == "WKST") r.wkst = std::max(0, dayCode(val));
    else if (key == "BYDAY")
      for (const std::string &d : split(val, ',')) {
        if (d.size() < 2) continue;
        const int wd = dayCode(d.substr(d.size() - 2));
        if (wd >= 0) r.byDay.push_back({atoi(d.substr(0, d.size() - 2).c_str()), wd});
      }
    else if (key == "BYMONTHDAY" || key == "BYMONTH" || key == "BYSETPOS")
      for (const std::string &n : split(val, ','))
        (key == "BYMONTHDAY" ? r.byMonthDay : key == "BYMONTH" ? r.byMonth : r.bySetPos).push_back(atoi(n.c_str()));
  }
  return r;
}

struct Event {
  std::string uid, summary, location, description;
  bool hasSummary = false, hasLocation = false, hasDescription = false, cancelled = false, noUid = true;
  When start, end, recurrenceId;
  bool hasDuration = false;
  Secs duration = 0;
  Rule rule;
  std::vector<When> exdates;
};

struct Line {
  Secs at;  // what the lines are sorted by (all-day: the day at 00:00 UTC, as the website does)
  std::string text;
};

class Reader {
 public:
  explicit Reader(Secs nowUtc) : now(nowUtc) {
    zones["Europe/London"] = london();
    zones["UTC"] = zones["Etc/UTC"] = zones["GMT"] = zones["Etc/GMT"] = Zone();
    nowDay = floorDiv(now, 86400);
  }

  // One line of the file as it comes (folded lines are joined here).
  void line(std::string raw) {
    if (!raw.empty() && raw[raw.size() - 1] == '\r') raw.erase(raw.size() - 1);
    if (!raw.empty() && (raw[0] == ' ' || raw[0] == '\t')) {
      if (pending.size() < MAX_LINE) pending += raw.substr(1, MAX_LINE - pending.size());
      return;
    }
    if (havePending) property(pending);
    pending = raw.substr(0, MAX_LINE);
    havePending = true;
  }

  // The event lines for today and the next DAYS_AHEAD days (London), soonest first, at most MAX_EVENTS.
  std::vector<std::string> finish() {
    if (havePending) property(pending);
    havePending = false;
    const Zone uk = london();
    const Secs first = floorDiv(now + uk.offsetAt(now), 86400);
    const Secs later = now + (Secs)DAYS_AHEAD * 86400;
    const Secs last = floorDiv(later + uk.offsetAt(later), 86400);
    std::vector<Line> out;
    for (const Event &ev : masters) {
      if (!ev.start.set || ev.cancelled) continue;
      if (ev.rule.freq == Rule::NONE) add(out, ev, ev.start, ev.end.set ? ev.end : endOf(ev, ev.start), first, last);
      else expand(out, ev, first, last);
    }
    std::stable_sort(out.begin(), out.end(), [](const Line &a, const Line &b) { return a.at < b.at; });
    std::vector<std::string> lines;
    for (size_t i = 0; i < out.size() && (int)i < MAX_EVENTS; i++) lines.push_back(out[i].text);
    return lines;
  }

  size_t kept() const { return masters.size() + overrides.size(); }  // events held for the window (memory check)

 private:
  Secs now, nowDay;
  std::string pending;
  bool havePending = false;
  std::map<std::string, Zone> zones;
  std::vector<Event> masters, overrides;
  std::map<std::string, size_t> masterAt;  // uid -> index in masters (a later copy replaces it in place)

  // Reading state.
  enum In { OUTSIDE, EVENT, TIMEZONE } in = OUTSIDE;
  int depth = 0;  // components inside the event (an alarm) or inside a time zone's STANDARD/DAYLIGHT
  Event ev;
  std::string tzName, subKind;
  struct Sub {
    bool daylight;
    When start;
    Secs from = 0, to = 0;
    Rule rule;
    bool hasRule = false;
  };
  Sub sub;
  std::vector<Sub> subs;

  static Secs offset(const std::string &v) {  // "+0100", "-0500", "+053000"
    if (v.size() < 5) return 0;
    const Secs s = num(v, 1, 2) * 3600 + num(v, 3, 2) * 60 + (v.size() >= 7 ? num(v, 5, 2) : 0);
    return v[0] == '-' ? -s : s;
  }

  const Zone &zoneFor(const std::string &tzid) const {
    auto it = zones.find(tzid);
    if (it != zones.end()) return it->second;
    static const Zone uk = london();  // floating, or a zone the file doesn't describe: London
    return uk;
  }

  Secs utcOf(const When &w) const {
    if (w.utc || w.date) return w.local;
    return zoneFor(w.tzid).toUtc(w.local);
  }

  When shifted(const When &w, Secs by) const {
    When out = w;
    out.local += by;
    return out;
  }

  // A repeat ends the same real time after it starts as the first one did (ical.js's duration).
  When endOf(const Event &e, const When &start) const {
    if (e.end.set) return shifted(start, utcOf(e.end) - utcOf(e.start));
    if (e.hasDuration) return shifted(start, e.duration);
    return shifted(start, start.date ? 86400 : 0);
  }

  void property(const std::string &l) {
    // name;param=value;param="value":value  (a colon inside quotes doesn't end the params)
    size_t colon = std::string::npos;
    bool quoted = false;
    for (size_t i = 0; i < l.size(); i++) {
      if (l[i] == '"') quoted = !quoted;
      else if (l[i] == ':' && !quoted) {
        colon = i;
        break;
      }
    }
    if (colon == std::string::npos) return;
    const std::string head = l.substr(0, colon), value = l.substr(colon + 1);
    std::vector<std::string> params;
    std::string cur;
    quoted = false;
    for (char c : head) {
      if (c == '"') quoted = !quoted;
      else if (c == ';' && !quoted) {
        params.push_back(cur);
        cur.clear();
      } else {
        cur += c;
      }
    }
    params.push_back(cur);
    std::string name = params[0];
    for (char &c : name) c = toupper(c);
    std::string tzid;
    bool dateParam = false;
    for (size_t i = 1; i < params.size(); i++) {
      std::string key = params[i].substr(0, params[i].find('='));
      for (char &c : key) c = toupper(c);
      const std::string val = params[i].find('=') == std::string::npos ? "" : params[i].substr(params[i].find('=') + 1);
      if (key == "TZID") tzid = val;
      if (key == "VALUE" && val == "DATE") dateParam = true;
    }

    if (name == "BEGIN") {
      if (in == OUTSIDE && value == "VEVENT") {
        in = EVENT;
        ev = Event();
        depth = 0;
      } else if (in == OUTSIDE && value == "VTIMEZONE") {
        in = TIMEZONE;
        tzName.clear();
        subs.clear();
        depth = 0;
      } else if (in == TIMEZONE && depth == 0 && (value == "STANDARD" || value == "DAYLIGHT")) {
        sub = Sub();
        sub.daylight = value == "DAYLIGHT";
        depth = 1;
      } else if (in != OUTSIDE) {
        depth++;
      }
      return;
    }
    if (name == "END") {
      if (in == EVENT && depth == 0 && value == "VEVENT") {
        in = OUTSIDE;
        endEvent();
      } else if (in == TIMEZONE && depth == 0 && value == "VTIMEZONE") {
        in = OUTSIDE;
        endZone();
      } else if (in == TIMEZONE && depth == 1 && (value == "STANDARD" || value == "DAYLIGHT")) {
        subs.push_back(sub);
        depth = 0;
      } else if (depth > 0) {
        depth--;
      }
      return;
    }
    if (in == TIMEZONE) {
      if (depth == 0 && name == "TZID") tzName = value;
      else if (depth == 1 && name == "DTSTART") sub.start = parseWhen(value, "", false);
      else if (depth == 1 && name == "TZOFFSETFROM") sub.from = offset(value);
      else if (depth == 1 && name == "TZOFFSETTO") sub.to = offset(value);
      else if (depth == 1 && name == "RRULE") sub.rule = parseRule(value, ""), sub.hasRule = true;
      return;
    }
    if (in != EVENT || depth > 0) return;  // an alarm's own description isn't the event's
    if (name == "UID") ev.uid = value, ev.noUid = false;
    else if (name == "SUMMARY" && !ev.hasSummary) ev.summary = unescape(value).substr(0, MAX_TEXT), ev.hasSummary = true;
    else if (name == "LOCATION" && !ev.hasLocation)
      ev.location = unescape(value).substr(0, MAX_TEXT), ev.hasLocation = true;
    else if (name == "DESCRIPTION" && !ev.hasDescription)
      ev.description = unescape(value).substr(0, MAX_TEXT), ev.hasDescription = true;
    else if (name == "DTSTART") ev.start = parseWhen(value, tzid, dateParam);
    else if (name == "DTEND") ev.end = parseWhen(value, tzid, dateParam);
    else if (name == "DURATION") ev.duration = parseDuration(value), ev.hasDuration = true;
    else if (name == "RRULE") ev.rule = parseRule(value, "");
    else if (name == "RECURRENCE-ID") ev.recurrenceId = parseWhen(value, tzid, dateParam);
    else if (name == "STATUS") ev.cancelled = value == "CANCELLED";
    else if (name == "EXDATE")
      for (const std::string &v : split(value, ',')) ev.exdates.push_back(parseWhen(v, tzid, dateParam));
  }

  // Keeps an event only if it can reach the window (a few days' slack each side for time zones).
  void endEvent() {
    if (ev.rule.until.set && !ev.rule.until.utc && !ev.rule.until.date) ev.rule.until.tzid = ev.start.tzid;
    if (ev.recurrenceId.set) {
      if (ev.recurrenceId.day() >= nowDay - 4 && ev.recurrenceId.day() <= nowDay + DAYS_AHEAD + 4)
        overrides.push_back(ev);
      return;
    }
    bool keep = ev.start.set && ev.start.day() <= nowDay + DAYS_AHEAD + 4;
    if (keep && ev.rule.freq == Rule::NONE) {
      const When end = endOf(ev, ev.start);
      keep = end.day() >= nowDay - 4;
    } else if (keep && ev.rule.until.set) {
      keep = ev.rule.until.day() >= nowDay - 4;
    }
    if (ev.noUid) {  // the website counts each one as itself
      if (keep) masters.push_back(ev);
      return;
    }
    auto it = masterAt.find(ev.uid);
    if (it != masterAt.end()) {  // a later copy replaces the earlier, in its place
      masters[it->second] = keep ? ev : Event();
      return;
    }
    if (!keep) {  // remembered without its text, in case a later copy of it is kept
      Event empty;
      empty.uid = ev.uid;
      ev = empty;
    }
    masterAt[ev.uid] = masters.size();
    masters.push_back(ev);
  }

  void endZone() {
    Zone z;
    const Sub *std_ = nullptr, *dst = nullptr;
    for (const Sub &s : subs) {  // the newest rule of each kind
      const Sub *&slot = s.daylight ? dst : std_;
      if (!slot || (s.hasRule && !slot->hasRule) || (s.hasRule == slot->hasRule && s.start.local > slot->start.local))
        slot = &s;
    }
    if (!std_ && !dst) return;
    if (!std_ || !dst || !std_->hasRule || !dst->hasRule || std_->rule.byDay.empty() || dst->rule.byDay.empty() ||
        std_->rule.byMonth.empty() || dst->rule.byMonth.empty() ||
        (std_->rule.until.set && dst->rule.until.set)) {  // no yearly change (any more): a fixed offset
      const Sub *only = std_ ? std_ : dst;
      if (std_ && dst && dst->start.local > std_->start.local) only = dst;
      z.std = only->to;
    } else {
      z.std = std_->to;
      z.dst = dst->to;
      z.hasDst = true;
      const Secs dstAt = dst->start.local - dst->start.day() * 86400, stdAt = std_->start.local - std_->start.day() * 86400;
      int nth = dst->rule.byDay[0].first;
      z.toDst = {dst->rule.byMonth[0], nth ? nth : 1, dst->rule.byDay[0].second, dstAt};
      nth = std_->rule.byDay[0].first;
      z.toStd = {std_->rule.byMonth[0], nth ? nth : 1, std_->rule.byDay[0].second, stdAt};
    }
    if (!tzName.empty()) zones[tzName] = z;
  }

  // Is `day` (in the event's own calendar) one of its repeats? Leaves out the count and the end date.
  bool onRule(const Rule &r, const When &start, Secs day) const {
    const Secs s = start.day();
    if (day < s) return false;
    if (day == s) return true;  // the first one always counts
    int y, m, d, sy, sm, sd;
    civilFromDays(day, y, m, d);
    civilFromDays(s, sy, sm, sd);
    const int wd = weekday(day);
    auto inList = [](const std::vector<int> &v, int x) { return std::find(v.begin(), v.end(), x) != v.end(); };
    auto dayMatch = [&](int x) { return inList(r.byMonthDay, x) || inList(r.byMonthDay, x - daysInMonth(y, m) - 1); };
    auto byDayMatch = [&](int dd) {
      const int pos = (dd - 1) / 7 + 1, neg = -((daysInMonth(y, m) - dd) / 7 + 1);
      const int w = weekday(daysFromCivil(y, m, dd));
      for (const auto &b : r.byDay)
        if (b.second == w && (b.first == 0 || b.first == pos || b.first == neg)) return true;
      return false;
    };
    if (!r.byMonth.empty() && !inList(r.byMonth, m)) return false;
    switch (r.freq) {
      case Rule::DAILY:
        if ((day - s) % r.interval) return false;
        if (!r.byDay.empty() && !byDayMatch(d)) return false;
        return r.byMonthDay.empty() || dayMatch(d);
      case Rule::WEEKLY: {
        const Secs k = (r.wkst - 4 + 7) % 7;  // the first day since 1970 a week starts on
        if ((floorDiv(day - k, 7) - floorDiv(s - k, 7)) % r.interval) return false;
        if (r.byDay.empty()) return wd == weekday(s);
        for (const auto &b : r.byDay)
          if (b.second == wd) return true;
        return false;
      }
      case Rule::MONTHLY:
      case Rule::YEARLY: {
        if (r.freq == Rule::MONTHLY && ((y - sy) * 12 + (m - sm)) % r.interval) return false;
        if (r.freq == Rule::YEARLY && ((y - sy) % r.interval || (r.byMonth.empty() && m != sm))) return false;
        // The days of this month that fit, then BYSETPOS picks among them.
        std::vector<int> fit;
        for (int dd = 1; dd <= daysInMonth(y, m); dd++) {
          bool ok;
          if (!r.byMonthDay.empty()) ok = dayMatch(dd) && (r.byDay.empty() || byDayMatch(dd));
          else if (!r.byDay.empty()) ok = byDayMatch(dd);
          else ok = dd == sd;
          if (ok) fit.push_back(dd);
        }
        if (r.bySetPos.empty()) return inList(fit, d);
        for (int p : r.bySetPos) {
          const int i = p > 0 ? p - 1 : (int)fit.size() + p;
          if (i >= 0 && i < (int)fit.size() && fit[i] == d) return true;
        }
        return false;
      }
      default:
        return false;
    }
  }

  void expand(std::vector<Line> &out, const Event &ev, Secs first, Secs last) {
    const Rule &r = ev.rule;
    const Secs from = now - 2 * 86400, to = now + (DAYS_AHEAD + 2) * 86400;  // the website's window
    const Secs timeOfDay = ev.start.local - ev.start.day() * 86400;
    // With COUNT: the last day it reaches, counted from the start.
    Secs lastDay = to / 86400 + 2;
    if (r.count > 0) {
      int seen = 0;
      Secs day = ev.start.day();
      for (int guard = 0; guard < 40000 && day <= lastDay; day++, guard++)
        if (onRule(r, ev.start, day) && ++seen == r.count) break;
      lastDay = std::min(lastDay, day);
    }
    for (Secs day = std::max(ev.start.day(), from / 86400 - 2); day <= lastDay; day++) {
      if (!onRule(r, ev.start, day)) continue;
      When at = ev.start;
      at.local = day * 86400 + timeOfDay;
      const Secs utc = utcOf(at);
      if (utc < from || utc > to) continue;
      if (r.until.set && (r.until.date ? at.day() > r.until.day() : utc > utcOf(r.until))) continue;
      bool excluded = false;
      for (const When &x : ev.exdates)
        if (x.date ? x.day() == day : utcOf(x) == utc) excluded = true;
      if (excluded) continue;
      const Event *moved = nullptr;
      for (const Event &o : overrides)
        if (o.uid == ev.uid && (o.recurrenceId.date ? o.recurrenceId.day() == day : utcOf(o.recurrenceId) == utc))
          moved = &o;
      if (moved) {
        if (!moved->cancelled && moved->start.set)
          add(out, *moved, moved->start, moved->end.set ? moved->end : endOf(*moved, moved->start), first, last);
      } else {
        add(out, ev, at, endOf(ev, at), first, last);
      }
    }
  }

  static std::string dayText(Secs day) {
    int y, m, d;
    civilFromDays(day, y, m, d);
    char s[12];
    snprintf(s, sizeof s, "%04d-%02d-%02d", y, m, d);
    return s;
  }

  // One occurrence as lines: a timed one on the day it starts (cut at 23:59 if it runs past midnight), an all-day
  // one on each day it covers.
  void add(std::vector<Line> &out, const Event &e, const When &start, const When &end, Secs first, Secs last) const {
    const std::string fields =
      "," + clean(e.summary, 63) + "," + clean(e.location, 47) + "," + cleanNotes(e.description);
    if (start.date) {
      for (Secs d = start.day(); d < end.day(); d++)
        if (d >= first && d <= last) out.push_back({d * 86400, dayText(d) + ",," + fields});
      return;
    }
    const Zone uk = london();
    const Secs s = utcOf(start), e2 = utcOf(end);
    const Secs sl = s + uk.offsetAt(s), el = e2 + uk.offsetAt(e2);
    const Secs day = floorDiv(sl, 86400);
    if (day < first || day > last) return;
    char times[16];
    const Secs sm = (sl - day * 86400) / 60;
    if (floorDiv(el, 86400) == day) {
      const Secs em = (el - day * 86400) / 60;
      snprintf(times, sizeof times, "%02d:%02d,%02d:%02d", (int)(sm / 60), (int)(sm % 60), (int)(em / 60), (int)(em % 60));
    } else {
      snprintf(times, sizeof times, "%02d:%02d,23:59", (int)(sm / 60), (int)(sm % 60));
    }
    out.push_back({s, dayText(day) + "," + times + fields});
  }
};

}  // namespace ics
