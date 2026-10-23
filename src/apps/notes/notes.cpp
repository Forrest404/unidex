// Notes: hold B to record a voice note, let go to stop. It's transcribed (Whisper), tidied up (OpenAI or
// Claude), kept on the SD card and, if switched on, pushed to GitHub as Markdown (for Obsidian). Set up
// from the website's Setup page. Without a card it still works online, but nothing is kept on the device.
// The online steps run in the background (job.h): letting go of B returns to the screen at once.
// Main: hold B = record, B = sync waiting notes, A = list. List: A = next, B = open, hold B = delete.
// Note: A = next page, B = play the recording (play.h; any button stops it), hold B = open on phone (starts the
// hotspot, phone.h, and shows its QR code until hold A). Hold A = back everywhere.
#include <vector>
#include <qrcode.h>
#include "job.h"
#include "phone.h"
#include "play.h"
#include "record.h"
#include "store.h"
#include "../../core/app.h"
#include "../../core/battery.h"
#include "../../core/clock.h"
#include "../../core/credentials.h"
#include "../../core/devtools.h"
#include "../../core/display.h"
#include "../../core/launcher.h"
#include "../../core/net.h"
#include "../../core/storage.h"
#include "../../core/theme.h"

static const uint32_t MAX_SECONDS = 180, AUTO_SYNC_S = 600;  // waiting notes: tried by themselves every 10 min
static const int ROWS = 5, ROW_H = 26, LINES = 7, LINE_H = 19;

enum Screen : uint8_t { MAIN, LIST, VIEW, CONFIRM, PHONE };
RTC_DATA_ATTR static uint8_t screen, cursor, page;
RTC_DATA_ATTR static char openId[24];  // the note on screen in VIEW / CONFIRM
RTC_DATA_ATTR static int16_t noteCount = -1, waitingCount;  // for the home screen (it can't read the card)
RTC_DATA_ATTR static time_t lastAutoSync;  // clock time of the last try by itself (millis start again after sleep)

// RAM caches: lost in deep sleep, rebuilt on first use after a wake.
static std::vector<NoteInfo> notes;
static bool listed;
static std::vector<String> lines;  // the open note, wrapped
static String linesFor, viewTitle;  // viewTitle: a note shown but not kept (no card, no GitHub)
static bool hasWav;  // the open note's recording is on the card (it can be played)
static String message;   // one line under the main screen's prompt: the last result or a problem
static uint32_t seenGen;  // the job status last taken in (results arriving while away show on return)
static uint32_t phoneShown;  // pages the phone had loaded when the screen was last drawn

static int8_t card = -1;  // SD card readable: checked once per visit (with no card, each check takes a while)

static bool githubOn() { return credGet("gh_on") == "1"; }

static bool hasCard() {
  if (card < 0) card = storeReady();
  return card;
}

#if UNIDEX_DEV
// Test build, X DEMO 1: sample notes, for screenshots without anyone's notes.
static const char *const DEMO_NOTES[][2] = {
  {"Physics: forces recap",
   "---\ntitle: Physics: forces recap\n---\n> [!summary] Newton's three laws, with the examples from today's class.\n\n"
   "**Key points**\n- A moving object keeps going at the same speed unless a force acts on it.\n"
   "- Force = mass x acceleration: twice the force, twice the acceleration.\n"
   "- Every push has an equal push back.\n\n**To do**\n- Questions 4 to 9 by Friday.\n"},
  {"Robotics club stall ideas",
   "---\ntitle: Robotics club stall ideas\n---\n> [!summary] Things to bring and show at the open day stall.\n\n"
   "- A line-following robot on a short track.\n- Sign-up sheet and stickers.\n"},
  {"Maths homework plan", "---\ntitle: Maths homework plan\n---\n- Monday: exercise 4B.\n- Wednesday: past paper, section A.\n"},
  {"Books for English", "---\ntitle: Books for English\n---\n- Of Mice and Men\n- An Inspector Calls\n"},
  {"Questions for open day", "---\ntitle: Questions for open day\n---\n- How big are the classes?\n- Which clubs run after school?\n"},
};
static bool listedDemo;
#endif

static void ensureList() {
#if UNIDEX_DEV
  if (listed && listedDemo != devDemo()) listed = false;
  listedDemo = devDemo();
#endif
  if (listed) return;
  notes = hasCard() ? storeList() : std::vector<NoteInfo>();
#if UNIDEX_DEV
  if (devDemo()) {
    notes.clear();
    for (size_t i = 0; i < sizeof DEMO_NOTES / sizeof *DEMO_NOTES; i++)
      notes.push_back({"demo" + String(i), DEMO_NOTES[i][0], true, true});
  }
#endif
  listed = true;
  if (cursor >= notes.size()) cursor = 0;
  const bool gh = githubOn();
  waitingCount = 0;
  for (const NoteInfo &i : notes) waitingCount += !i.text || (gh && !i.pushed);
  noteCount = notes.size();
}

static int waiting() { return waitingCount; }

static String ascii(const String &s) { return asciiText(s); }

// Words into lines that fit the screen width in the small font.
static void wrap(const String &text, std::vector<String> &out) {
  display.setFont(FONT_SMALL);
  const int16_t width = display.width() - 2 * MARGIN;
  for (int start = 0; start <= (int)text.length();) {
    int end = text.indexOf('\n', start);
    if (end < 0) end = text.length();
    String para = text.substring(start, end), line;
    para.trim();
    if (!para.length()) {
      if (out.size() && out.back().length()) out.push_back("");
    }
    int from = 0;
    while (from < (int)para.length()) {
      int space = para.indexOf(' ', from);
      if (space < 0) space = para.length();
      const String word = para.substring(from, space);
      const String trial = line.length() ? line + " " + word : word;
      int16_t x, y;
      uint16_t w, h;
      display.getTextBounds(trial.c_str(), 0, 0, &x, &y, &w, &h);
      if (w > width && line.length()) {
        out.push_back(line);
        line = word;
      } else {
        line = trial;
      }
      from = space + 1;
    }
    if (line.length()) out.push_back(line);
    start = end + 1;
  }
  while (out.size() && !out.back().length()) out.pop_back();
}

// What to show of a note: the summary and the body, without front matter, transcript or topic links.
static String readable(const String &md) {
  String body = md;
  if (body.startsWith("---")) {
    const int end = body.indexOf("\n---", 3);
    body = end < 0 ? "" : body.substring(body.indexOf('\n', end + 1) + 1);
  }
  const int quote = body.indexOf("\n> [!quote]");
  if (quote >= 0) body = body.substring(0, quote);
  const int topics = body.indexOf("\n---\nTopics:");
  if (topics >= 0) body = body.substring(0, topics);
  body.replace("> [!summary] ", "");
  body.replace("**", "");
  body.replace("- ", "* ");
  body.trim();
  return ascii(body);
}

static void ensureLines() {
  if (linesFor == openId) return;
  lines.clear();
#if UNIDEX_DEV
  const String md = devDemo() && strncmp(openId, "demo", 4) == 0 ? String(DEMO_NOTES[atoi(openId + 4) % 5][1])
                                                                 : storeReadNote(openId);
#else
  const String md = storeReadNote(openId);
#endif
  if (md.length()) wrap(readable(md), lines);
  else wrap("Not sent yet. It goes by itself when WiFi is in reach. B to listen.", lines);
  hasWav = hasCard() && storeValidId(openId) && storageExists(storeWavPath(openId).c_str());
  linesFor = openId;
}

// A note's title, or for one not transcribed yet, when it was recorded ("2 Oct, 21:50", from its id).
static String titleOf(const NoteInfo &n) {
  if (n.text) return n.title.length() ? ascii(n.title) : n.id;
  static const char *MONTHS[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  const int month = n.id.substring(4, 6).toInt();
  if (n.id.length() < 13 || !isdigit((unsigned char)n.id[0]) || month < 1 || month > 12) return n.id;
  return String(n.id.substring(6, 8).toInt()) + " " + MONTHS[month - 1] + ", " + n.id.substring(9, 11) + ":" +
         n.id.substring(11, 13);
}

// --- drawing ---

static const char *stepName(JobStep s) {
  switch (s) {
    case JobStep::Saving: return "Saving";
    case JobStep::Connecting: return "Connecting";
    case JobStep::Transcribing: return "Transcribing";
    case JobStep::Tidying: return "Tidying up";
    case JobStep::Pushing: return "To GitHub";
    default: return "";
  }
}

static int stepPercent(JobStep s) {
  static const int PCT[] = {0, 10, 25, 50, 75, 90};
  return PCT[(int)s];
}

static bool sending(const NoteInfo &n) { return jobBusy() && n.id == jobStatus().noteId; }

static void drawMain() {
  ensureList();
  drawHeader("Notes");
  drawIcon(ICON_NOTES, (display.width() - ICON_SIZE) / 2, CONTENT_TOP + 6, BLACK);
  if (jobBusy()) {  // the online steps, running in the background
    const JobStep step = jobStatus().step;
    display.setFont(FONT_MEDIUM);
    drawCentered(stepName(step), 92);
    drawProgress(116, stepPercent(step));
    display.setFont(FONT_SMALL);
    drawCentered("You can keep using it", 140);
  } else {
    display.setFont(FONT_LARGE);
    drawCentered("Hold B", 92);
    display.setFont(FONT_SMALL);
    String line = ascii(message);
    if (!line.length() && !credWifiAny()) line = "Setup: add your WiFi";
    else if (!line.length() && !credHas("openai_key")) line = "Setup: add OpenAI key";
    else if (!line.length() && !hasCard()) line = githubOn() ? "No SD card: GitHub only" : "No SD card: nothing is kept";
    else if (!line.length() && waiting()) line = String(waiting()) + " waiting: sends on WiFi";
    else if (!line.length()) line = "to record a note";
    drawCentered(fitText(line.c_str(), display.width() - 2 * MARGIN).c_str(), 128);
  }
  drawHints(hasCard() ? "list" : "", waiting() && !jobBusy() ? "sync" : "", jobCanTake() ? "record" : "");
}

static void drawListRows() {
  ensureList();
  drawHeader("Notes");
  display.setFont(FONT_SMALL);
  const int first = cursor / ROWS * ROWS;
  for (int r = 0; r < ROWS && first + r < (int)notes.size(); r++) {
    const int16_t top = CONTENT_TOP + 4 + r * ROW_H, baseline = top + 17;
    const bool sel = first + r == cursor;
    if (sel) display.fillRect(MARGIN - 4, top, display.width() - 2 * (MARGIN - 4), ROW_H - 2, BLACK);
    display.setTextColor(sel ? WHITE : BLACK);
    const NoteInfo &n = notes[first + r];
    const char *tag = sending(n) ? "sending" : !n.text || (githubOn() && !n.pushed) ? "waiting" : "";
    display.setFont(FONT_TINY);
    drawRight(tag, baseline);
    const int16_t tagW = *tag ? textWidth(tag) + 6 : 0;
    display.setFont(FONT_SMALL);
    display.setCursor(MARGIN, baseline);
    display.print(fitText(titleOf(n).c_str(), display.width() - 2 * MARGIN - tagW));
    display.setTextColor(BLACK);
  }
}

static void drawList() {
  drawListRows();
  if (notes.empty()) {
    drawEmpty("No notes yet", "Hold B on the Notes", "screen to record one");
    drawHints("", "", "");
    return;
  }
  drawHints(notes.size() > 1 ? "next" : "", "open", "delete");
}

// "Open on phone": the hotspot's QR code (the camera joins with it), then the name and password to type.
static void drawPhone() {
  phoneShown = phoneServed();
  drawHeader("Open on phone");
  QRCode qr;
  uint8_t modules[qrcode_getBufferSize(3)];
  if (qrcode_initText(&qr, modules, 3, ECC_LOW, phoneJoinCode()) == 0) {  // version 3: 29x29, 4 px each
    const int scale = 4, x0 = (display.width() - qr.size * scale) / 2, y0 = CONTENT_TOP + 4;
    for (uint8_t y = 0; y < qr.size; y++)
      for (uint8_t x = 0; x < qr.size; x++)
        if (qrcode_getModule(&qr, x, y)) display.fillRect(x0 + x * scale, y0 + y * scale, scale, scale, BLACK);
  }
  display.setFont(FONT_SMALL);
  drawCenteredLine(phoneShown ? "Phone connected" : "Scan with your camera", 164);
  display.setFont(FONT_TINY);
  char line[40];
  snprintf(line, sizeof line, "%s   %s", phoneSsid(), phonePassword());
  drawCenteredLine(line, 181);
  drawCenteredLine("Hold A to stop", 196);
}

static int viewPages() { return max(1, ((int)lines.size() + LINES - 1) / LINES); }

static void drawViewText() {
  ensureLines();
  const int pages = max(1, ((int)lines.size() + LINES - 1) / LINES);
  if (page >= pages) page = 0;
  String title = viewTitle.length() && linesFor == openId ? viewTitle : String(openId);
  for (const NoteInfo &n : notes)
    if (n.id == openId) title = titleOf(n);
  drawHeader(title.c_str());
  display.setFont(FONT_SMALL);
  for (int i = 0; i < LINES && page * LINES + i < (int)lines.size(); i++) {
    display.setCursor(MARGIN, CONTENT_TOP + 18 + i * LINE_H);
    display.print(lines[page * LINES + i]);
  }
}

static void drawView() {
  drawViewText();
  drawHints(viewPages() > 1 ? "more" : "", hasWav ? "play" : "", hasCard() ? "phone" : "");
}

static void drawConfirm() {
  bool pushed = false;
  String title = openId;
  for (const NoteInfo &n : notes)
    if (n.id == openId) title = titleOf(n), pushed = n.pushed;
  drawListRows();
  drawSheet("Delete this note?", title.c_str(), pushed ? "GitHub copy stays." : "Can't be undone.");
  drawHints("keep", "delete", "");
}

// --- recording ---

static void record() {
  if (const char *low = batteryTooLow()) {  // a brown-out while writing the recording could damage the card
    launcherToast(low);
    return;
  }
  if (!jobCanTake()) {  // the last note is still in memory (no card) or one is already queued
    launcherToast("Still sending a note");
    return;
  }
  card = -1;  // look again: a card may have gone in since
  const bool onCard = hasCard();  // mounted here, on the screen's side, before the job uses it
  int16_t *samples;
  size_t count;
  if (const char *err = recordWhileHeld(MAX_SECONDS, samples, count)) {
    message = err;
    return;
  }
  message = "";
  listed = false;
  if (!jobStartNote(storeNewId(), samples, count, onCard)) jobGiveBuffer(samples);  // the buffer is the job's now; this returns at once
}

static void syncWaiting() {
  card = -1;
  listed = false;
  ensureList();
  if (!hasCard()) message = "No SD card: nothing waiting";
  else if (jobBusy()) launcherToast("Already sending");
  else if (!waiting()) message = "Nothing waiting";
  else message = "", jobStartSweep();
}

// --- app interface ---

static void onEnter() {
  screen = MAIN;
  card = -1;
  listed = false;
  linesFor = "";
  message = "";
}

static Redraw onButton(Event e) {
  ensureList();
  switch (screen) {
    case MAIN:
      if (e == Event::BLong) {
        record();
        return Redraw::Partial;  // quick: back to the screen ~0.5 s after letting go (the job does the rest)
      }
      if (e == Event::BShort) {
        syncWaiting();
        return Redraw::Partial;
      }
      if (e == Event::AShort) {
        if (!hasCard()) {
          launcherToast("No SD card here");
          return Redraw::Partial;
        }
        screen = LIST;
        cursor = 0;  // the newest note
        return Redraw::Partial;
      }
      return Redraw::None;
    case PHONE:
      phoneActivity();  // any press keeps the hotspot up
      return Redraw::None;
    case LIST:
      if (notes.empty()) return Redraw::None;
      if (e == Event::AShort) cursor = (cursor + 1) % notes.size();
      else if (e == Event::BShort) {
        strlcpy(openId, notes[cursor].id.c_str(), sizeof openId);
        page = 0;
        screen = VIEW;
      } else if (e == Event::BLong) {
        strlcpy(openId, notes[cursor].id.c_str(), sizeof openId);
        screen = CONFIRM;
      } else
        return Redraw::None;
      return Redraw::Partial;
    case VIEW:
      if (e == Event::AShort) {
        page++;  // wraps in drawView
        return Redraw::Partial;
      }
      if (e == Event::BShort && hasWav) {
        if (const char *err = playNote(openId)) launcherToast(err);
        return Redraw::Partial;
      }
      if (e == Event::BLong && hasCard()) {  // read it (and the other notes) on a phone
        if (!phoneStart()) {
          launcherToast("Sending a note: try soon");
          return Redraw::Partial;
        }
        screen = PHONE;
        return Redraw::Partial;
      }
      return Redraw::None;  // deleting is in the list (hold B there)
    case CONFIRM:
      if (e == Event::BShort) {
        if (jobBusy() && strcmp(openId, jobStatus().noteId) == 0) {
          launcherToast("It's sending: wait");
        } else if (devDryRun()) {
          launcherToast("Dry run: not deleted");
        } else {
          storeDelete(openId);
          launcherToast("Deleted");
          listed = false;
          ensureList();
        }
        screen = LIST;
        return Redraw::Partial;
      }
      if (e == Event::AShort) {
        screen = LIST;
        return Redraw::Partial;
      }
      return Redraw::None;
  }
  return Redraw::None;
}

static Redraw onBack() {
  switch (screen) {
    case MAIN: return Redraw::Exit;
    case LIST: screen = MAIN; break;
    case VIEW: screen = hasCard() ? LIST : MAIN; break;
    case CONFIRM: screen = LIST; break;
    case PHONE:
      phoneStop();
      screen = VIEW;
      break;
  }
  return Redraw::Partial;
}

static void draw() {
  if (screen == PHONE && !phoneOn()) screen = VIEW;  // e.g. after a restart
  if (screen == PHONE) drawPhone();
  else if (screen == LIST) drawList();
  else if (screen == VIEW) drawView();
  else if (screen == CONFIRM) drawConfirm();
  else drawMain();
}

// Redraws as the background job moves on, and takes in its result (also one that arrived while away).
static Redraw tick() {
  if (screen == PHONE) {
    phonePoll();
    if (!phoneOn()) {  // stopped itself: nobody used it for a while
      screen = VIEW;
      return Redraw::Partial;
    }
    // "Phone connected", once its first few requests are answered: a screen refresh holds everything up for
    // ~0.5 s, and the phone is waiting on those answers to open the page.
    return !phoneShown && phoneConnectedMs() > 3000 ? Redraw::Partial : Redraw::None;
  }
  const JobStatus js = jobStatus();
  if (js.gen == seenGen) return Redraw::None;
  seenGen = js.gen;
  if (!jobBusy()) {
    if (*js.result) message = js.result;
    listed = false;
    linesFor = "";
    String md, title;
    if (jobTakeShown(md, title)) {  // no card, no GitHub: show the note now, it isn't kept
      lines.clear();
      wrap(readable(md), lines);
      strlcpy(openId, "shown", sizeof openId);
      linesFor = openId;
      viewTitle = ascii(title);
      page = 0;
      screen = VIEW;
      return Redraw::Full;
    }
  }
  return screen == MAIN || screen == LIST ? Redraw::Partial : Redraw::None;
}

static void onExit() {
  phoneStop();
  notes.clear();
  lines.clear();
  listed = false;
  linesFor = "";
}

static void status(char *out, size_t len) {
  static bool looked;  // after a restart, count the notes once so the line is there before the first visit
  if (noteCount < 0 && !looked) {
    looked = true;
    ensureList();
  }
  if (jobBusy()) snprintf(out, len, "Sending: %s", stepName(jobStatus().step));
  else if (noteCount < 0) snprintf(out, len, "Hold B to record");
  else if (waitingCount) snprintf(out, len, "%d note%s, %d waiting", noteCount, noteCount == 1 ? "" : "s", waitingCount);
  else snprintf(out, len, noteCount == 1 ? "1 note" : "%d notes", noteCount);
}

// Notes tries sending by itself every 10 min while awake, and on waking if 10 min have passed since the last try.
// Without the clock set: once per wake, then every 10 min. Any send counts as a try (a note just recorded too).
static uint32_t lastTryMs;
static bool triedThisWake;

static void tried() {
  if (clockValid()) lastAutoSync = time(nullptr);
  lastTryMs = millis();
  triedThisWake = true;
}

static bool autoSyncDue() {
  const uint32_t every = devAutoSync() ? devAutoSync() : AUTO_SYNC_S;
  if (triedThisWake) return millis() - lastTryMs >= every * 1000;
  if (!clockValid() || !lastAutoSync) return true;
  const time_t now = time(nullptr);
  return now < lastAutoSync || now - lastAutoSync >= (time_t)every;
}

// Every loop, whatever is on screen: recordings that couldn't go (no WiFi) are sent once it can be reached.
static bool background() {
  static uint32_t seen;
  const JobStatus js = jobStatus();
  if (js.gen != seen && !jobBusy()) {
    seen = js.gen;
    if (*js.result) {  // a note's work finished (not a reminder's): it was a try, and what's waiting may have changed
      tried();
      listed = false;
      ensureList();
    }
  }
  if (waitingCount <= 0 || jobBusy() || netClaimed() || !credWifiAny() || !credHas("openai_key")) return false;
  if (batteryTooLow() || !autoSyncDue()) return false;
  tried();
  jobStartSweep(true);
  return false;
}

extern const App notesApp = {"Notes", ICON_NOTES, onEnter, onButton, draw,  onExit,
                             onBack,  status,     tick,    false,    background};
