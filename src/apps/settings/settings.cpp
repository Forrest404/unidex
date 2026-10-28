// Settings: date and time, sync (the clock and the calendar link over WiFi), sleep, volume, alert sound, invert,
// battery and info, reset data (one thing, or the whole device). Six rows a page.
// A = next row or field, B = change, open or clear, hold A = back (in the date editor: leave without saving).
// Volume: B = louder, hold B = quieter, each with a chime at the new level.
// Hold B in the list: set up from a phone (the phone portal, src/apps/notes/phone.cpp), then check what changed.
#include "../../core/app.h"
#include "../../core/audio.h"
#include "../../core/ringtones.h"
#include "../../core/battery.h"
#include "../../core/clock.h"
#include "../../core/devtools.h"
#include "../../core/display.h"
#include "../../core/launcher.h"
#include "../../core/power.h"
#include "../../core/storage.h"
#include "../../core/theme.h"
#include "../notes/cloud.h"
#include "../notes/phone.h"
#include "../pet/meet.h"
#include "../timetable/callink.h"
#include "../notes/languages.h"
#include "../../core/credentials.h"
#include <LittleFS.h>
#include <esp_mac.h>
#include "../../core/usbsync.h"
#include <unidex_version.h>

static const char *VERSION = UNIDEX_VERSION;  // from git, tools/version.py
static const int SLEEP_CHOICES[] = {10, 20, 30, 60};
static const int ROW_H = 22, PAGE_ROWS = 6;  // six rows fit above the hints

enum Screen : uint8_t { LIST, DATETIME, INFO, RESET, CONFIRM, CONFIRM_AGAIN, VOLUME, PHONE, CHECK, SYNC };
enum Row : uint8_t { DATE, SYNC_ROW, SLEEP, VOLUME_ROW, ALERTS, RINGTONE, LANGUAGE, INVERT, INFO_ROW, RESET_ROW, ROWS };
enum Reset : uint8_t { CALENDAR, PET_FRIENDS, EVERYTHING, FACTORY, RESETS };
static const char *RESET_NAMES[] = {"Calendar", "Pet friends", "Everything", "Factory reset"};
static const int RESET_ROW_H = 22;  // six rows fit above the hints
enum Field : uint8_t { YEAR, MONTH, DAY, HOUR, MINUTE, SAVE, FIELDS };

RTC_DATA_ATTR static uint8_t screen, cursor, resetCursor, field;
RTC_DATA_ATTR static int16_t value[MINUTE + 1];  // the date being edited: y, m, d, h, min

static int daysIn(int year, int month) {
  static const int DAYS[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
  return DAYS[month - 1] + (month == 2 && leap);
}

// One settings row: label left, value right; the selected row is inverted.
static void drawRow(int i, bool selected, const char *label, const char *val, int rowH = ROW_H) {
  const int16_t top = CONTENT_TOP + 4 + i * rowH, baseline = top + rowH / 2 + 4;
  if (selected) display.fillRect(MARGIN - 4, top, display.width() - 2 * (MARGIN - 4), rowH - 2, BLACK);
  display.setTextColor(selected ? WHITE : BLACK);
  display.setCursor(MARGIN, baseline);
  display.print(label);
  drawRight(val, baseline);
  display.setTextColor(BLACK);
}

// The volume screen's slider: AUDIO_VOLUME_MAX blocks, centred, filled up to the volume.
static void drawSlider(int16_t cy) {
  const int16_t w = 14, h = 24, gap = 3, x0 = (display.width() - AUDIO_VOLUME_MAX * (w + gap) + gap) / 2;
  for (int i = 0; i < AUDIO_VOLUME_MAX; i++) {
    const int16_t x = x0 + i * (w + gap);
    if (i < audioVolume()) display.fillRect(x, cy - h / 2, w, h, BLACK);
    else display.drawRect(x, cy - h / 2, w, h, BLACK);
  }
}

// When the clock and calendar last came (the Mac, the Tools page or Sync): "10:42" today, else "27 Oct"; "" if not
// since power-up.
static void lastSync(char *out, size_t len) {
  *out = 0;
  if (time_t t = usbSyncLastTime()) {
    struct tm s, now = clockLocal();
    localtime_r(&t, &s);
    strftime(out, len, s.tm_yday == now.tm_yday && s.tm_year == now.tm_year ? "%H:%M" : "%d %b", &s);
  }
}

static void drawList() {
  const int page = cursor / PAGE_ROWS, first = page * PAGE_ROWS;
  char pages[8];
  snprintf(pages, sizeof pages, "%d/%d", page + 1, (ROWS + PAGE_ROWS - 1) / PAGE_ROWS);
  drawHeader("Settings", pages);
  display.setFont(FONT_SMALL);
  char time[8] = "not set", sleep[8], synced[12];
  if (clockValid()) {
    struct tm now = clockLocal();
    snprintf(time, sizeof time, "%02d:%02d", now.tm_hour, now.tm_min);
  }
  snprintf(sleep, sizeof sleep, "%d s", powerSleepSeconds());
  lastSync(synced, sizeof synced);
  for (int row = first; row < first + PAGE_ROWS && row < ROWS; row++) {
    const int i = row - first;
    const bool sel = cursor == row;
    switch (row) {
      case DATE: drawRow(i, sel, "Date & time", time); break;
      case SYNC_ROW: drawRow(i, sel, "Sync", synced); break;
      case SLEEP: drawRow(i, sel, "Sleep after", sleep); break;
      case VOLUME_ROW: {
        drawRow(i, sel, "Volume", "");
        // a small bar, filled as far as the volume
        const int16_t w = 50, x = display.width() - MARGIN - w, y = CONTENT_TOP + 4 + i * ROW_H + 5;
        const uint16_t ink = sel ? WHITE : BLACK;
        display.drawRect(x, y, w, 10, ink);
        display.fillRect(x + 2, y + 2, (w - 4) * audioVolume() / AUDIO_VOLUME_MAX, 6, ink);
        break;
      }
      case ALERTS: drawRow(i, sel, "Alert sound", audioAlertsMuted() ? "off" : "on"); break;
      case RINGTONE: drawRow(i, sel, "Ringtone", audioRingtoneName(audioRingtone())); break;
      case LANGUAGE: {  // the language notes are in ("Auto" or its name)
        const int lang = max(0, languages::find(credGet("note_lang").c_str()));
        drawRow(i, sel, "Note language", languages::LIST[lang].name);
        break;
      }
      case INVERT: drawRow(i, sel, "Invert", display.inverted ? "on" : "off"); break;
      case INFO_ROW: drawRow(i, sel, "Battery & info", ">"); break;
      case RESET_ROW: drawRow(i, sel, "Reset data", ">"); break;
    }
  }
  drawHints("next", cursor == SLEEP || cursor == INVERT || cursor == ALERTS || cursor == RINGTONE || cursor == LANGUAGE ? "change" : "open", "phone");
}

// Sync: the clock from the internet, then this week's events from the calendar link (src/apps/timetable/callink.h).
static SyncResult synced;
static bool hasSynced, syncing;

static void drawSync() {
  drawHeader("Sync");
  display.setFont(FONT_SMALL);
  if (syncing) {
    drawCentered("Syncing...", 88);
    drawHints("", "", "");
    return;
  }
  if (!hasSynced) {
    const bool link = credHas("cal_url");
    drawCentered("Sets the clock from", 56);
    drawCentered(link ? "the internet, and gets" : "the internet.", 76);
    drawCentered(link ? "this week's events" : "Add a calendar link", link ? 96 : 108);
    drawCentered(link ? "from your calendar." : "on the Setup page too.", link ? 116 : 128);
    drawHints("", "sync", "");
    return;
  }
  // What each part did, and below, why one didn't ("Link not found"), as the service said it.
  char clock[12] = "set", events[16];
  if (!synced.clock && clockValid()) {
    struct tm now = clockLocal();
    snprintf(clock, sizeof clock, "set %02d:%02d", now.tm_hour, now.tm_min);
  }
  drawRow(0, false, "Clock", synced.clock ? "not synced" : clock);
  if (!synced.link) snprintf(events, sizeof events, "no link");
  else if (synced.clock || synced.calendar) snprintf(events, sizeof events, "not synced");
  else snprintf(events, sizeof events, synced.events == 1 ? "1 event" : "%d events", synced.events);
  drawRow(1, false, "Calendar", events);
  String why = synced.clock ? synced.clock : synced.calendar ? synced.calendar : "";
  if (why.startsWith("Calendar: ")) why = why.substring(10);
  if (why.length()) {
    char line[48];
    snprintf(line, sizeof line, "%s", why.c_str());
    line[0] = toupper(line[0]);
    drawCentered(fitText(line, display.width() - 2 * MARGIN).c_str(), 108);
  } else if (!synced.link) {
    drawCentered("Add a calendar link", 100);
    drawCentered("on the Setup page.", 120);
  }
  drawHints("", "again", "");
}

// After phone setup changed WiFi or keys: each service with something saved, checked over the internet one per
// tick, so the screen shows how far it's got. Not kept over a sleep (it's back to the list then).
static const int CHECKS_MAX = 6;
static const char *checkWhat[CHECKS_MAX];
static String checkResult[CHECKS_MAX];
static int checks, checked;
static bool checkShown;  // the row being checked shows "..." before the (blocking) check starts
static bool checksThenExit;  // opened for the checks alone (the first start): when they're read, leave

static const char *checkLabel(const char *what) {
  return !strcmp(what, "wifi") ? "WiFi" : !strcmp(what, "openai") ? "OpenAI" : !strcmp(what, "elevenlabs") ? "ElevenLabs"
         : !strcmp(what, "anthropic") ? "Claude" : !strcmp(what, "github") ? "GitHub" : "Calendar";
}

static void planChecks() {
  checks = checked = 0;
  checkShown = checksThenExit = false;
  auto add = [](const char *what) { checkWhat[checks] = what, checkResult[checks++] = ""; };
  add("wifi");
  const bool eleven = credGet("transcribe") == "elevenlabs";
  if (credHas(eleven ? "elevenlabs_key" : "openai_key")) add(eleven ? "elevenlabs" : "openai");
  const String tidy = credGet("cleanup");
  if (tidy == "anthropic" && credHas("anthropic_key")) add("anthropic");
  else if (tidy == "openai" && eleven && credHas("openai_key")) add("openai");
  if (credGet("gh_on") == "1") add("github");
  if (credHas("cal_url")) add("calendar");
}

// The next check. A failure reads without the service's name, which is on the row already ("OpenAI: bad key").
static void runCheck() {
  const char *what = checkWhat[checked], *label = checkLabel(what);
  String detail, result;
  if (checked > 0 && checkResult[0] != "ok") {
    result = "needs WiFi";
  } else if (const char *err = cloudTest(what, detail)) {
    result = err;
    if (result.startsWith(label)) result = result.substring(strlen(label));
    if (result.startsWith(":")) result = result.substring(1);
    result.trim();
  } else {
    result = "ok";
  }
  checkResult[checked++] = result;
  checkShown = false;
  powerActivity();  // still in use: no sleep between checks
}

static void drawCheck() {
  drawHeader(checked < checks ? "Checking" : "Checked");
  display.setFont(FONT_SMALL);
  for (int i = 0; i < checks; i++) {
    String v = i < checked ? checkResult[i] : i == checked ? "..." : "";
    const int16_t room = display.width() - 2 * MARGIN - textWidth(checkLabel(checkWhat[i])) - 10;
    while (v.length() > 3 && textWidth(v.c_str()) > room) v = v.substring(0, v.length() - 4) + "..";
    drawRow(i, false, checkLabel(checkWhat[i]), v.c_str());
  }
  drawHints("", checked < checks ? "" : "done", "");
}

static void drawVolume() {
  drawHeader("Volume");
  char n[8];
  snprintf(n, sizeof n, "%d", audioVolume());
  display.setFont(FONT_LARGE);
  drawCentered(audioVolume() ? n : "Off", 64);
  drawSlider(108);
  display.setFont(FONT_SMALL);
  drawCentered("Notes and reminders", 144);
  drawHints("", audioVolume() < AUDIO_VOLUME_MAX ? "louder" : "", audioVolume() ? "quieter" : "");
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

// "unidex-1A2B", so a device can be told apart (when asking for help): the same name as its own WiFi in Notes'
// Open on phone (src/apps/notes/phone.cpp). The Pet's radio uses a new random address every time instead.
static const char *deviceId() {
  static char id[16];
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
  snprintf(id, sizeof id, "unidex-%02X%02X", mac[4], mac[5]);
  return id;
}

static void drawInfo() {
  drawHeader("Battery & info");
  display.setFont(FONT_SMALL);
  char battery[24], storage[24], sync[12];
  const int mv = batteryMillivolts(), pct = batteryPercent();
  if (pct < 0) snprintf(battery, sizeof battery, "none");
  else snprintf(battery, sizeof battery, "%d.%02d V %d%%", mv / 1000, mv % 1000 / 10, pct);
  uint64_t used, total;
  if (storageUsage(used, total))
    snprintf(storage, sizeof storage, "%llu / %llu MB", used >> 20, total >> 20);
  else
    snprintf(storage, sizeof storage, "no card");
  lastSync(sync, sizeof sync);
  if (!*sync) snprintf(sync, sizeof sync, "not yet");
  drawRow(0, false, pct >= 0 && batteryCharging() ? "On USB" : "Battery", battery);
  // A build between releases, "v1.4-3-g13bf907": shown as "v1.4-3", which fits. A test build's "-dirty" (uncommitted
  // changes) is left out here to fit too; USB's V command reports it.
  String version = VERSION;
  if (version.indexOf("-g") > 0) version.remove(version.indexOf("-g"), 9);
  version.replace("-dirty", "");
  drawRow(1, false, "Firmware", version.c_str());
  drawRow(2, false, "Storage", storage);
  drawRow(3, false, "Calendar sync", sync);
  drawRow(4, false, "Device", deviceId());
  drawHints("", "refresh", "");
}

static void drawResetRows() {
  drawHeader("Reset data");
  display.setFont(FONT_SMALL);
  for (int i = 0; i < RESETS; i++) drawRow(i, i == resetCursor, RESET_NAMES[i], "", RESET_ROW_H);
}

static void drawReset() {
  drawResetRows();
  drawHints("next", "clear", "");
}

static void drawConfirm() {
  static const char *TITLES[] = {"Clear the calendar?", "Forget friends?", "Reset everything?", "Factory reset?"};
  // Each line has to fit the box: about 164 px in the small font.
  static const char *WHAT[] = {"Synced events go.", "Your Pet forgets all.", "Settings and the", "Wipes WiFi, keys,"};
  static const char *MORE[] = {"Can't be undone.", "Can't be undone.", "calendar go.", "the Pet, all settings."};
  static const char *LAST[] = {"", "", "Pet and keys stay.", "Card files stay."};
  drawResetRows();
  if (screen == CONFIRM_AGAIN) {  // the factory reset asks twice
    drawSheet("Wipe the device?", "Back to as new.", "Can't be undone.");
    drawHints("keep", "wipe", "");
    return;
  }
  drawSheet(TITLES[resetCursor], WHAT[resetCursor], MORE[resetCursor], LAST[resetCursor]);
  drawHints("keep", resetCursor == FACTORY ? "next" : resetCursor == EVERYTHING ? "reset" : "clear", "");
}

// The whole device back to as new, for passing it on: every setting, the WiFi and keys (also the copy the WiFi
// keeps for itself), the Pet, its friends (and their copy on the card), the calendar, and anything left in
// the internal flash from older versions. Notes, badges and the timetable on the card stay: the card can be taken
// out and wiped on a computer.
static void factoryReset() {
  if (devDryRun()) {
    launcherToast("Dry run: not wiped");
    return;
  }
  for (const char *path : {"/pet/friends.bin", "/pet/friends.bin.old", "/events.csv"}) storageRemove(path);
  LittleFS.begin(false);  // format() needs the partition begin() names (without it, it stops the device)
  LittleFS.format();
  LittleFS.end();
  storageEraseAll();
  systemRestart("Wiped: as new");
}

// Clears one thing and says what happened (the honest result: nothing there, no card, done).
static void clear(uint8_t what) {
  if (devDryRun()) {
    launcherToast("Dry run: not cleared");
    return;
  }
  if (what == CALENDAR) {
    if (!storageCardMount()) launcherToast("No SD card");
    else if (!storageExists("/events.csv")) launcherToast("Nothing to clear");
    else launcherToast(storageRemove("/events.csv") ? "Calendar cleared" : "Couldn't clear it");
    storageRemoveKey("events_crc");  // so the next Mac sync writes the events again
    return;
  }
  if (what == PET_FRIENDS) {
    const bool any = meetFriendCount() > 0;
    meetForgetFriends();
    launcherToast(any ? "Friends forgotten" : "No friends to forget");
    return;
  }
  // Everything: but the Pet stays (its look, name, number and friends, and your name), like the WiFi and keys, so
  // friends' devices still know it. Read before NVS is cleared, written back after.
  const int32_t petId = storageGetInt("pet_id", 0), look = storageGetInt("pet_look", INT32_MIN);
  const String petName = storageGetString("pet_name"), ownerName = storageGetString("owner_name");
  static uint8_t friendsBlock[1024];
  const size_t friendsLen = storageGetBytes("pet_friends", friendsBlock, sizeof friendsBlock);
  storageRemove("/events.csv");
  storageClearKeys();  // settings and the badge choice (not the WiFi and keys: they're separate)
  if (petId) storagePutInt("pet_id", petId);
  if (look != INT32_MIN) storagePutInt("pet_look", look);
  if (petName.length()) storagePutString("pet_name", petName.c_str());
  if (ownerName.length()) storagePutString("owner_name", ownerName.c_str());
  if (friendsLen) storagePutBytes("pet_friends", friendsBlock, friendsLen);
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
        } else if (cursor == SYNC_ROW) {
          screen = SYNC;
          hasSynced = false;
        } else if (cursor == VOLUME_ROW) {
          screen = VOLUME;
        } else if (cursor == ALERTS) {
          audioSetAlertsMuted(!audioAlertsMuted());
          if (!audioAlertsMuted()) audioChime();  // what an alert sounds like now
        } else if (cursor == RINGTONE) {  // the next tune, and how it sounds (even with Alert sound off: you asked)
          audioSetRingtone((audioRingtone() + 1) % ringtones::COUNT);
          audioChime();
        } else if (cursor == LANGUAGE) {  // the next one in the list, round to Auto
          const int lang = max(0, languages::find(credGet("note_lang").c_str()));
          credSet("note_lang", languages::LIST[(lang + 1) % languages::COUNT].code);
        } else if (cursor == INVERT) {
          displaySetInverted(!display.inverted);
          return Redraw::Full;  // every pixel changes
        } else {
          screen = cursor == INFO_ROW ? INFO : RESET;
          resetCursor = 0;
        }
      } else if (e == Event::BLong) {  // set up from a phone: Setup first if there's no WiFi yet
        if (const char *low = batteryTooLow()) launcherToast(low);  // the hotspot draws as much as normal WiFi
        else if (const char *err = phoneStart(credWifiAny() ? PhoneTab::Settings : PhoneTab::Setup)) launcherToast(err);
        else screen = PHONE;
      } else {
        return Redraw::None;
      }
      return Redraw::Partial;
    case PHONE:
      phoneActivity();  // any press keeps the hotspot up
      return Redraw::None;
    case CHECK:
      if (checked < checks) return Redraw::None;
      screen = LIST;
      if (checksThenExit) return checksThenExit = false, Redraw::Exit;
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
    case SYNC:
      if (e != Event::BShort) return Redraw::None;
      syncing = true;
      displayShow(drawSync, false);  // the sync holds everything up for a few seconds (more for a big calendar)
      synced = calendarSync();
      syncing = false;
      hasSynced = true;
      powerActivity();  // it took a while: don't sleep straight away
      return Redraw::Partial;
    case VOLUME: {
      const int v = audioVolume() + (e == Event::BShort ? 1 : e == Event::BLong ? -1 : 0);
      if (v == audioVolume() || v < 0 || v > AUDIO_VOLUME_MAX) return Redraw::None;
      audioSetVolume(v);
      audioChime();  // how loud it is now (silent at 0)
      return Redraw::Partial;
    }
    case RESET:
      if (e == Event::AShort) resetCursor = (resetCursor + 1) % RESETS;
      else if (e == Event::BShort) screen = CONFIRM;
      else return Redraw::None;
      return Redraw::Partial;
    case CONFIRM:
      if (e == Event::BShort && resetCursor == FACTORY) {
        screen = CONFIRM_AGAIN;
        return Redraw::Partial;
      }
      if (e == Event::BShort) clear(resetCursor);
      else if (e != Event::AShort) return Redraw::None;
      screen = RESET;
      return Redraw::Partial;
    case CONFIRM_AGAIN:
      if (e == Event::BShort) factoryReset();
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
    case CONFIRM:
    case CONFIRM_AGAIN: screen = RESET; break;
    case PHONE:
      phoneStop();
      if (phoneChanged()) planChecks(), screen = CHECK;
      else screen = LIST;
      break;
    case CHECK:
      if (checked < checks) return Redraw::None;
      screen = LIST;
      if (checksThenExit) return checksThenExit = false, Redraw::Exit;
      break;
    default: screen = LIST; break;
  }
  return Redraw::Partial;
}

static void draw() {
  if ((screen == PHONE && !phoneOn()) || (screen == CHECK && !checks)) screen = LIST;  // e.g. after a sleep
  switch (screen) {
    case PHONE: phoneDraw("Set up on phone"); break;
    case CHECK: drawCheck(); break;
    case DATETIME: drawDateTime(); break;
    case INFO: drawInfo(); break;
    case SYNC: drawSync(); break;
    case VOLUME: drawVolume(); break;
    case RESET: drawReset(); break;
    case CONFIRM:
    case CONFIRM_AGAIN: drawConfirm(); break;
    default: drawList(); break;
  }
}

static Redraw tick() {
  if (screen == PHONE) {
    const Redraw r = phoneTick();
    if (phoneOn()) return r;
    // Done on the page, or nobody used it for a while: check what changed.
    if (phoneChanged()) planChecks(), screen = CHECK;
    else screen = LIST;
    return Redraw::Partial;
  }
  if (screen != CHECK || checked >= checks) return Redraw::None;
  if (!checkShown) {
    checkShown = true;  // show "..." on the row first: the check holds everything up for a few seconds
    return Redraw::Partial;
  }
  runCheck();
  return Redraw::Partial;
}

static void onExit() { phoneStop(); }

void settingsShowChecks() {
  planChecks();
  screen = CHECK;
  checksThenExit = true;
}

static void status(char *out, size_t len) {
  if (!clockValid()) snprintf(out, len, "Clock not set");
  else snprintf(out, len, "Sleep after %d s", powerSleepSeconds());
}

extern const App settingsApp = {"Settings", ICON_SETTINGS, onEnter, onButton, draw, onExit, onBack, status, tick};
