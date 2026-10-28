#pragma once
#include <cstdint>
#include <string>

// Reading the Timetable's files, without the screen: timetable.csv rows are weekly (day,start,end,module,room) and
// events.csv rows are dated (YYYY-MM-DD,start,end,title,location[,notes]; empty times = all day). No Arduino in
// here, so tools/timetabletest checks it on a computer.
namespace timetable {

// Minutes since midnight from "9:05" or "09:05" ("24:00" is the end of the day, 1440); -1 if it isn't a time.
inline int parseTime(const std::string &s) {
  const size_t colon = s.find(':');
  if (colon < 1 || colon > 2 || colon == std::string::npos || s.size() != colon + 3) return -1;
  for (size_t i = 0; i < s.size(); i++)
    if (i != colon && (s[i] < '0' || s[i] > '9')) return -1;
  const int h = std::stoi(s.substr(0, colon)), m = std::stoi(s.substr(colon + 1));
  if (h == 24 && m == 0) return 1440;
  return h < 24 && m < 60 ? h * 60 + m : -1;
}

inline std::string trim(const std::string &s) {
  const size_t a = s.find_first_not_of(" \t\r"), b = s.find_last_not_of(" \t\r");
  return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}

// Up to six comma-separated fields. A field in double quotes can hold commas ("" is a quote inside one), and the
// sixth (an event's notes) is the rest of the line.
inline int splitCsv(const std::string &line, std::string *field) {
  int n = 0;
  size_t i = 0;
  while (n < 6) {
    std::string f;
    while (i < line.size() && line[i] == ' ') i++;
    if (n == 5) {  // the notes: everything left
      field[n++] = trim(line.substr(i));
      break;
    }
    if (i < line.size() && line[i] == '"') {
      for (i++; i < line.size(); i++) {
        if (line[i] == '"' && i + 1 < line.size() && line[i + 1] == '"') f += '"', i++;
        else if (line[i] == '"') break;
        else f += line[i];
      }
      i++;  // the closing quote
      while (i < line.size() && line[i] != ',') i++;  // anything after it, up to the comma, is dropped
    } else {
      while (i < line.size() && line[i] != ',') f += line[i++];
      f = trim(f);
    }
    field[n++] = f;
    if (i >= line.size()) break;
    i++;  // the comma
  }
  return n;
}

inline int parseDay(std::string s) {
  static const char *DAYS[] = {"sun", "mon", "tue", "wed", "thu", "fri", "sat"};
  s = trim(s);
  for (char &c : s) c = (char)tolower((unsigned char)c);
  for (int i = 0; i < 7; i++)
    if (s.compare(0, 3, DAYS[i]) == 0) return i;
  return -1;
}

// Days since 1970-01-01 (Howard Hinnant's days_from_civil).
inline int32_t daysFromCivil(int y, int m, int d) {
  y -= m <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const int yoe = y - era * 400;
  const int doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  return era * 146097 + yoe * 365 + yoe / 4 - yoe / 100 + doy - 719468;
}

inline int32_t parseDate(const std::string &s) {  // YYYY-MM-DD
  if (s.size() != 10 || s[4] != '-' || s[7] != '-') return -1;
  for (int i : {0, 1, 2, 3, 5, 6, 8, 9})
    if (s[i] < '0' || s[i] > '9') return -1;
  const int y = std::stoi(s.substr(0, 4)), m = std::stoi(s.substr(5, 2)), d = std::stoi(s.substr(8, 2));
  return (y >= 2024 && m >= 1 && m <= 12 && d >= 1 && d <= 31) ? daysFromCivil(y, m, d) : -1;
}

struct Row {
  int32_t date;  // days since 1970 for an event, -1 for a weekly class
  int wday;      // 0 = Sunday
  bool allDay;
  int start, end;  // minutes since midnight
  std::string module, room, notes;
};

// One line of either file; false for one that doesn't parse (a header, a bad day, date or time).
inline bool parseRow(const std::string &line, bool dated, Row &r) {
  std::string f[6];
  const int n = splitCsv(line, f);
  if (n < 5) return false;
  r.date = dated ? parseDate(f[0]) : -1;
  if (dated && r.date < 0) return false;
  r.allDay = dated && f[1].empty() && f[2].empty();
  r.wday = dated ? (int)((r.date % 7 + 11) % 7) : parseDay(f[0]);  // 1970-01-01 was a Thursday
  r.start = r.allDay ? 0 : parseTime(f[1]);
  r.end = r.allDay ? 1440 : parseTime(f[2]);
  if (r.wday < 0 || r.start < 0 || r.end < 0 || r.start >= 1440) return false;
  // Ending at midnight (00:00) or after it: cut at the end of the day, as the website does with events.
  if (r.end < r.start || (r.end == 0 && r.start > 0)) r.end = 1440;
  if (r.end <= r.start) return false;  // no length
  r.module = f[3];
  r.room = f[4];
  r.notes = n > 5 ? f[5] : "";
  return true;
}

}  // namespace timetable
