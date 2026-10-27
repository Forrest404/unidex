// Reminders: a to-do list, kept in /reminders.csv on the SD card. List: A = next, B = done (or undo), hold B = say
// a new one ("call Sam tomorrow at 5"). The last row clears the done ones (B, then B again to confirm). Hold A = back.
// Each recording goes on the card first (waiting.h); with WiFi it's turned into words by Notes' transcriber (Whisper
// or Scribe, through its background job: no new key to set up) and the recording goes. Without, it waits (a row at
// the top: B sends now) and is tried again every 10 min. Which part says when is worked out on the device, by the
// rules in reminders_logic.h, from when it was said. When one falls due it chimes
// and shows over whatever is on screen, waking the device for it: B = done, A = in 10 minutes, hold A = leave it.
#include <vector>
#include "reminders.h"
#include "reminders_logic.h"
#include "waiting.h"
#include "../notes/cloud.h"
#include "../notes/job.h"
#include "../notes/record.h"
#include "../../core/app.h"
#include "../../core/audio.h"
#include "../../core/battery.h"
#include "../../core/clock.h"
#include "../../core/credentials.h"
#include "../../core/devtools.h"
#include "../../core/display.h"
#include "../../core/launcher.h"
#include "../../core/net.h"
#include "../../core/power.h"
#include "../../core/storage.h"
#include "../../core/theme.h"

using reminders::Reminder;

static const char *const FILE_PATH = "/reminders.csv", *const TMP_PATH = "/reminders.tmp";
static const int ROWS = 5, ROW_H = 26, BOX = 11;
static const uint32_t MAX_SECONDS = 30;  // a reminder is a sentence
static const size_t MAX_TITLE = 120;
static const int SNOOZE_MIN = 10;
static const time_t RETRY_S = 600;  // recordings waiting for WiFi: tried again every 10 min

enum Screen : uint8_t { LIST, CLEAR, ALERT };
RTC_DATA_ATTR static uint8_t screen;
// The one falling due, on the ALERT screen. alertedUpTo: the latest due time already shown (a reminder due at or
// before it doesn't come up again); alertMin: the next one to show, both in local minutes from 1970 (-1: none).
RTC_DATA_ATTR static char alertDue[17], alertTitle[MAX_TITLE + 1];
RTC_DATA_ATTR static bool alertOverList;  // it came up with the list open: back to the list after
RTC_DATA_ATTR static int32_t alertedUpTo = -1, alertMin = -1;
RTC_DATA_ATTR static uint16_t cursor;
// For the home screen (it can't read the card): kept up to date whenever the list changes.
RTC_DATA_ATTR static int16_t openCount = -1, lateCount;
RTC_DATA_ATTR static int32_t nextMin = -1;  // the next one due, in local minutes from 1970 (-1: none)
RTC_DATA_ATTR static bool nextHasTime;
RTC_DATA_ATTR static char nextTitle[32];
RTC_DATA_ATTR static char lateTitle[32];  // the late one that came due last
RTC_DATA_ATTR static int16_t waitingCount;  // recordings on the card still to be sent
RTC_DATA_ATTR static time_t lastTry;        // when they were last tried (0: not yet)

// RAM cache: lost in deep sleep, read again on first use after a wake.
static std::vector<Reminder> list;
static bool listed;
static int8_t drawnMinute = -1;
static bool listening;    // a recording is with the job, being turned into words
static String failed;     // why the last one wasn't added: shown over the list until a press
RTC_DATA_ATTR static bool isOpen;  // the open app stays open through deep sleep (onEnter isn't called again)
static uint32_t seenGen;  // the job status last drawn

#if UNIDEX_DEV
static bool listedDemo;
#endif

// Now as local minutes from 1970, the unit reminders_logic.h counts in. -1 if the clock isn't set.
static long nowMinutes() {
  if (!clockValid()) return -1;
  const struct tm t = clockLocal();
  return (long)reminders::daysFromCivil(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday) * 1440 + t.tm_hour * 60 + t.tm_min;
}

static int doneCount() {
  int n = 0;
  for (const Reminder &r : list) n += r.done;
  return n;
}

// The rows: "2 waiting for WiFi" first if any are, the reminders, then "Clear 2 done" if any are done.
static int firstItem() { return waitingCount > 0 ? 1 : 0; }
static int rowCount() { return firstItem() + list.size() + (doneCount() ? 1 : 0); }
static bool onWaitingRow() { return waitingCount > 0 && cursor == 0; }
static bool onClearRow() { return cursor == firstItem() + list.size(); }

static void remember() {
  const long now = nowMinutes();
  openCount = lateCount = 0;
  long lateMin = -1;
  lateTitle[0] = 0;
  for (const Reminder &r : list) {
    openCount += !r.done;
    if (!reminders::isLate(r, now)) continue;
    lateCount++;
    if (reminders::dueMinutes(r.due) > lateMin) {
      lateMin = reminders::dueMinutes(r.due);
      snprintf(lateTitle, sizeof lateTitle, "%s", r.title.c_str());
    }
  }
  const int next = reminders::nextDue(list, now);
  nextMin = next < 0 ? -1 : reminders::dueMinutes(list[next].due);
  const int alert = alertedUpTo < 0 ? -1 : reminders::nextDue(list, alertedUpTo);
  alertMin = alert < 0 ? -1 : reminders::dueMinutes(list[alert].due);
  nextHasTime = next >= 0 && list[next].hasTime();
  snprintf(nextTitle, sizeof nextTitle, "%s", next < 0 ? "" : list[next].title.c_str());
}

#if UNIDEX_DEV
// Test build, X DEMO 1: sample reminders around today, for screenshots without anyone's list.
static void demoList() {
  const long now = nowMinutes() < 0 ? (long)reminders::daysFromCivil(2026, 10, 19) * 1440 + 630 : nowMinutes();
  const int today = now / 1440;
  auto at = [](int day, int minutes) { return reminders::formatDue(day, minutes); };
  list = {{at(today, 8 * 60), false, "Hand in the art project"}, {at(today, 17 * 60), false, "Call Sam"},
          {at(today + 1, 7 * 60 + 30), false, "Swim kit"},       {at(today + 3, -1), false, "Library books back"},
          {"", false, "Buy a birthday card for Jo"},             {"", true, "Charge the bike lights"}};
  reminders::sortList(list);
}
#endif

static void ensureList() {
#if UNIDEX_DEV
  if (listed && listedDemo != devDemo()) {
    listed = false;
    alertedUpTo = nowMinutes();  // a fresh start for the other list: the samples already past don't come up
  }
  listedDemo = devDemo();
  if (!listed && devDemo()) {
    demoList();
    listed = true;
    remember();
    return;
  }
#endif
  if (listed) return;
  list.clear();
  fs::File f = storageExists(FILE_PATH) ? storageOpen(FILE_PATH, "r") : fs::File();  // none yet: nothing to say
  String line;
  Reminder r;
  while (f && storageReadLine(f, line) && (int)list.size() < reminders::MAX_REMINDERS)
    if (reminders::fromLine(line.c_str(), r)) list.push_back(r);
  if (f) f.close();
  reminders::sortList(list);
  waitingCount = waitingRecordings().size();
  listed = true;
  if (cursor >= rowCount()) cursor = 0;
  remember();
}

// The whole list, written to a new file and then swapped in, so a power cut can't leave half of it.
static bool save() {
  reminders::sortList(list);
  remember();
#if UNIDEX_DEV
  if (devDemo()) return true;  // the samples are never written to the card
#endif
  String text;
  for (const Reminder &r : list) text += (reminders::toLine(r) + "\n").c_str();
  fs::File f = storageOpen(TMP_PATH, "w");
  if (!f) return false;
  const size_t wrote = f.print(text);
  f.close();
  return wrote == text.length() && storageReplace(TMP_PATH, FILE_PATH, text.length());
}

static void clearDone() {
  if (devDryRun()) {
    launcherToast("Dry run: not cleared");
    return;
  }
  const int n = doneCount();
  list.erase(std::remove_if(list.begin(), list.end(), [](const Reminder &r) { return r.done; }), list.end());
  if (!save()) {
    listed = false;  // read back what the card really holds
    launcherToast("Couldn't save");
    return;
  }
  cursor = 0;
  char msg[24];
  snprintf(msg, sizeof msg, n == 1 ? "1 cleared" : "%d cleared", n);
  launcherToast(msg);
}

static void toggleDone() {
  Reminder &r = list[cursor - firstItem()];
  r.done = !r.done;
  if (!save()) {
    listed = false;
    launcherToast("Couldn't save");
  }
  // The cursor stays on the same row, so ticking one after another goes down the list.
  if (cursor >= rowCount()) cursor = 0;
}

// --- adding one by voice ---

// A brown-out while the radio is on could damage the card.
static void record() {
  if (const char *low = batteryTooLow()) {
    launcherToast(low);
    return;
  }
  if ((int)list.size() + waitingCount >= reminders::MAX_REMINDERS) {
    launcherToast("Full: clear done ones");
    return;
  }
  if (!jobCanTake()) {  // the last recording isn't on the card yet
    launcherToast("Still saving one");
    return;
  }
  int16_t *samples;
  size_t count;
  if (const char *err = recordWhileHeld(MAX_SECONDS, samples, count)) {
    launcherToast(err);
    return;
  }
  if (!jobStartReminders(waitingNewId(), samples, count)) {
    jobGiveBuffer(samples);
    launcherToast("Busy: try again");
    return;
  }
  listening = true;
  seenGen = jobStatus().gen;
  lastTry = clockValid() ? time(nullptr) : 0;  // a try of its own: the waiting ones go with it
}

// The ones waiting for WiFi, sent now (B on their row) or every RETRY_S by themselves. Quiet: no toast if it can't.
static bool sendWaiting(bool quiet) {
  const char *why = !credWifiAny() ? "Setup: add your WiFi"
                    : cloudTranscribeMissing() && !devFakeCloud() ? "Setup: add a key"  // OpenAI's or ElevenLabs'
                    : jobBusy() || netClaimed() ? "Busy: try again"
                    : batteryTooLow();
  if (!why && !jobStartReminders("", nullptr, 0)) why = "Busy: try again";
  if (why) {
    if (!quiet) launcherToast(why);
    return false;
  }
  listening = true;
  seenGen = jobStatus().gen;
  lastTry = clockValid() ? time(nullptr) : 0;
  return true;
}

static bool retryDue() {
  static uint32_t triedMs;  // without the clock: once per start, then every RETRY_S awake
  if (!clockValid()) {
    if (triedMs && millis() - triedMs < RETRY_S * 1000) return false;
    triedMs = millis() | 1;
    return true;
  }
  const time_t now = time(nullptr);
  return !lastTry || now < lastTry || now - lastTry >= RETRY_S;
}

// The words the job left on the card (waiting.h): each becomes a reminder, timed from when it was said, selected in
// the list. Words that can't be added yet (the list is full, or it won't save) stay on the card for next time.
static void addWords() {
  const std::vector<String> ids = waitingWords();
  if (ids.empty()) return;
  ensureList();
  std::vector<String> taken;
  int heardNothing = 0;
  Reminder last;
  for (const String &id : ids) {
    String said = asciiText(waitingReadWords(id));
    said.trim();
    if (!said.length()) {
      heardNothing++;
      taken.push_back(id);
      continue;
    }
    if ((int)list.size() >= reminders::MAX_REMINDERS) break;
    const long saidAt = waitingSaidAt(id) >= 0 ? waitingSaidAt(id) : nowMinutes();
    const reminders::Heard heard = reminders::parseReminder(said.c_str(), saidAt);
    last = {heard.due, false, heard.title.substr(0, MAX_TITLE)};
    list.insert(list.begin(), last);  // the front: the newest of the undated ones
    taken.push_back(id);
  }
  const int added = taken.size() - heardNothing;
  if (added && !save()) {
    listed = false;
    launcherToast("Couldn't save");
    return;
  }
  for (const String &id : taken) waitingRemoveWords(id);
  if (!added) {
    failed = "Heard nothing";
    if (!isOpen) launcherToast("Reminder not added");  // the reason waits on the list
    return;
  }
  for (size_t i = 0; i < list.size(); i++)
    if (!list[i].done && list[i].title == last.title && list[i].due == last.due) {
      cursor = firstItem() + i;
      break;
    }
  screen = LIST;
  const long now = nowMinutes();
  const std::string tag = reminders::dueTag(last, now);
  // The row is selected, with its title: the toast says when.
  char many[24];
  snprintf(many, sizeof many, "%d added", added);
  const std::string msg = added > 1 ? many : now < 0 ? "Added, no time: clock not set" : tag.empty() ? "Added" : "Added for " + tag;
  launcherToast(msg.c_str());
}

// A run of the job has finished: what it turned into words goes in the list; what it couldn't send waits.
static void sent(const String &problem, bool offline) {
  waitingCount = waitingRecordings().size();
  if (cursor >= rowCount()) cursor = 0;
  addWords();
  if (problem.length()) {  // the service said no, or the card: the recordings wait, and say why
    failed = problem;
    if (!isOpen) launcherToast("Reminder not added yet");
  } else if (offline && waitingCount > 0) {
    launcherToast(credWifiAny() ? "Saved for WiFi" : "Saved: set up WiFi");
  }
}

// --- falling due ---

extern const App remindersApp;

static void showAlert(const Reminder &r, bool chime) {
  snprintf(alertDue, sizeof alertDue, "%s", r.due.c_str());
  snprintf(alertTitle, sizeof alertTitle, "%s", r.title.c_str());
  if (screen != ALERT) alertOverList = isOpen;
  screen = ALERT;
  alertedUpTo = max(alertedUpTo, (int32_t)reminders::dueMinutes(r.due));
  remember();
  if (chime && !audioAlertsMuted()) audioChime();  // Settings > Mute alerts: on screen only
  powerActivity();  // on screen for the whole idle time, even after a timer wake
  launcherInterrupt(&remindersApp);
}

// Once a second, wherever you are: shows the one falling due, and sets the wake for the next.
static void checkDue() {
  const long now = nowMinutes();
  if (now < 0) {
    powerWakeAt(WakeFor::Reminders, 0);
    return;
  }
  static bool looked;  // after a restart, read the card once (the list's times are needed to wake for them)
  if (openCount < 0) {
    if (looked) return;
    looked = true;
    if (!storageCardMount()) return;
    ensureList();
  }
  // A fresh start: only what falls due from now on comes up (the late ones are in the list). The same when the clock
  // goes back (set by hand, or a fast clock put right), or what falls due before the old time would never come up.
  if (alertedUpTo < 0 || now < alertedUpTo - 1) {
    alertedUpTo = now;
    remember();
  }
  if (alertMin >= 0 && alertMin <= now && screen != ALERT) {
    ensureList();
    bool shown = false;
    for (const Reminder &r : list)
      if (!shown && !r.done && reminders::dueMinutes(r.due) == alertMin) showAlert(r, true), shown = true;
    if (!shown) alertedUpTo = alertMin, remember();  // gone from the card since: don't look for it again
  }
  if (alertMin < 0) powerWakeAt(WakeFor::Reminders, 0);
  else powerWakeAt(WakeFor::Reminders, time(nullptr) + (alertMin - now) * 60 - clockLocal().tm_sec);
}

// Done (B), later (A) or left (hold A); then the next one due the same minute, if any, or back.
static Redraw leaveAlert(Event e) {
  ensureList();
  const long now = nowMinutes();
  for (Reminder &r : list) {
    if (r.done || r.due != alertDue || r.title != alertTitle) continue;
    if (e == Event::BShort) r.done = true;
    else if (e == Event::AShort && now >= 0) r.due = reminders::formatDue((now + SNOOZE_MIN) / 1440, (now + SNOOZE_MIN) % 1440);
    if (e != Event::ALong && !save()) listed = false, launcherToast("Couldn't save");
    else if (e == Event::BShort) launcherToast("Done");
    else if (e == Event::AShort) launcherToast("Again in 10 min");
    break;
  }
  if (e != Event::ALong)
    for (const Reminder &r : list)
      if (!r.done && reminders::dueMinutes(r.due) == alertedUpTo) {
        showAlert(r, false);
        return Redraw::None;  // shown
      }
  screen = LIST;
  return alertOverList ? Redraw::Partial : Redraw::Exit;
}

static const char *stepName(JobStep s) {
  return s == JobStep::Saving ? "saving" : s == JobStep::Connecting ? "sending" : s == JobStep::Transcribing ? "listening" : "";
}

// --- drawing ---

static void drawBox(int16_t x, int16_t y, bool ticked, uint16_t ink) {
  display.drawRect(x, y, BOX, BOX, ink);
  if (!ticked) return;
  display.fillRect(x, y, BOX, BOX, ink);
  const uint16_t paper = ink == BLACK ? WHITE : BLACK;
  for (int16_t t = 0; t < 2; t++) {  // a tick, two pixels thick
    display.drawLine(x + 2, y + 5 + t, x + 4, y + 7 + t, paper);
    display.drawLine(x + 4, y + 7 + t, x + 8, y + 3 + t, paper);
  }
}

static void drawList() {
  ensureList();
  char count[8];
  snprintf(count, sizeof count, "%d", openCount);
  const char *right = listening ? stepName(jobStatus().step) : list.empty() ? "" : count;
  drawHeader("Reminders", *right ? right : nullptr);
  if (list.empty() && waitingCount <= 0) {
    drawEmpty("No reminders", "Hold B and say one,", "like: call Sam at 5");
    return;
  }
  const long now = nowMinutes();
  const int rows = rowCount(), first = cursor / ROWS * ROWS;
  for (int r = 0; r < ROWS && first + r < rows; r++) {
    const int16_t top = CONTENT_TOP + 4 + r * ROW_H, baseline = top + 17;
    const int row = first + r;
    const bool sel = row == cursor;
    const uint16_t ink = sel ? WHITE : BLACK;
    if (sel) display.fillRect(MARGIN - 4, top, display.width() - 2 * (MARGIN - 4), ROW_H - 2, BLACK);
    display.setTextColor(ink);
    display.setFont(FONT_SMALL);
    if ((waitingCount > 0 && row == 0) || row == firstItem() + (int)list.size()) {
      char text[28];
      if (row == 0 && waitingCount > 0) snprintf(text, sizeof text, "%d waiting for WiFi", waitingCount);
      else snprintf(text, sizeof text, "Clear %d done", doneCount());
      display.setCursor(MARGIN, baseline);
      display.print(text);
      display.setTextColor(BLACK);
      continue;
    }
    const Reminder &item = list[row - firstItem()];
    drawBox(MARGIN, top + 7, item.done, ink);
    const std::string tag = reminders::dueTag(item, now);
    display.setFont(FONT_TINY);
    drawRight(tag.c_str(), baseline);
    const int16_t tagW = tag.empty() ? 0 : textWidth(tag.c_str()) + 6, x = MARGIN + BOX + 6;
    display.setFont(FONT_SMALL);
    const String title = fitText(item.title.c_str(), display.width() - MARGIN - x - tagW);
    display.setCursor(x, baseline);
    display.print(title);
    if (item.done) display.drawFastHLine(x, baseline - 5, textWidth(title.c_str()), ink);  // struck through
    display.setTextColor(BLACK);
  }
}

// --- app interface ---

static void onEnter() {
  isOpen = true;
  screen = LIST;
  listed = false;  // the card may have changed since the last visit
  cursor = 0;
}

static Redraw onButton(Event e) {
  ensureList();
  if (screen == ALERT) return e == Event::AShort || e == Event::BShort ? leaveAlert(e) : Redraw::None;
  if (failed.length() && e != Event::BLong) {  // any press puts it away (hold B tries again)
    failed = "";
    return Redraw::Partial;
  }
  failed = "";
  if (screen == CLEAR) {
    if (e == Event::BShort) clearDone();
    else if (e != Event::AShort) return Redraw::None;
    screen = LIST;
    return Redraw::Partial;
  }
  if (e == Event::BLong) {
    record();
    return Redraw::Partial;  // back on the list ~0.5 s after letting go: the job does the rest
  }
  if (e == Event::AShort && rowCount() > 1) {
    cursor = (cursor + 1) % rowCount();
    return Redraw::Partial;
  }
  if (e == Event::BShort && rowCount() > 0) {
    if (onWaitingRow()) sendWaiting(false);
    else if (onClearRow()) screen = CLEAR;
    else toggleDone();
    return Redraw::Partial;
  }
  return Redraw::None;
}

static Redraw onBack() {
  if (screen == ALERT) return leaveAlert(Event::ALong);
  if (screen == LIST) return Redraw::Exit;
  screen = LIST;
  return Redraw::Partial;
}

static void drawAlert() {
  Reminder r = {alertDue, false, alertTitle};
  const long due = reminders::dueMinutes(r.due);
  drawHeader("Reminder", r.hasTime() ? reminders::clockText(due % 1440).c_str() : "today");
  String lines[3];
  display.setFont(FONT_LARGE);
  int16_t lineH = 30;
  int n = wrapText(alertTitle, display.width() - 2 * MARGIN, lines, 3);
  if (n > 2) {  // a long one: smaller, so more of it fits
    display.setFont(FONT_MEDIUM);
    lineH = 22;
    n = wrapText(alertTitle, display.width() - 2 * MARGIN, lines, 3);
  }
  n = min(n, 3);
  const int16_t mid = (CONTENT_TOP + HINTS_TOP) / 2;
  for (int i = 0; i < n; i++) drawCentered(lines[i].c_str(), mid - (n - 1) * lineH / 2 + i * lineH);
  drawHints("later", "done", "");
}

static void draw() {
  if (screen == ALERT) {
    drawAlert();
    return;
  }
  drawList();
  if (failed.length() && screen == LIST) {
    const bool kept = waitingCount > 0 && failed != "Heard nothing";  // the recordings wait on the card
    drawSheet(kept ? "Not added yet" : "Not added", fitText(failed.c_str(), display.width() - 2 * MARGIN - 16).c_str(),
              kept ? "Kept: tried again later" : "Hold B to try again");
    drawHints("ok", "ok", kept ? "" : "again");
  } else if (screen == CLEAR) {
    const int n = doneCount();
    char title[24];
    snprintf(title, sizeof title, n == 1 ? "Clear 1 done?" : "Clear %d done?", n);
    drawSheet(title, "Ticked ones go.", "Can't be undone.");
    drawHints("keep", "clear", "");
  } else if (rowCount() == 0) {
    drawHints("", "", "add");
  } else {
    const char *b = onWaitingRow() ? (listening ? "" : "send") : onClearRow() ? "clear"
                    : list[cursor - firstItem()].done ? "undo" : "done";
    drawHints(rowCount() > 1 ? "next" : "", b, "add");
  }
  const struct tm t = clockLocal();
  drawnMinute = clockValid() ? t.tm_min : -1;
}

// The job's steps while it listens, and the due tags ("5pm", "late") moving on with the clock.
static Redraw tick() {
  if (listening && jobStatus().gen != seenGen) {
    seenGen = jobStatus().gen;
    return Redraw::Partial;
  }
  if (screen != LIST || drawnMinute < 0 || clockLocal().tm_min == drawnMinute) return Redraw::None;
  return Redraw::Tick;
}

// Every loop, wherever you are: words heard while away from the list are added now (RAM is lost in deep sleep).
static bool background() {
  String problem;
  bool offline;
  if (jobTakeRemindersDone(problem, offline)) {
    listening = false;
    sent(problem, offline);
    return true;
  }
  static uint32_t checked;
  if (checked && millis() - checked < 1000) return false;
  checked = millis() | 1;
  checkDue();
  if (waitingCount > 0 && !listening && retryDue()) sendWaiting(true);
  return false;
}

static void onExit() { isOpen = false; }

// After a restart, reads the card once so the home screen's line (and the Pet) have the list before the first visit.
static void ensureRemembered() {
  static bool looked;
  if (openCount < 0 && !looked) {
    looked = true;
    if (storageCardMount()) ensureList();
  }
}

bool remindersNext(RemindersNext *out) {
  ensureRemembered();
  if (openCount < 0) return false;
  const long now = nowMinutes();
  const bool nextDue = nextMin >= 0 && now >= nextMin;  // the next one may have come due since
  out->late = lateCount + nextDue;
  snprintf(out->lateTitle, sizeof out->lateTitle, "%s", nextDue ? nextTitle : lateTitle);
  out->nextMin = nextDue ? -1 : nextMin;
  out->nextHasTime = nextHasTime;
  snprintf(out->nextTitle, sizeof out->nextTitle, "%s", nextDue ? "" : nextTitle);
  return true;
}

// "1 late", "5pm: Call Sam" (the next one today), "3 to do", from what the last visit saw.
static void status(char *out, size_t len) {
  ensureRemembered();
  const long now = nowMinutes();
  if (openCount < 0) snprintf(out, len, "Your to-do list");
  else if (lateCount + (nextMin >= 0 && now >= nextMin) > 0)  // the next one may have come due since
    snprintf(out, len, "%d late", lateCount + (nextMin >= 0 && now >= nextMin));
  else if (nextMin >= 0 && now >= 0 && nextMin / 1440 == now / 1440 && nextMin > now && nextHasTime)
    snprintf(out, len, "%s: %s", reminders::clockText(nextMin % 1440).c_str(), nextTitle);
  else if (waitingCount > 0) snprintf(out, len, "%d waiting for WiFi", waitingCount);
  else if (openCount > 0) snprintf(out, len, "%d to do", openCount);
  else snprintf(out, len, "Hold B to add one");
}

extern const App remindersApp = {"Reminders", ICON_REMINDERS, onEnter, onButton, draw,      onExit,
                                 onBack,      status,         tick,    true,     background};
