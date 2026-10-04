// Timetable: the next class or event as a card (countdown, title, time, place, what's after).
// Weekly classes from /timetable.csv, dated events from /events.csv (Mac sync or the website).
// A = next event, B = details (full title, place, notes), B long = rest of today.
#include "../../core/app.h"
#include "../../core/clock.h"
#include "../../core/display.h"
#include "../../core/launcher.h"
#include "../../core/power.h"
#include "../../core/storage.h"
#include "../../core/theme.h"
#include "../../core/usbsync.h"

struct Class {
  int32_t date;  // days since 1970-01-01 for a calendar event; -1 = weekly class
  uint8_t wday;  // 0 = Sunday, as in struct tm
  bool allDay;
  uint16_t start, end;  // minutes since midnight
  char module[64], room[48], notes[161];  // title, location, notes (events.csv field 6)
};

struct Upcoming {
  int16_t index;
  int32_t delta;  // minutes from now to the start; negative = in progress
};

static const int MAX_CLASSES = 96, MAX_UPCOMING = 5, DAY_ROWS = 6, ROW_H = 20;
static const int DETAIL_LINES = 40, PAGE_LINES = 7, LINE_H = 18;  // details screen, small font
static const int32_t WEEK = 7 * 1440;

// RAM is lost in deep sleep, so the CSV is loaded again on first use after a wake.
static Class classes[MAX_CLASSES];
static int count = -1;  // -1 = not loaded yet
static uint32_t loadedGeneration;  // reload when the USB sync brings new events
static int drawnMinute = -1;       // the minute on screen, for the live countdown

enum View : uint8_t { NEXT, DAY, DETAIL };
RTC_DATA_ATTR static uint8_t view, peek, scroll, page, row, detailFrom;
RTC_DATA_ATTR static int16_t detailClass;   // the event the details screen shows
RTC_DATA_ATTR static int32_t detailDelta;   // minutes from now to its start, when it was opened

// The next few events, kept through sleep for the home screen's line (it can't read the card).
struct Soon {
  char title[32];
  time_t start, end;
};
RTC_DATA_ATTR static Soon soon[3];
RTC_DATA_ATTR static int8_t soonCount = -1;  // -1 = not worked out since power-up

static int parseDay(String s) {
  static const char *DAYS[] = {"sun", "mon", "tue", "wed", "thu", "fri", "sat"};
  s.trim();
  s.toLowerCase();
  for (int i = 0; i < 7; i++)
    if (s.startsWith(DAYS[i])) return i;
  return -1;
}

// Days since 1970-01-01 (Howard Hinnant's days_from_civil).
static int32_t daysFromCivil(int y, int m, int d) {
  y -= m <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const int yoe = y - era * 400;
  const int doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  return era * 146097 + yoe * 365 + yoe / 4 - yoe / 100 + doy - 719468;
}

static int32_t parseDate(const String &s) {  // YYYY-MM-DD
  if (s.length() != 10 || s[4] != '-' || s[7] != '-') return -1;
  int y = s.substring(0, 4).toInt(), m = s.substring(5, 7).toInt(), d = s.substring(8, 10).toInt();
  return (y >= 2024 && m >= 1 && m <= 12 && d >= 1 && d <= 31) ? daysFromCivil(y, m, d) : -1;
}

static int parseTime(const String &s) {
  int colon = s.indexOf(':');
  if (colon < 1) return -1;
  int h = s.substring(0, colon).toInt(), m = s.substring(colon + 1).toInt();
  return (h >= 0 && h < 24 && m >= 0 && m < 60) ? h * 60 + m : -1;
}

static int splitCsv(const String &line, String *field) {
  int n = 0, from = 0;
  while (n < 6) {
    int comma = line.indexOf(',', from);
    field[n] = line.substring(from, comma < 0 ? line.length() : comma);
    field[n++].trim();
    if (comma < 0) break;
    from = comma + 1;
  }
  return n;
}

// timetable.csv rows are weekly (day,start,end,module,room); events.csv rows are dated
// (YYYY-MM-DD,start,end,title,location[,notes]; empty times = all day). Rows that don't parse,
// including header rows, are skipped.
static void loadFile(const char *path, bool dated) {
  fs::File f = storageOpen(path);
  while (f && f.available() && count < MAX_CLASSES) {
    String field[6];
    const int n = splitCsv(f.readStringUntil('\n'), field);
    if (n < 5) continue;
    Class c = {};
    c.date = dated ? parseDate(field[0]) : -1;
    c.allDay = dated && field[1].isEmpty() && field[2].isEmpty();
    int wday = dated ? (c.date + 4) % 7 : parseDay(field[0]);  // 1970-01-01 was a Thursday
    int start = c.allDay ? 0 : parseTime(field[1]), end = c.allDay ? 1440 : parseTime(field[2]);
    if ((dated && c.date < 0) || wday < 0 || start < 0 || end <= start) continue;
    c.wday = wday;
    c.start = start;
    c.end = end;
    strlcpy(c.module, field[3].c_str(), sizeof c.module);
    strlcpy(c.room, field[4].c_str(), sizeof c.room);
    if (n > 5) strlcpy(c.notes, field[5].c_str(), sizeof c.notes);
    classes[count++] = c;
  }
}

static void load() {
  count = 0;
  loadFile("/timetable.csv", false);
  loadFile("/events.csv", true);
  loadedGeneration = usbSyncGeneration();
}

static void ensureReady() {
  if (count < 0 || loadedGeneration != usbSyncGeneration()) load();
}

static int nowMinutes(const struct tm &now) { return now.tm_hour * 60 + now.tm_min; }

static int32_t today(const struct tm &now) { return daysFromCivil(now.tm_year + 1900, now.tm_mon + 1, now.tm_mday); }

// The next classes and timed events, soonest first. All-day events are left out: they'd always be "now".
static int findUpcoming(Upcoming *out, const struct tm &now) {
  const int32_t nowWeek = now.tm_wday * 1440 + nowMinutes(now);
  int n = 0;
  for (int i = 0; i < count; i++) {
    const Class &c = classes[i];
    if (c.allDay) continue;
    int32_t d;
    if (c.date >= 0) {
      d = (c.date - today(now)) * 1440 + c.start - nowMinutes(now);
      if (d + (c.end - c.start) <= 0) continue;  // already over
    } else {
      d = c.wday * 1440 + c.start - nowWeek;
      if (d + (c.end - c.start) <= 0) d += WEEK;  // already over this week
    }
    int pos = n < MAX_UPCOMING ? n++ : MAX_UPCOMING;
    while (pos > 0 && out[pos - 1].delta > d) {
      if (pos < MAX_UPCOMING) out[pos] = out[pos - 1];
      pos--;
    }
    if (pos < MAX_UPCOMING) out[pos] = {(int16_t)i, d};
  }
  return n;
}

// Today's classes and events that haven't ended, by start time (all-day events first).
static int findToday(int16_t *out, const struct tm &now) {
  int n = 0;
  for (int i = 0; i < count; i++) {
    const Class &c = classes[i];
    bool isToday = c.date >= 0 ? c.date == today(now) : c.wday == now.tm_wday;
    if (!isToday || c.end <= nowMinutes(now)) continue;
    int pos = n++;
    while (pos > 0 && classes[out[pos - 1]].start > c.start) {
      out[pos] = out[pos - 1];
      pos--;
    }
    out[pos] = i;
  }
  return n;
}

static void formatDuration(char *buf, size_t len, int32_t min) {
  if (min < 60) snprintf(buf, len, "%ld min", (long)min);
  else if (min % 60 == 0) snprintf(buf, len, "%ld h", (long)(min / 60));
  else snprintf(buf, len, "%ld h %ld min", (long)(min / 60), (long)(min % 60));
}

static void formatHm(char *buf, size_t len, int min) { snprintf(buf, len, "%02d:%02d", min / 60, min % 60); }

static void drawTitle(const char *title, const struct tm &now) {
  char hm[6] = "";
  if (clockValid()) formatHm(hm, sizeof hm, nowMinutes(now));
  drawHeader(title, hm);
  drawnMinute = clockValid() ? now.tm_min : -1;
}

// "IN 42 MIN" / "NOW, ENDS IN 20 MIN" / "TOMORROW 09:30" / "TUE 09:00".
static void formatWhen(char *buf, size_t len, const Class &c, int32_t d, const struct tm &now) {
  static const char *DAYS[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
  char dur[16], hm[6];
  if (d <= 0) {
    formatDuration(dur, sizeof dur, d + (c.end - c.start));
    snprintf(buf, len, "NOW, ENDS IN %s", dur);
  } else if (d < 1440 - nowMinutes(now)) {
    formatDuration(dur, sizeof dur, d);
    snprintf(buf, len, "IN %s", dur);
  } else {
    formatHm(hm, sizeof hm, c.start);
    bool tomorrow = c.wday == (now.tm_wday + 1) % 7 && d < 2 * 1440;
    snprintf(buf, len, "%s %s", tomorrow ? "TOMORROW" : DAYS[c.wday], hm);
  }
  for (char *p = buf; *p; p++) *p = toupper(*p);
}

static void formatSpan(char *buf, size_t len, const Class &c) {
  char a[6], b[6];
  formatHm(a, sizeof a, c.start);
  formatHm(b, sizeof b, c.end);
  snprintf(buf, len, "%s-%s", a, b);
}

// Remembers the next few events for the home screen's line.
static void rememberSoon(const Upcoming *up, int n) {
  const time_t now = time(nullptr);
  soonCount = min(n, 3);
  for (int i = 0; i < soonCount; i++) {
    const Class &c = classes[up[i].index];
    strlcpy(soon[i].title, c.module, sizeof soon[i].title);
    soon[i].start = now - now % 60 + (time_t)up[i].delta * 60;
    soon[i].end = soon[i].start + (time_t)(c.end - c.start) * 60;
  }
}

// The empty and "can't show it" states. Returns the number of upcoming events (0 = the screen is drawn).
static int upcomingOrEmpty(Upcoming *up, const struct tm &now) {
  if (count == 0) {
    soonCount = 0;
    drawTitle("Timetable", now);
    drawEmpty("Nothing coming up", "Add events on the", "website or your Mac");
    drawHints("", "", "home", "");
    return 0;
  }
  if (!clockValid()) {
    drawTitle("Timetable", now);
    drawEmpty("Time not set", "Press B to sync it,", "or set it in Settings");
    drawHints("", "sync", "home", "");
    return 0;
  }
  const int n = findUpcoming(up, now);
  rememberSoon(up, n);
  if (n == 0) {  // e.g. only all-day or finished events left
    drawTitle("Timetable", now);
    drawEmpty("Nothing coming up", "Only all-day or past", "events are left");
    drawHints("", "", "home", "today");
  }
  return n;
}

// The card: countdown, the title (large, 2 lines), time and length, place, and a peek at what's after.
static void drawNext(const struct tm &now) {
  Upcoming up[MAX_UPCOMING];
  const int n = upcomingOrEmpty(up, now);
  if (n == 0) return;
  peek %= n;
  const Class &c = classes[up[peek].index];
  const int16_t width = display.width() - 2 * MARGIN;
  drawTitle("Timetable", now);

  char when[32], pos[8];
  formatWhen(when, sizeof when, c, up[peek].delta, now);
  display.setFont(FONT_SMALL);
  display.setCursor(MARGIN, CONTENT_TOP + 16);
  display.print(when);
  if (n > 1) {  // which of the upcoming events this is
    snprintf(pos, sizeof pos, "%d/%d", peek + 1, n);
    display.setFont(FONT_TINY);
    drawRight(pos, CONTENT_TOP + 16);
  }

  // The title: large on up to 2 lines, else small on up to 3, centred between the countdown and the rule.
  String lines[3];
  int used, lineH, ascent;
  display.setFont(FONT_LARGE);
  if ((used = wrapText(c.module, width, lines, 2)) <= 2) {
    lineH = 28, ascent = 20;
  } else {
    display.setFont(FONT_SMALL);
    used = min(wrapText(c.module, width, lines, 3), 3);
    lineH = 18, ascent = 13;
  }
  const int16_t top = CONTENT_TOP + 22, bottom = CONTENT_TOP + 78;
  const int16_t first = top + (bottom - top - used * lineH) / 2 + ascent;
  for (int i = 0; i < used; i++) display.setCursor(MARGIN, first + i * lineH), display.print(lines[i]);
  display.drawFastHLine(MARGIN, CONTENT_TOP + 81, 40, GxEPD_BLACK);

  char span[16], dur[16], line[48];
  formatSpan(span, sizeof span, c);
  formatDuration(dur, sizeof dur, c.end - c.start);
  snprintf(line, sizeof line, "%s   %s", span, dur);
  display.setFont(FONT_SMALL);
  int16_t y = CONTENT_TOP + 98;
  display.setCursor(MARGIN, y);
  display.print(line);
  if (*c.room) display.setCursor(MARGIN, y += 18), display.print(fitText(c.room, width));

  if (n > 1) {  // what's after this one
    const Class &next = classes[up[(peek + 1) % n].index];
    char at[32], hm[6];
    formatWhen(at, sizeof at, next, up[(peek + 1) % n].delta, now);
    formatHm(hm, sizeof hm, next.start);
    snprintf(line, sizeof line, "then %s  ", up[(peek + 1) % n].delta < 1440 - nowMinutes(now) ? hm : at);
    display.setCursor(MARGIN, y + 18);
    display.print(fitText((String(line) + next.module).c_str(), width));
  }
  drawHints(n > 1 ? "next" : "", "details", "home", "today");
}

// Everything about the event, as lines of small text: title, date, time and length, place, notes.
static int detailLines(const Class &c, int32_t d, String *out) {
  static const char *DAYS[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  static const char *MONTHS[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  const int16_t width = display.width() - 2 * MARGIN;
  int n = 0;
  auto add = [&](const char *text) {
    String part[DETAIL_LINES];
    int got = wrapText(text, width, part, DETAIL_LINES);
    for (int i = 0; i < got && i < DETAIL_LINES && n < DETAIL_LINES; i++) out[n++] = part[i];
  };
  add(c.module);
  if (n < DETAIL_LINES) out[n++] = "";
  // The date it happens on: today plus however many days ahead the start is.
  time_t t = time(nullptr) + (time_t)d * 60;
  struct tm on;
  localtime_r(&t, &on);
  char date[24], span[16], dur[16], line[64];
  snprintf(date, sizeof date, "%s %d %s", DAYS[on.tm_wday], on.tm_mday, MONTHS[on.tm_mon]);
  add(date);
  formatSpan(span, sizeof span, c);
  formatDuration(dur, sizeof dur, c.end - c.start);
  snprintf(line, sizeof line, "%s  (%s)", span, dur);
  add(line);
  if (*c.room) add(c.room);
  if (*c.notes) {
    if (n < DETAIL_LINES) out[n++] = "";
    add(c.notes);
  }
  return n;
}

static void drawDetail(const struct tm &now) {
  if (detailClass < 0 || detailClass >= count) {  // the events changed underneath: back to the card
    view = NEXT;
    drawNext(now);
    return;
  }
  String lines[DETAIL_LINES];
  display.setFont(FONT_SMALL);
  const int total = detailLines(classes[detailClass], detailDelta, lines);
  const int pages = (total + PAGE_LINES - 1) / PAGE_LINES;
  if (page >= pages) page = 0;
  char pos[8] = "";
  if (pages > 1) snprintf(pos, sizeof pos, "%d/%d", page + 1, pages);
  drawHeader("Details", pos);
  drawnMinute = -1;
  display.setFont(FONT_SMALL);
  for (int i = 0; i < PAGE_LINES && page * PAGE_LINES + i < total; i++) {
    display.setCursor(MARGIN, CONTENT_TOP + 18 + i * LINE_H);
    display.print(lines[page * PAGE_LINES + i]);
  }
  drawHints(pages > 1 ? "more" : "", "", "back", "");
}

static void drawDay(const struct tm &now) {
  int16_t today[MAX_CLASSES];
  const int n = findToday(today, now);
  drawTitle("Today", now);
  if (n == 0) {
    drawEmpty("Nothing left today", "Enjoy the free time", "");
    drawHints("", "", "back", "");
    return;
  }
  row %= n;
  if (row < scroll) scroll = row;  // keep the selected row on screen
  if (row >= scroll + DAY_ROWS) scroll = row - DAY_ROWS + 1;
  display.setFont(FONT_SMALL);
  for (int r = 0; r < DAY_ROWS && scroll + r < n; r++) {
    const Class &c = classes[today[scroll + r]];
    const int16_t top = CONTENT_TOP + 4 + r * ROW_H, baseline = top + 15;
    const bool selected = scroll + r == row;
    if (selected) display.fillRect(MARGIN - 4, top, display.width() - 2 * (MARGIN - 4), ROW_H, GxEPD_BLACK);
    display.setTextColor(selected ? GxEPD_WHITE : GxEPD_BLACK);
    char hm[8];
    if (c.allDay) strcpy(hm, "all day");
    else if (c.start <= nowMinutes(now)) strcpy(hm, "now");  // in progress
    else formatHm(hm, sizeof hm, c.start);
    display.setCursor(MARGIN, baseline);
    display.print(hm);
    // The title gets the rest of the row (the room is on the details screen).
    const int16_t left = MARGIN + 54;
    display.setCursor(left, baseline);
    display.print(fitText(c.module, display.width() - MARGIN - left));
    display.setTextColor(GxEPD_BLACK);
  }
  drawHints(n > 1 ? "next" : "", "details", "back", "");
}

static void drawSyncing() {
  drawTitle("Timetable", clockLocal());
  drawEmpty("Setting the clock", "WiFi, then a time server", "up to 30 s");
  drawProgress(HINTS_TOP - 14, -1);
  drawHints("", "", "", "");
}

static void openDetail(int16_t cls, int32_t delta, uint8_t from) {
  detailClass = cls;
  detailDelta = delta;
  detailFrom = from;
  view = DETAIL;
  page = 0;
}

static void onEnter() {
  view = NEXT;
  peek = scroll = page = row = 0;
  count = -1;  // re-read the CSV in case it changed
  ensureReady();
}

static Redraw onButton(Event e) {
  ensureReady();
  const struct tm now = clockLocal();
  if (view == DAY) {
    int16_t today[MAX_CLASSES];
    const int n = findToday(today, now);
    if (!n) return Redraw::None;
    if (e == Event::AShort && n > 1) {
      row = (row + 1) % n;
    } else if (e == Event::BShort) {
      const Class &c = classes[today[row % n]];
      openDetail(today[row % n], c.start - nowMinutes(now), DAY);
    } else {
      return Redraw::None;
    }
    return Redraw::Partial;
  }
  if (view == DETAIL) {
    if (e != Event::AShort) return Redraw::None;
    page++;  // drawDetail wraps back to the first page after the last
    return Redraw::Partial;
  }
  if (count == 0) return Redraw::None;
  if (!clockValid()) {  // "Time not set": B syncs over WiFi
    if (e != Event::BShort) return Redraw::None;
    displayShow(drawSyncing, false);
    const char *err = clockSync();
    powerActivity();  // the sync took a while; don't sleep straight away
    launcherToast(err ? err : "Clock set");
    peek = 0;
    return Redraw::Partial;
  }
  Upcoming up[MAX_UPCOMING];
  const int n = findUpcoming(up, now);
  if (e == Event::BLong) {
    view = DAY;
    scroll = row = 0;
  } else if (n == 0) {
    return Redraw::None;
  } else if (e == Event::AShort && n > 1) {
    peek = (peek + 1) % n;
  } else if (e == Event::BShort) {
    openDetail(up[peek % n].index, up[peek % n].delta, NEXT);
  } else {
    return Redraw::None;
  }
  return Redraw::Partial;
}

static Redraw onBack() {
  if (view == NEXT) return Redraw::Exit;
  view = view == DETAIL ? detailFrom : NEXT;
  return Redraw::Partial;
}

static void draw() {
  ensureReady();
  struct tm now = clockLocal();
  if (view == DAY) drawDay(now);
  else if (view == DETAIL) drawDetail(now);
  else drawNext(now);
}

// The countdown and "now" markers move on each minute.
static Redraw tick() {
  if (view == DETAIL || drawnMinute < 0 || !clockValid() || clockLocal().tm_min == drawnMinute) return Redraw::None;
  return Redraw::Tick;
}

static void onExit() {}

// "In 12 min: Maths", "Now: Maths", "Tue 09:00: Maths", from the events remembered on the last visit. The time
// goes first so a long title is what gets cut.
static void status(char *out, size_t len) {
  if (!clockValid()) {
    snprintf(out, len, "Time not set");
    return;
  }
  const time_t now = time(nullptr);
  for (int i = 0; i < soonCount; i++) {
    const Soon &s = soon[i];
    if (now >= s.end) continue;
    if (now >= s.start) {
      snprintf(out, len, "Now: %s", s.title);
      return;
    }
    const long mins = (s.start - now + 59) / 60;
    char when[24];
    if (mins < 60) snprintf(when, sizeof when, "In %ld min", mins);
    else if (mins < 12 * 60) snprintf(when, sizeof when, "In %ld h %02ld", mins / 60, mins % 60);
    else {
      struct tm at;
      localtime_r(&s.start, &at);
      strftime(when, sizeof when, "%a %H:%M", &at);
    }
    snprintf(out, len, "%s: %s", when, s.title);
    return;
  }
  if (soonCount == 0) snprintf(out, len, "Nothing coming up");
}

extern const App timetableApp = {"Timetable", ICON_TIMETABLE, onEnter, onButton, draw, onExit, onBack, status, tick, true};
