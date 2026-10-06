// Settings: date and time, sleep, invert, battery and info, reset data.
// A = next row or field, B = change, open or clear, hold A = back (in the date editor: leave without saving).
#include "../../core/app.h"
#include "../../core/battery.h"
#include "../../core/clock.h"
#include "../../core/devtools.h"
#include "../../core/display.h"
#include "../../core/launcher.h"
#include "../../core/power.h"
#include "../../core/storage.h"
#include "../../core/theme.h"
#include "../../core/usbsync.h"
#include <unidex_version.h>

static const char *VERSION = UNIDEX_VERSION;  // from git, tools/version.py
static const int SLEEP_CHOICES[] = {10, 20, 30, 60};
static const int ROW_H = 26;

enum Screen : uint8_t { LIST, DATETIME, INFO, RESET, CONFIRM };
enum Row : uint8_t { DATE, SLEEP, INVERT, INFO_ROW, RESET_ROW, ROWS };
enum Reset : uint8_t { TALLY, DEX, CALENDAR, EVERYTHING, RESETS };
static const char *RESET_NAMES[] = {"Chooser tally", "Dex", "Calendar", "Everything"};
enum Field : uint8_t { YEAR, MONTH, DAY, HOUR, MINUTE, SAVE, FIELDS };

RTC_DATA_ATTR static uint8_t screen, cursor, resetCursor, field;
RTC_DATA_ATTR static int16_t value[MINUTE + 1];  // the date being edited: y, m, d, h, min

static int daysIn(int year, int month) {
  static const int DAYS[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
  return DAYS[month - 1] + (month == 2 && leap);
}

// One settings row: label left, value right; the selected row is inverted.
static void drawRow(int i, bool selected, const char *label, const char *val) {
  const int16_t top = CONTENT_TOP + 4 + i * ROW_H, baseline = top + 17;
  if (selected) display.fillRect(MARGIN - 4, top, display.width() - 2 * (MARGIN - 4), ROW_H - 2, BLACK);
  display.setTextColor(selected ? WHITE : BLACK);
  display.setCursor(MARGIN, baseline);
  display.print(label);
  drawRight(val, baseline);
  display.setTextColor(BLACK);
}

static void drawList() {
  drawHeader("Settings");
  display.setFont(FONT_SMALL);
  char time[8] = "not set", sleep[8];
  if (clockValid()) {
    struct tm now = clockLocal();
    snprintf(time, sizeof time, "%02d:%02d", now.tm_hour, now.tm_min);
  }
  snprintf(sleep, sizeof sleep, "%d s", powerSleepSeconds());
  drawRow(DATE, cursor == DATE, "Date & time", time);
  drawRow(SLEEP, cursor == SLEEP, "Sleep after", sleep);
  drawRow(INVERT, cursor == INVERT, "Invert", display.inverted ? "on" : "off");
  drawRow(INFO_ROW, cursor == INFO_ROW, "Battery & info", ">");
  drawRow(RESET_ROW, cursor == RESET_ROW, "Reset data", ">");
  drawHints("next", cursor == SLEEP || cursor == INVERT ? "change" : "open", "");
}

// Text pieces in a line, centred; the piece at `active` is inverted in a box `boxH` tall.
static void drawPieces(const char *const *pieces, int n, int active, int16_t cy, int16_t boxH) {
  String line;
  for (int i = 0; i < n; i++) line += pieces[i];
  int16_t x, y;
  uint16_t w, h;
  display.getTextBounds(line.c_str(), 0, 0, &x, &y, &w, &h);
  int16_t cursorX = (display.width() - w) / 2 - x;
  const int16_t baseline = cy + h / 2 - (h + y);
  for (int i = 0; i < n; i++) {
    int16_t px, py;
    uint16_t pw, ph;
    display.getTextBounds(pieces[i], cursorX, baseline, &px, &py, &pw, &ph);
    if (i == active) {
      display.fillRect(px - 3, cy - boxH / 2, pw + 6, boxH, BLACK);
      display.setTextColor(WHITE);
    }
    display.setCursor(cursorX, baseline);
    display.print(pieces[i]);
    cursorX = display.getCursorX();
    display.setTextColor(BLACK);
  }
}

static void drawDateTime() {
  drawHeader("Date & time");
  char y[6], mo[4], d[4], hh[4], mi[4];
  snprintf(y, sizeof y, "%d", value[YEAR]);
  snprintf(mo, sizeof mo, "%02d", value[MONTH]);
  snprintf(d, sizeof d, "%02d", value[DAY]);
  snprintf(hh, sizeof hh, "%02d", value[HOUR]);
  snprintf(mi, sizeof mi, "%02d", value[MINUTE]);
  const char *date[] = {y, "-", mo, "-", d}, *time[] = {hh, ":", mi};
  display.setFont(FONT_LARGE);
  drawPieces(date, 5, field <= DAY ? field * 2 : -1, 56, 36);
  drawPieces(time, 3, field == HOUR ? 0 : field == MINUTE ? 2 : -1, 100, 36);
  display.setFont(FONT_SMALL);
  const char *save[] = {"Save"};
  drawPieces(save, 1, field == SAVE ? 0 : -1, 140, 24);
  drawHints("next", field == SAVE ? "save" : "+1", field == SAVE ? "" : "-1");
}

static void drawInfo() {
  drawHeader("Battery & info");
  display.setFont(FONT_SMALL);
  char battery[24], storage[24], sync[12] = "not yet";
  const int mv = batteryMillivolts(), pct = batteryPercent();
  if (pct < 0) snprintf(battery, sizeof battery, "none");
  else snprintf(battery, sizeof battery, "%d.%02d V %d%%", mv / 1000, mv % 1000 / 10, pct);
  uint64_t used, total;
  if (storageUsage(used, total))
    snprintf(storage, sizeof storage, "%llu / %llu MB", used >> 20, total >> 20);
  else
    snprintf(storage, sizeof storage, "no card");
  if (time_t t = usbSyncLastTime()) {
    struct tm s, now = clockLocal();
    localtime_r(&t, &s);
    if (s.tm_yday == now.tm_yday && s.tm_year == now.tm_year) strftime(sync, sizeof sync, "%H:%M", &s);
    else strftime(sync, sizeof sync, "%d %b", &s);
  }
  drawRow(0, false, pct >= 0 && batteryCharging() ? "On USB" : "Battery", battery);
  String version = VERSION;  // a build between releases, "v1.4-3-g13bf907": shown as "v1.4-3", which fits
  if (version.indexOf("-g") > 0) version.remove(version.indexOf("-g"));
  drawRow(1, false, "Firmware", version.c_str());
  drawRow(2, false, "Storage", storage);
  drawRow(3, false, "Mac sync", sync);
  drawHints("", "refresh", "");
}

static void drawResetRows() {
  drawHeader("Reset data");
  display.setFont(FONT_SMALL);
  for (int i = 0; i < RESETS; i++) drawRow(i, i == resetCursor, RESET_NAMES[i], "");
}

static void drawReset() {
  drawResetRows();
  drawHints("next", "clear", "");
}

static void drawConfirm() {
  static const char *TITLES[] = {"Clear the tally?", "Clear the Dex?", "Clear the calendar?", "Reset everything?"};
  static const char *WHAT[] = {"Wins go back to 0.", "Forget all networks.", "Synced events go.",
                               "Settings, Dex, tally,"};
  drawResetRows();
  drawSheet(TITLES[resetCursor], WHAT[resetCursor],
            resetCursor == EVERYTHING ? "calendar. Keys stay." : "Can't be undone.");
  drawHints("keep", resetCursor == EVERYTHING ? "reset" : "clear", "");
}

// Clears one thing and says what happened (the honest result: nothing there, no card, done).
static void clear(uint8_t what) {
  if (devDryRun()) {
    launcherToast("Dry run: not cleared");
    return;
  }
  if (what == TALLY) {
    bool any = false;
    char key[8];
    for (int i = 1; i <= 6; i++) {
      snprintf(key, sizeof key, "ch_w%d", i);
      any |= storageGetInt(key, 0) != 0;
      storageRemoveKey(key);
    }
    launcherToast(any ? "Tally cleared" : "Nothing to clear");
    return;
  }
  if (what == DEX || what == CALENDAR) {
    const char *path = what == DEX ? "/dex.csv" : "/events.csv";
    if (!storageCardMount()) launcherToast("No SD card");
    else if (!storageExists(path)) launcherToast("Nothing to clear");
    else launcherToast(storageRemove(path) ? (what == DEX ? "Dex cleared" : "Calendar cleared") : "Couldn't clear it");
    if (what == CALENDAR) storageRemoveKey("events_crc");  // so the next Mac sync writes the events again
    return;
  }
  storageRemove("/dex.csv");
  storageRemove("/events.csv");
  storageClearKeys();  // settings, badge choice, tally, Dex salt (not the WiFi and keys: they're separate)
  systemRestart("Starting fresh");
}

static void startEditing() {
  struct tm now = clockLocal();
  bool set = clockValid();
  value[YEAR] = set ? now.tm_year + 1900 : 2026;
  value[MONTH] = set ? now.tm_mon + 1 : 1;
  value[DAY] = set ? now.tm_mday : 1;
  value[HOUR] = set ? now.tm_hour : 12;
  value[MINUTE] = set ? now.tm_min : 0;
  field = YEAR;
}

static void step(int delta) {
  static const int16_t LOW_[] = {2024, 1, 1, 0, 0}, HIGH_[] = {2099, 12, 31, 23, 59};
  int16_t high = field == DAY ? daysIn(value[YEAR], value[MONTH]) : HIGH_[field];
  int16_t span = high - LOW_[field] + 1;
  value[field] = LOW_[field] + ((value[field] - LOW_[field] + delta) % span + span) % span;
  value[DAY] = min<int16_t>(value[DAY], daysIn(value[YEAR], value[MONTH]));  // 31 Jan -> Feb gives 28/29
}

static void save() {
  struct tm t = {};
  t.tm_year = value[YEAR] - 1900;
  t.tm_mon = value[MONTH] - 1;
  t.tm_mday = value[DAY];
  t.tm_hour = value[HOUR];
  t.tm_min = value[MINUTE];
  t.tm_isdst = -1;     // let the London rules decide GMT or BST
  clockSet(mktime(&t));  // local -> UTC; sets system time and the clock chip
}

static void onEnter() {
  screen = LIST;
  cursor = 0;
}

static Redraw onButton(Event e) {
  switch (screen) {
    case LIST:
      if (e == Event::AShort) {
        cursor = (cursor + 1) % ROWS;
      } else if (e == Event::BShort) {
        if (cursor == DATE) {
          startEditing();
          screen = DATETIME;
        } else if (cursor == SLEEP) {
          int now = powerSleepSeconds(), next = SLEEP_CHOICES[0];
          for (int i = 0; i < 3; i++)
            if (SLEEP_CHOICES[i] == now) next = SLEEP_CHOICES[i + 1];
          powerSetSleepSeconds(next);
        } else if (cursor == INVERT) {
          displaySetInverted(!display.inverted);
          return Redraw::Full;  // every pixel changes
        } else {
          screen = cursor == INFO_ROW ? INFO : RESET;
          resetCursor = 0;
        }
      } else {
        return Redraw::None;
      }
      return Redraw::Partial;
    case DATETIME:
      if (e == Event::AShort) field = (field + 1) % FIELDS;
      else if (e == Event::BShort && field == SAVE) {
        save();
        screen = LIST;
        launcherToast("Clock set");
      } else if (e == Event::BShort) step(+1);
      else if (e == Event::BLong && field != SAVE) step(-1);
      else return Redraw::None;
      return Redraw::Partial;
    case INFO:
      return e == Event::BShort ? Redraw::Partial : Redraw::None;  // refresh the readings
    case RESET:
      if (e == Event::AShort) resetCursor = (resetCursor + 1) % RESETS;
      else if (e == Event::BShort) screen = CONFIRM;
      else return Redraw::None;
      return Redraw::Partial;
    case CONFIRM:
      if (e == Event::BShort) clear(resetCursor);
      else if (e != Event::AShort) return Redraw::None;
      screen = RESET;
      return Redraw::Partial;
  }
  return Redraw::None;
}

static Redraw onBack() {
  switch (screen) {
    case LIST: return Redraw::Exit;
    case DATETIME: launcherToast("Not saved"); screen = LIST; break;
    case CONFIRM: screen = RESET; break;
    default: screen = LIST; break;
  }
  return Redraw::Partial;
}

static void draw() {
  switch (screen) {
    case DATETIME: drawDateTime(); break;
    case INFO: drawInfo(); break;
    case RESET: drawReset(); break;
    case CONFIRM: drawConfirm(); break;
    default: drawList(); break;
  }
}

static void onExit() {}

static void status(char *out, size_t len) {
  if (!clockValid()) snprintf(out, len, "Clock not set");
  else snprintf(out, len, "Sleep after %d s", powerSleepSeconds());
}

extern const App settingsApp = {"Settings", ICON_SETTINGS, onEnter, onButton, draw, onExit, onBack, status};
