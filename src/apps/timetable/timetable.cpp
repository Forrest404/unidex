// Timetable: next class + countdown from /timetable.csv. B long = rest of today, B short = sync time.
#include "../../core/app.h"
#include "../../core/display.h"
#include "../../core/storage.h"
#include "../../core/theme.h"
#include "clock.h"

struct Class {
  uint8_t wday;  // 0 = Sunday, as in struct tm
  uint16_t start, end;  // minutes since midnight
  char module[24], room[12];
};

struct Upcoming {
  int16_t index;
  int32_t delta;  // minutes from now to the start; negative = in progress
};

static const int MAX_CLASSES = 64, MAX_UPCOMING = 5, DAY_ROWS = 7, ROW_H = 20;
static const int32_t WEEK = 7 * 1440;

// RAM is lost in deep sleep, so the CSV and clock are loaded again on first use after a wake.
static Class classes[MAX_CLASSES];
static int count = -1;  // -1 = not loaded yet
static bool clockReady;
static const char *syncError;  // shown once after a failed sync

enum View : uint8_t { NEXT, DAY };
RTC_DATA_ATTR static uint8_t view, peek, scroll;

static int parseDay(String s) {
  static const char *DAYS[] = {"sun", "mon", "tue", "wed", "thu", "fri", "sat"};
  s.trim();
  s.toLowerCase();
  for (int i = 0; i < 7; i++)
    if (s.startsWith(DAYS[i])) return i;
  return -1;
}

static int parseTime(const String &s) {
  int colon = s.indexOf(':');
  if (colon < 1) return -1;
  int h = s.substring(0, colon).toInt(), m = s.substring(colon + 1).toInt();
  return (h >= 0 && h < 24 && m >= 0 && m < 60) ? h * 60 + m : -1;
}

// Rows that don't parse (including the "day,start,..." header) are skipped.
static void load() {
  count = 0;
  fs::File f = storageOpen("/timetable.csv");
  while (f && f.available() && count < MAX_CLASSES) {
    String line = f.readStringUntil('\n');
    String field[5];
    int n = 0, from = 0;
    while (n < 5) {
      int comma = line.indexOf(',', from);
      field[n] = line.substring(from, comma < 0 ? line.length() : comma);
      field[n++].trim();
      if (comma < 0) break;
      from = comma + 1;
    }
    int wday = parseDay(field[0]), start = parseTime(field[1]), end = parseTime(field[2]);
    if (n < 5 || wday < 0 || start < 0 || end <= start) continue;
    Class &c = classes[count++];
    c.wday = wday;
    c.start = start;
    c.end = end;
    strlcpy(c.module, field[3].c_str(), sizeof c.module);
    strlcpy(c.room, field[4].c_str(), sizeof c.room);
  }
}

static void ensureReady() {
  if (!clockReady) {
    clockBegin();
    clockReady = true;
  }
  if (count < 0) load();
}

static struct tm localNow() {
  time_t t = time(nullptr);
  struct tm now;
  localtime_r(&t, &now);
  return now;
}

static int nowMinutes(const struct tm &now) { return now.tm_hour * 60 + now.tm_min; }

// The next classes across the week, soonest first.
static int findUpcoming(Upcoming *out, const struct tm &now) {
  const int32_t nowWeek = now.tm_wday * 1440 + nowMinutes(now);
  int n = 0;
  for (int i = 0; i < count; i++) {
    const Class &c = classes[i];
    int32_t d = c.wday * 1440 + c.start - nowWeek;
    if (d + (c.end - c.start) <= 0) d += WEEK;  // already over this week
    int pos = n < MAX_UPCOMING ? n++ : MAX_UPCOMING;
    while (pos > 0 && out[pos - 1].delta > d) {
      if (pos < MAX_UPCOMING) out[pos] = out[pos - 1];
      pos--;
    }
    if (pos < MAX_UPCOMING) out[pos] = {(int16_t)i, d};
  }
  return n;
}

// Today's classes that haven't ended, by start time.
static int findToday(int16_t *out, const struct tm &now) {
  int n = 0;
  for (int i = 0; i < count; i++) {
    const Class &c = classes[i];
    if (c.wday != now.tm_wday || c.end <= nowMinutes(now)) continue;
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
  drawHeader(title);
  if (clockValid()) {
    char hm[6];
    formatHm(hm, sizeof hm, nowMinutes(now));
    drawRight(hm, 16);
  }
}

static void drawMessage(const struct tm &now, const char *line1, const char *line2) {
  const int16_t mid = (CONTENT_TOP + CONTENT_BOTTOM) / 2;
  drawTitle("Timetable", now);
  display.setFont(FONT_SMALL);
  drawCentered(line1, mid - 12);
  drawCentered(line2, mid + 12);
}

static void drawNext(const struct tm &now) {
  if (count == 0) {
    drawMessage(now, "no timetable", "add /timetable.csv");
    drawFooter("hold home", "");
    return;
  }
  if (!clockValid()) {
    drawMessage(now, "time not set", syncError ? syncError : "B to sync");
    drawFooter("hold home", "sync");
    return;
  }
  Upcoming up[MAX_UPCOMING];
  int n = findUpcoming(up, now);
  peek %= n;
  const Class &c = classes[up[peek].index];
  const int32_t d = up[peek].delta;

  drawTitle("Timetable", now);
  int16_t x, y;
  uint16_t w, h;
  display.setFont(FONT_LARGE);
  display.getTextBounds(c.module, 0, 0, &x, &y, &w, &h);
  if (w > display.width() - 2 * MARGIN) display.setFont(FONT_SMALL);  // long names drop to the small font
  drawCentered(c.module, 82);

  char when[32], span[16], dur[16];
  if (syncError) {
    snprintf(when, sizeof when, "%s", syncError);
  } else if (d <= 0) {
    formatDuration(dur, sizeof dur, d + (c.end - c.start));
    snprintf(when, sizeof when, "now, ends in %s", dur);
  } else if (d < 1440 - nowMinutes(now)) {
    formatDuration(dur, sizeof dur, d);
    snprintf(when, sizeof when, "in %s", dur);
  } else {
    static const char *DAY_NAMES[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    char hm[6];
    formatHm(hm, sizeof hm, c.start);
    bool tomorrow = c.wday == (now.tm_wday + 1) % 7 && d < 2 * 1440;
    snprintf(when, sizeof when, "%s %s", tomorrow ? "tomorrow" : DAY_NAMES[c.wday], hm);
  }
  char start[6], end[6];
  formatHm(start, sizeof start, c.start);
  formatHm(end, sizeof end, c.end);
  snprintf(span, sizeof span, "%s-%s", start, end);
  char detail[32];
  snprintf(detail, sizeof detail, "%s  %s", span, c.room);

  display.setFont(FONT_SMALL);
  drawCentered(when, 120);
  drawCentered(detail, 144);
  drawFooter("next", "hold: day");
}

static void drawDay(const struct tm &now) {
  int16_t today[MAX_CLASSES];
  int n = findToday(today, now);
  drawTitle("Today", now);
  display.setFont(FONT_SMALL);
  if (n == 0) drawCentered("no more classes today", (CONTENT_TOP + CONTENT_BOTTOM) / 2);
  for (int r = 0; r < DAY_ROWS && scroll + r < n; r++) {
    const Class &c = classes[today[scroll + r]];
    const int16_t top = CONTENT_TOP + 4 + r * ROW_H, baseline = top + 15;
    uint16_t ink = GxEPD_BLACK;
    if (c.start <= nowMinutes(now)) {  // in progress: the one inverted row
      display.fillRect(MARGIN - 4, top, display.width() - 2 * (MARGIN - 4), ROW_H, GxEPD_BLACK);
      ink = GxEPD_WHITE;
    }
    display.setTextColor(ink);
    char hm[6];
    formatHm(hm, sizeof hm, c.start);
    display.setCursor(MARGIN, baseline);
    display.print(hm);
    drawRight(c.room, baseline);

    // Shorten the module name until it fits between the time and the room.
    int16_t x, y;
    uint16_t w, h, roomW;
    display.getTextBounds(c.room, 0, 0, &x, &y, &roomW, &h);
    const int16_t left = MARGIN + 50, room = display.width() - MARGIN - roomW - 6;
    String name = c.module;
    display.getTextBounds(name.c_str(), 0, 0, &x, &y, &w, &h);
    while (name.length() > 1 && left + w > room) {
      name.remove(name.length() - 1);
      display.getTextBounds(name.c_str(), 0, 0, &x, &y, &w, &h);
    }
    display.setCursor(left, baseline);
    display.print(name);
    display.setTextColor(GxEPD_BLACK);
  }
  drawFooter("scroll", "back");
}

static void drawSyncing() {
  drawMessage(localNow(), "syncing time...", "");
}

static void onEnter() {
  view = NEXT;
  peek = scroll = 0;
  syncError = nullptr;
  count = -1;  // re-read the CSV in case it changed
  ensureReady();
}

static Redraw onButton(Event e) {
  ensureReady();
  syncError = nullptr;
  if (view == DAY) {
    if (e == Event::AShort) {
      int16_t today[MAX_CLASSES];
      int n = findToday(today, localNow());
      if (scroll + DAY_ROWS >= n) return Redraw::None;
      scroll++;
    } else {
      view = NEXT;
    }
    return Redraw::Partial;
  }
  if (e == Event::AShort) {
    peek++;
  } else if (e == Event::BLong) {
    view = DAY;
    scroll = 0;
  } else if (e == Event::BShort) {
    displayShow(drawSyncing, false);
    syncError = clockSync();
    peek = 0;
  }
  return Redraw::Partial;
}

static void draw() {
  ensureReady();
  struct tm now = localNow();
  if (view == DAY) drawDay(now);
  else drawNext(now);
}

static void onExit() {}

extern const App timetableApp = {"Timetable", ICON_TIMETABLE, onEnter, onButton, draw, onExit};
