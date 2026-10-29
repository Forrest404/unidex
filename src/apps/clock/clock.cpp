// Clock: a timer, up to 3 alarms, a stopwatch and the time in other cities. The timer and alarms ring wherever you
// are, waking the device from sleep (powerWakeAt) as reminders do, and they ring even with Settings > Alert sound off
// (Volume still counts: 0 is silent). The sums are in clock_logic.h. Hold B on the menu to say what to set ("timer
// for 8 minutes", "alarm at 7 every weekday"): the words come back from Notes' transcription over WiFi, and
// voice_logic.h reads them.
#include <sys/time.h>
#include "clock.h"
#include "clock_logic.h"
#include "voice_logic.h"
#include "../apps.h"
#include "../notes/cloud.h"
#include "../notes/job.h"
#include "../notes/record.h"
#include "../notes/store.h"
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

using clocks::Alarm;

enum Screen : uint8_t { MENU, TIMER, ALARMS, ALARM_EDIT, ALARM_TIME, STOPWATCH, WORLD, RING };
enum MenuRow : uint8_t { M_TIMER, M_ALARM, M_STOPWATCH, M_WORLD, MENU_ROWS };
enum EditRow : uint8_t { E_ON, E_TIME, E_REPEAT, EDIT_ROWS };
enum TimeField : uint8_t { F_HOUR, F_MINUTE, F_DONE };
enum Ringing : uint8_t { NOT_RINGING, RING_TIMER, RING_ALARM };  // RING_ALARM + n for alarm n

static const int ROW_H = 22, WORLD_ROWS = 4, WORLD_ROW_H = 33, LAPS = 3;

RTC_DATA_ATTR static uint8_t screen, menuRow, alarmRow, editRow, timeField, worldRow, ringFrom;
RTC_DATA_ATTR static bool ringOverOpen;      // the Clock was open when it rang: back to where it was
RTC_DATA_ATTR static uint8_t ringing;        // what's ringing (Ringing), 0 if nothing
RTC_DATA_ATTR static time_t ringStart, lastChime;
RTC_DATA_ATTR static int8_t preset = -1;     // the timer's preset (-1: not read from NVS yet)
RTC_DATA_ATTR static time_t timerEndsAt;        // when the running timer ends (0: not running)
RTC_DATA_ATTR static int32_t pausedLeft;     // seconds left while paused (0: not paused)
RTC_DATA_ATTR static bool swRunning;
RTC_DATA_ATTR static int64_t swBase, swSince;  // stopwatch: ms counted before the last start, and when that was
RTC_DATA_ATTR static int64_t laps[LAPS + 1];   // the last laps, newest first (elapsed ms at each; one more than shown)
RTC_DATA_ATTR static uint8_t lapCount;         // all the laps so far (only the last 3 are kept)
RTC_DATA_ATTR static time_t checkedUpTo;       // alarms that ring up to here have rung
RTC_DATA_ATTR static time_t snoozeAt;          // a snoozed alarm rings again then (0: none)
RTC_DATA_ATTR static uint8_t snoozed;          // which alarm
RTC_DATA_ATTR static Alarm draft;              // the alarm being edited
RTC_DATA_ATTR static bool isOpen;

// --- saved settings (NVS): read on a wake, and again each minute (a reset or USB may have changed them) ---

static Alarm alarms[clocks::ALARMS];
static int8_t cities[WORLD_ROWS];
static bool loaded;
static uint32_t loadedAt;

static void load() {
  if (loaded && millis() - loadedAt < 60000) return;
  loaded = true;
  loadedAt = millis();
  for (int i = 0; i < clocks::ALARMS; i++) {
    char key[8];
    snprintf(key, sizeof key, "clk_al%d", i + 1);
    alarms[i] = clocks::unpack(storageGetInt(key, 0));
  }
  // 5 bits a row, the city + 1 (0: none). New York, Tokyo and Paris by default.
  const int32_t packed = storageGetInt("clk_cities", (5 + 1) | (21 + 1) << 5 | (8 + 1) << 10);
  for (int i = 0; i < WORLD_ROWS; i++) {
    const int c = (packed >> (5 * i) & 31) - 1;
    cities[i] = c < clocks::CITY_COUNT ? c : -1;
  }
  if (preset < 0) preset = min<int32_t>(max<int32_t>(storageGetInt("clk_timer", clocks::DEFAULT_PRESET), 0), clocks::PRESET_COUNT - 1);
}

static void saveAlarm(int i) {
  char key[8];
  snprintf(key, sizeof key, "clk_al%d", i + 1);
  storagePutInt(key, clocks::pack(alarms[i]));
  checkedUpTo = 0;  // a fresh look: it rings from its next time
}

static void saveCities() {
  int32_t packed = 0;
  for (int i = 0; i < WORLD_ROWS; i++) packed |= (cities[i] + 1) << (5 * i);
  storagePutInt("clk_cities", packed);
}

// --- time ---

static int64_t nowMs() {  // wall time in ms: it carries on through deep sleep, unlike millis()
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  return (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static long timerLeft() {  // seconds
  if (pausedLeft) return pausedLeft;
  if (!timerEndsAt) return clocks::PRESETS[preset];
  return max<long>(0, (long)(timerEndsAt - time(nullptr)));
}

static int64_t swElapsed() { return swBase + (swRunning ? nowMs() - swSince : 0); }

static std::string swText(int64_t ms, bool tenths) {  // "2:31", "2:31.4", "1:02:31"
  std::string t = clocks::duration((long)(ms / 1000));
  if (tenths) t += "." + std::to_string((int)(ms / 100 % 10));
  return t;
}

static std::string localHhmm(time_t t) {  // "14:25" here
  const clocks::Local l = clocks::localIn(clocks::CITIES[clocks::HOME], t);
  return clocks::hhmm(l.hour, l.minute);
}

// The next alarm to ring (-1: none on), and when.
static int nextAlarm(time_t &at) {
  int which = -1;
  at = 0;
  if (!clockValid()) return -1;
  const time_t now = time(nullptr);
  for (int i = 0; i < clocks::ALARMS; i++) {
    if (!alarms[i].on) continue;
    const long long r = clocks::nextRing(alarms[i], now);
    if (r > 0 && (which < 0 || r < at)) which = i, at = (time_t)r;
  }
  if (snoozeAt && (which < 0 || snoozeAt < at)) which = snoozed, at = snoozeAt;
  return which;
}

// --- ringing ---

static void chime() {
  lastChime = time(nullptr);
  audioChime();  // not muted by Settings > Alert sound: you set this one yourself
  powerActivity();
}

extern const App clockApp;

static void ring(uint8_t what) {
  if (screen != RING) {
    ringOverOpen = isOpen;
    ringFrom = screen;
  }
  ringing = what;
  ringStart = time(nullptr);
  screen = RING;
  powerActivity();
  launcherInterrupt(&clockApp);
  chime();
}

// Once a second, open or not: a timer ending or an alarm due rings; the chime repeats while it rings; and the device
// is told when to wake for the next one.
static bool background() {
  static uint32_t checked;
  if (checked && millis() - checked < 1000) return false;
  checked = millis() | 1;
  load();
  const time_t now = time(nullptr);
  if (ringing) {
    if (now - ringStart < clocks::RING_FOR && now - lastChime >= clocks::RING_EVERY) chime();
    return false;  // anything else falling due waits until this one is stopped
  }
  if (timerEndsAt && now >= timerEndsAt) {
    timerEndsAt = 0;
    ring(RING_TIMER);
    return false;
  }
  if (clockValid()) {
    // A fresh start, or the clock went back: only what rings from now on counts
    if (!checkedUpTo || now < checkedUpTo - 60) checkedUpTo = now;
    if (snoozeAt && now >= snoozeAt) {
      snoozeAt = 0;
      ring(RING_ALARM + snoozed);
      return false;
    }
    for (int i = 0; i < clocks::ALARMS; i++) {
      if (!alarms[i].on) continue;
      const long long r = clocks::nextRing(alarms[i], checkedUpTo);
      if (r > 0 && r <= now) {
        checkedUpTo = (time_t)r;
        if (alarms[i].repeat == clocks::ONCE) alarms[i].on = false, saveAlarm(i), checkedUpTo = (time_t)r;
        ring(RING_ALARM + i);
        return false;
      }
    }
    checkedUpTo = now;
  }
  time_t wake = timerEndsAt, at;
  if (nextAlarm(at) >= 0 && (!wake || at < wake)) wake = at;
  powerWakeAt(WakeFor::Clock, wake);
  return false;
}

static Redraw stopRinging(bool again) {
  const time_t now = time(nullptr);
  if (again && ringing == RING_TIMER) timerEndsAt = now + clocks::TIMER_EXTRA, launcherToast("A minute more");
  else if (again) snoozeAt = now + clocks::SNOOZE, snoozed = ringing - RING_ALARM, launcherToast("Again in 9 min");
  ringing = NOT_RINGING;
  screen = ringFrom;
  return ringOverOpen ? Redraw::Full : Redraw::Exit;
}

// --- drawing ---

static void drawRow(int i, bool selected, const char *label, const char *val, int16_t rowH = ROW_H) {
  const int16_t top = CONTENT_TOP + 4 + i * rowH, baseline = top + rowH / 2 + 4;
  if (selected) display.fillRect(MARGIN - 4, top, display.width() - 2 * (MARGIN - 4), rowH - 2, BLACK);
  display.setTextColor(selected ? WHITE : BLACK);
  display.setFont(FONT_SMALL);
  display.setCursor(MARGIN, baseline);
  display.print(label);
  drawRight(val, baseline);
  display.setTextColor(BLACK);
}

static std::string alarmWords(const Alarm &a) { return a.on ? clocks::REPEAT_NAMES[a.repeat] : "Off"; }

static void drawMenu() {
  drawHeader("Clock", clockValid() ? localHhmm(time(nullptr)).c_str() : nullptr);
  std::string timer = timerEndsAt ? clocks::duration(timerLeft()) + " left"
                      : pausedLeft ? "paused " + clocks::duration(pausedLeft)
                                   : clocks::presetName(preset);
  time_t at;
  const int next = nextAlarm(at);
  std::string alarm = next < 0 ? "Off" : localHhmm(at) + " " + clocks::whenWords(at, time(nullptr));
  std::string sw = swElapsed() ? swText(swElapsed(), false) : "";
  std::string world;
  for (int i = 0; i < WORLD_ROWS && world.empty(); i++)
    if (cities[i] >= 0 && clockValid()) {
      const clocks::Local l = clocks::localIn(clocks::CITIES[cities[i]], time(nullptr));
      world = std::string(clocks::CITIES[cities[i]].name) + " " + clocks::hhmm(l.hour, l.minute);
    }
  drawRow(M_TIMER, menuRow == M_TIMER, "Timer", timer.c_str());
  drawRow(M_ALARM, menuRow == M_ALARM, "Alarm", alarm.c_str());
  drawRow(M_STOPWATCH, menuRow == M_STOPWATCH, "Stopwatch", sw.c_str());
  drawRow(M_WORLD, menuRow == M_WORLD, "World", world.c_str());
  drawHints("next", "open", "say it");
}

static void drawBig(const char *text, int16_t cy) {
  display.setFont(FONT_LARGE);
  drawCentered(text, cy);
}

static void drawTimer() {
  drawHeader("Timer");
  const int16_t mid = (CONTENT_TOP + HINTS_TOP) / 2;
  drawBig(clocks::duration(timerLeft()).c_str(), mid - 8);
  display.setFont(FONT_SMALL);
  if (timerEndsAt) {
    drawCentered(("Ends at " + localHhmm(timerEndsAt)).c_str(), mid + 30);
    drawHints("pause", "", "reset");
  } else if (pausedLeft) {
    drawCentered("Paused", mid + 30);
    drawHints("resume", "", "reset");
  } else {
    drawCentered(clocks::presetName(preset).c_str(), mid + 30);
    drawHints("start", "longer", "shorter");
  }
}

static void drawAlarms() {
  drawHeader("Alarms", clockValid() ? nullptr : "time not set");
  for (int i = 0; i < clocks::ALARMS; i++) {
    const Alarm &a = alarms[i];
    drawRow(i, alarmRow == i, clocks::hhmm(a.hour, a.minute).c_str(), alarmWords(a).c_str(), 26);
  }
  drawHints("next", "change", "");
}

static void drawAlarmEdit() {
  char title[12];
  snprintf(title, sizeof title, "Alarm %d", alarmRow + 1);
  drawHeader(title);
  drawRow(E_ON, editRow == E_ON, "Alarm", draft.on ? "On" : "Off", 26);
  drawRow(E_TIME, editRow == E_TIME, "Time", clocks::hhmm(draft.hour, draft.minute).c_str(), 26);
  drawRow(E_REPEAT, editRow == E_REPEAT, "Repeat", clocks::REPEAT_NAMES[draft.repeat], 26);
  drawHints("next", editRow == E_ON ? (draft.on ? "turn off" : "turn on") : "change", "");
}

// Text pieces in a line, centred; the piece at `active` in a black box (as Settings' date and time).
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

static void drawAlarmTime() {
  drawHeader("Alarm time");
  char hh[4], mm[4];
  snprintf(hh, sizeof hh, "%02d", draft.hour);
  snprintf(mm, sizeof mm, "%02d", draft.minute);
  const char *time[] = {hh, ":", mm}, *done[] = {"Done"};
  display.setFont(FONT_LARGE);
  drawPieces(time, 3, timeField == F_HOUR ? 0 : timeField == F_MINUTE ? 2 : -1, 80, 36);
  display.setFont(FONT_SMALL);
  drawPieces(done, 1, timeField == F_DONE ? 0 : -1, 130, 24);
  drawHints("next", timeField == F_DONE ? "done" : "+1", timeField == F_DONE ? "" : "-1");
}

static void drawStopwatch() {
  drawHeader("Stopwatch");
  const int64_t ms = swElapsed();
  drawBig(swText(ms, !swRunning).c_str(), CONTENT_TOP + 34);
  display.setFont(FONT_SMALL);
  if (swRunning) drawCentered(("since " + localHhmm((time_t)(swSince / 1000))).c_str(), CONTENT_TOP + 64);
  for (int i = 0; i < min<int>(lapCount, LAPS); i++) {  // newest first
    const int16_t baseline = CONTENT_TOP + 92 + i * 18;
    char label[12];
    snprintf(label, sizeof label, "Lap %d", lapCount - i);
    display.setCursor(MARGIN, baseline);
    display.print(label);
    const int64_t split = laps[i] - (i + 1 < lapCount ? laps[i + 1] : 0);
    drawRight(swText(split, true).c_str(), baseline);
  }
  if (swRunning) drawHints("lap", "stop", "reset");
  else drawHints("", ms ? "go on" : "start", ms ? "reset" : "");
}

static void drawWorld() {
  drawHeader("World", clockValid() ? ("London " + localHhmm(time(nullptr))).c_str() : "time not set");
  const time_t now = time(nullptr);
  const clocks::Local home = clocks::localIn(clocks::CITIES[clocks::HOME], now);
  for (int i = 0; i < WORLD_ROWS; i++) {
    const int16_t top = CONTENT_TOP + 3 + i * WORLD_ROW_H;
    const bool sel = worldRow == i;
    if (sel) display.fillRect(MARGIN - 4, top, display.width() - 2 * (MARGIN - 4), WORLD_ROW_H - 2, BLACK);
    display.setTextColor(sel ? WHITE : BLACK);
    display.setFont(FONT_SMALL);
    display.setCursor(MARGIN, top + 15);
    if (cities[i] < 0) {
      display.print("None");
    } else {
      const clocks::City &c = clocks::CITIES[cities[i]];
      display.print(c.name);
      if (clockValid()) {
        const clocks::Local l = clocks::localIn(c, now);
        std::string note = clocks::difference(c, now);
        if (l.days > home.days) note += ", tomorrow";
        else if (l.days < home.days) note += ", yesterday";
        display.setFont(FONT_MEDIUM);
        drawRight(clocks::hhmm(l.hour, l.minute).c_str(), top + 20);
        display.setFont(FONT_TINY);
        display.setCursor(MARGIN, top + 27);
        display.print(note.c_str());
      }
    }
    display.setTextColor(BLACK);
  }
  drawHints("next", "city", "previous");
}

static void drawRing() {
  const bool timer = ringing == RING_TIMER;
  const Alarm &a = alarms[timer ? 0 : (ringing - RING_ALARM) % clocks::ALARMS];
  drawHeader(timer ? "Timer" : "Alarm");
  const int16_t mid = (CONTENT_TOP + HINTS_TOP) / 2;
  drawBig(timer ? "Time's up" : clocks::hhmm(a.hour, a.minute).c_str(), mid - 6);
  display.setFont(FONT_SMALL);
  if (!timer && a.repeat != clocks::ONCE) drawCentered(clocks::REPEAT_NAMES[a.repeat], mid + 28);
  drawHints(timer ? "+1 min" : "snooze", "stop", "");
}

static void draw() {
  load();
  switch (screen) {
    case TIMER: drawTimer(); break;
    case ALARMS: drawAlarms(); break;
    case ALARM_EDIT: drawAlarmEdit(); break;
    case ALARM_TIME: drawAlarmTime(); break;
    case STOPWATCH: drawStopwatch(); break;
    case WORLD: drawWorld(); break;
    case RING: drawRing(); break;
    default: drawMenu(); break;
  }
}

// --- buttons ---

static void startTimer() {
  timerEndsAt = time(nullptr) + (pausedLeft ? pausedLeft : clocks::PRESETS[preset]);
  pausedLeft = 0;
  storagePutInt("clk_timer", preset);  // skips the write if unchanged
}

// --- by voice ---

static const uint32_t VOICE_SECONDS = 10;  // a command is a few words

static void drawWorking() {
  drawHeader("Clock");
  drawEmpty("Working it out", "a few seconds");
}

// Records while B is held, then has it transcribed over WiFi (blocking: a few seconds). nullptr with the words in
// `heard`, or why not, for a toast.
static const char *hear(String &heard) {
  if (!credWifiAny()) return "Voice needs WiFi: Setup";
  if (const char *missing = cloudTranscribeMissing()) return missing;
  if (jobBusy() || netClaimed()) return "WiFi busy: try again soon";
  if (const char *low = batteryTooLow()) return low;
  if (!jobCanTake()) return "Still saving a note";
  int16_t *samples;
  size_t count;
  if (const char *err = recordWhileHeld(VOICE_SECONDS, samples, count)) return err;
  displayShow(drawWorking, false);
  netClaim(true);  // the WiFi is ours until the words are back: Notes' background sends wait
  powerHold();
  const char *err = netConnect();
  if (err) {
    err = "Voice needs WiFi";
  } else {
    uint8_t wav[44];
    storeWavHeader(wav, count * 2);
    const NetPart audio[] = {NetPart::bytes(wav, sizeof wav), NetPart::bytes(samples, count * 2)};
    err = cloudTranscribe(audio, 2, heard);
  }
  jobGiveBuffer(samples);
  netOff();
  netClaim(false);
  powerRelease();
  return err;
}

// Does what the command asks, and opens what it changed. `said` is the toast: what was done, or why not.
static void doCommand(const clockvoice::Command &c, String &said) {
  const time_t now = time(nullptr);
  switch (c.kind) {
    case clockvoice::Kind::TIMER:
      timerEndsAt = now + c.seconds, pausedLeft = 0, screen = TIMER;
      said = ("Timer " + clocks::duration(c.seconds)).c_str();
      return;
    case clockvoice::Kind::TIMER_STOP:
      said = timerEndsAt || pausedLeft ? "Timer stopped" : "No timer running";
      timerEndsAt = 0, pausedLeft = 0;
      return;
    case clockvoice::Kind::ALARM: {
      int slot = -1;  // the alarm already at that time, or else the first that's off
      for (int i = 0; i < clocks::ALARMS && slot < 0; i++)
        if (alarms[i].hour == c.hour && alarms[i].minute == c.minute) slot = i;
      for (int i = 0; i < clocks::ALARMS && slot < 0; i++)
        if (!alarms[i].on) slot = i;
      if (slot < 0) {
        said = "3 alarms on: turn one off";
        return;
      }
      alarms[slot] = {true, (uint8_t)c.hour, (uint8_t)c.minute, c.repeat};
      saveAlarm(slot);
      if (snoozed == slot) snoozeAt = 0;
      screen = ALARMS, alarmRow = slot;
      said = ("Alarm " + clocks::hhmm(c.hour, c.minute)).c_str();
      if (!clockValid()) said += ": clock not set";
      else if (c.repeat != clocks::ONCE) said += String(" ") + clocks::REPEAT_NAMES[c.repeat];
      else said += (" " + clocks::whenWords((time_t)clocks::nextRing(alarms[slot], now), now)).c_str();
      return;
    }
    case clockvoice::Kind::ALARMS_OFF:
      for (int i = 0; i < clocks::ALARMS; i++)
        if (alarms[i].on) alarms[i].on = false, saveAlarm(i);
      snoozeAt = 0;
      said = "Alarms off";
      return;
    case clockvoice::Kind::STOPWATCH_START:
      if (!swRunning) swSince = nowMs(), swRunning = true;
      screen = STOPWATCH;
      said = "Stopwatch going";
      return;
    case clockvoice::Kind::STOPWATCH_STOP:
      if (swRunning) swBase = swElapsed(), swRunning = false;
      screen = STOPWATCH;
      said = "Stopwatch stopped";
      return;
    case clockvoice::Kind::STOPWATCH_RESET:
      swRunning = false, swBase = 0, lapCount = 0;
      screen = STOPWATCH;
      said = "Stopwatch reset";
      return;
    default:
      return;
  }
}

// Does what the words ask (doCommand).
static void act(const String &words, String &said) {
  const time_t now = time(nullptr);
  const clocks::Local here = clocks::localIn(clocks::CITIES[clocks::HOME], now);
  const clockvoice::Command c = clockvoice::parse(asciiText(words).c_str(), clockValid() ? here.hour * 60 + here.minute : -1);
  if (c.kind == clockvoice::Kind::NONE) said = words.length() ? "Try \"timer 5 min\"" : "Heard nothing";  // (~17 chars)
  else doCommand(c, said);
}

String clockCommand(const clockvoice::Command &c) {
  load();
  String said;
  const uint8_t was = screen;
  doCommand(c, said);
  if (ringing) screen = was;  // something's ringing: its screen stays, so it can be stopped
  return said;
}

String clockState() {
  load();
  const time_t now = time(nullptr);
  String s;
  if (timerEndsAt) s = ("timer running, " + clocks::duration(max<time_t>(timerEndsAt - now, 0)) + " left").c_str();
  else if (pausedLeft) s = ("timer paused, " + clocks::duration(pausedLeft) + " left").c_str();
  else s = "no timer";
  for (int i = 0; i < clocks::ALARMS; i++) {
    const Alarm &a = alarms[i];
    s += String("; alarm ") + String(i + 1) + " " + clocks::hhmm(a.hour, a.minute).c_str() + " " +
         clocks::REPEAT_NAMES[a.repeat] + (a.on ? " on" : " off");
  }
  s += swRunning ? "; stopwatch running" : swBase ? "; stopwatch stopped" : "; stopwatch at zero";
  return s;
}

static Redraw voice() {
  String heard, said;
  if (const char *err = hear(heard)) said = err;
  else act(heard, said);
  launcherToast(said.c_str());
  return Redraw::Full;
}

#if UNIDEX_DEV
const char *clockSay(const char *words) {  // X CLOCKSAY: as if it had been said
  static String said;
  load();
  act(words, said);
  return said.c_str();
}
#endif

static Redraw onButton(Event e) {
  load();
  const bool a = e == Event::AShort, b = e == Event::BShort, holdB = e == Event::BLong;
  switch (screen) {
    case RING:
      if (a || b) return stopRinging(a);
      return Redraw::None;
    case MENU:
      if (holdB) return voice();
      if (a) menuRow = (menuRow + 1) % MENU_ROWS;
      else if (b) {
        screen = menuRow == M_TIMER ? TIMER : menuRow == M_ALARM ? ALARMS : menuRow == M_STOPWATCH ? STOPWATCH : WORLD;
        alarmRow = worldRow = 0;  // each opens at the top
      }
      else return Redraw::None;
      return b ? Redraw::Full : Redraw::Partial;
    case TIMER:
      if (timerEndsAt) {  // running: A pauses, hold B resets
        if (a) pausedLeft = max<long>(1, timerLeft()), timerEndsAt = 0;
        else if (holdB) timerEndsAt = 0;
        else return Redraw::None;
      } else if (pausedLeft) {
        if (a) startTimer();
        else if (holdB) pausedLeft = 0;
        else return Redraw::None;
      } else {
        if (a) startTimer();
        else if (b) preset = (preset + 1) % clocks::PRESET_COUNT;
        else if (holdB) preset = (preset + clocks::PRESET_COUNT - 1) % clocks::PRESET_COUNT;
        else return Redraw::None;
      }
      return Redraw::Partial;
    case ALARMS:
      if (a) alarmRow = (alarmRow + 1) % clocks::ALARMS;
      else if (b) draft = alarms[alarmRow], editRow = E_ON, screen = ALARM_EDIT;
      else return Redraw::None;
      return Redraw::Partial;
    case ALARM_EDIT:
      if (a) editRow = (editRow + 1) % EDIT_ROWS;
      else if (b && editRow == E_ON) draft.on = !draft.on;
      else if (b && editRow == E_TIME) timeField = F_HOUR, screen = ALARM_TIME;
      else if (b && editRow == E_REPEAT) draft.repeat = (clocks::Repeat)((draft.repeat + 1) % clocks::REPEATS), draft.on = true;
      else return Redraw::None;
      return Redraw::Partial;
    case ALARM_TIME:
      if (a) timeField = (timeField + 1) % (F_DONE + 1);
      else if (b && timeField == F_DONE) draft.on = true, screen = ALARM_EDIT;  // setting a time turns it on
      else if ((b || holdB) && timeField == F_HOUR) draft.hour = (draft.hour + (b ? 1 : 23)) % 24;
      else if ((b || holdB) && timeField == F_MINUTE) draft.minute = (draft.minute + (b ? 1 : 59)) % 60;
      else return Redraw::None;
      return Redraw::Partial;
    case STOPWATCH:
      if (b && swRunning) swBase = swElapsed(), swRunning = false;
      else if (b) swSince = nowMs(), swRunning = true;
      else if (a && swRunning) {
        for (int i = LAPS; i > 0; i--) laps[i] = laps[i - 1];
        laps[0] = swElapsed();
        lapCount = min(lapCount + 1, 99);
      } else if (holdB && (swRunning || swBase)) swRunning = false, swBase = 0, lapCount = 0;
      else return Redraw::None;
      return Redraw::Partial;
    case WORLD: {
      if (a) {
        worldRow = (worldRow + 1) % WORLD_ROWS;
        return Redraw::Partial;
      }
      if (!b && !holdB) return Redraw::None;
      // B: the next city (None between the last and the first), hold B the one before
      const int n = clocks::CITY_COUNT + 1, at = cities[worldRow] + 1;
      cities[worldRow] = (at + (b ? 1 : n - 1)) % n - 1;
      saveCities();
      return Redraw::Partial;
    }
  }
  return Redraw::None;
}

static Redraw onBack() {
  switch (screen) {
    case RING: return stopRinging(false);
    case ALARM_TIME: screen = ALARM_EDIT; return Redraw::Partial;  // back without changing the time
    case ALARM_EDIT:
      alarms[alarmRow] = draft;
      saveAlarm(alarmRow);
      if (snoozed == alarmRow) snoozeAt = 0;
      screen = ALARMS;
      return Redraw::Partial;
    case MENU: return Redraw::Exit;
    default: screen = MENU; return Redraw::Full;
  }
}


#if UNIDEX_DEV
// Test build (X STATE detail): each screen and what it shows.
static const char *detail() {
  static char buf[48];
  switch (screen) {
    case TIMER: snprintf(buf, sizeof buf, "timer:%s:%ld", timerEndsAt ? "run" : pausedLeft ? "pause" : "set", timerLeft()); break;
    case ALARMS: snprintf(buf, sizeof buf, "alarms:%d", alarmRow); break;
    case ALARM_EDIT: snprintf(buf, sizeof buf, "edit:%d:%d:%ld", alarmRow, editRow, (long)clocks::pack(draft)); break;
    case ALARM_TIME: snprintf(buf, sizeof buf, "time:%d:%02d:%02d", timeField, draft.hour, draft.minute); break;
    case STOPWATCH: snprintf(buf, sizeof buf, "stopwatch:%s:%d", swRunning ? "run" : "stop", lapCount); break;
    case WORLD: snprintf(buf, sizeof buf, "world:%d:%d,%d,%d,%d", worldRow, cities[0], cities[1], cities[2], cities[3]); break;
    case RING: snprintf(buf, sizeof buf, "ring:%s", ringing == RING_TIMER ? "timer" : ringing ? "alarm" : "none"); break;
    default: snprintf(buf, sizeof buf, "menu:%d", menuRow); break;
  }
  return buf;
}
#endif

static void onEnter() {
  isOpen = true;
  load();
  if (screen != RING) screen = MENU, menuRow = 0;
#if UNIDEX_DEV
  devSetDetail(detail);  // (and in tick, for a wake from sleep, which skips this)
#endif
}

static void onExit() {
  isOpen = false;
#if UNIDEX_DEV
  devSetDetail(nullptr);
#endif
}

// While open: the timer, stopwatch and clock move on each second (or minute), and the light sleep ends in time.
static Redraw tick() {
#if UNIDEX_DEV
  devSetDetail(detail);
#endif
  static long shown = -1;
  long now;
  if ((screen == TIMER && timerEndsAt) || (screen == STOPWATCH && swRunning) || (screen == MENU && (timerEndsAt || swRunning))) {
    now = (long)(nowMs() / 1000);
    powerWakeWithin(1000 - nowMs() % 1000 + 5);
  } else if (screen == MENU || screen == WORLD) {
    now = (long)(time(nullptr) / 60);
  } else {
    shown = -1;
    return Redraw::None;
  }
  if (now == shown) return Redraw::None;
  const bool first = shown < 0;
  shown = now;
  return first ? Redraw::None : Redraw::Tick;
}

// "Timer: 5 min left", "Stopwatch running", "Alarm 7:00 tomorrow", "Set an alarm". Whole minutes, so the home screen
// doesn't redraw every second.
static void status(char *out, size_t len) {
  load();
  if (ringing) snprintf(out, len, "%s", ringing == RING_TIMER ? "Time's up" : "Alarm ringing");
  else if (timerEndsAt) snprintf(out, len, "Timer: %ld min left", (timerLeft() + 59) / 60);
  else if (pausedLeft) snprintf(out, len, "Timer paused");
  else if (swRunning) snprintf(out, len, "Stopwatch running");
  else {
    time_t at;
    if (nextAlarm(at) >= 0) snprintf(out, len, "Alarm %s %s", localHhmm(at).c_str(), clocks::whenWords(at, time(nullptr)).c_str());
    else snprintf(out, len, "Set an alarm");
  }
}

extern const App clockApp = {"Clock", ICON_CLOCK, onEnter, onButton, draw, onExit, onBack, status, tick, false, background};
