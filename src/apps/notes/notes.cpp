// Notes: hold B to record a voice note, let go to stop. It's transcribed (Whisper), tidied up (OpenAI or
// Claude), kept on the SD card and, if switched on, pushed to GitHub as Markdown (for Obsidian). Set up
// from the website's Notes page. Without a card it still works online, but nothing is kept on the device.
// The online steps run in the background (job.h): letting go of B returns to the screen at once.
// Main: hold B = record, B = sync waiting notes, A = list. List: A = next, B = open, hold B = delete; its
// first row, "Open on phone", starts the hotspot (phone.h) and shows its QR code until hold A.
// Note: A = next page, hold B = delete. Hold A = back everywhere.
#include <vector>
#include <qrcode.h>
#include "job.h"
#include "phone.h"
#include "store.h"
#include "../../core/app.h"
#include "../../core/audio.h"
#include "../../core/credentials.h"
#include "../../core/devtools.h"
#include "../../core/display.h"
#include "../../core/input.h"
#include "../../core/launcher.h"
#include "../../core/power.h"
#include "../../core/storage.h"
#include "../../core/theme.h"

static const uint32_t MAX_SECONDS = 180, MIN_MS = 600;  // 3 min = 5.8 MB of PSRAM
static const int ROWS = 5, ROW_H = 26, LINES = 7, LINE_H = 19;

enum Screen : uint8_t { MAIN, LIST, VIEW, CONFIRM, PHONE };
RTC_DATA_ATTR static uint8_t screen, cursor, page, confirmFrom;
RTC_DATA_ATTR static char openId[24];  // the note on screen in VIEW / CONFIRM
RTC_DATA_ATTR static int16_t noteCount = -1, waitingCount;  // for the home screen (it can't read the card)

// RAM caches: lost in deep sleep, rebuilt on first use after a wake.
static std::vector<NoteInfo> notes;
static bool listed;
static std::vector<String> lines;  // the open note, wrapped
static String linesFor, viewTitle;  // viewTitle: a note shown but not kept (no card, no GitHub)
static String message;   // one line under the main screen's prompt: the last result or a problem
static uint32_t seenGen;  // the job status last taken in (results arriving while away show on return)
static uint32_t phoneShown;  // pages the phone had loaded when the screen was last drawn
static uint32_t recordMs;
static int recordLevel;  // 0-100

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
  if (cursor > notes.size()) cursor = 0;  // row 0 is "Open on phone", the notes follow
  const bool gh = githubOn();
  waitingCount = 0;
  for (const NoteInfo &i : notes) waitingCount += !i.text || (gh && !i.pushed);
  noteCount = notes.size();
}

static int waiting() { return waitingCount; }

// The display font is ASCII: fold the usual UTF-8 punctuation from transcripts, drop the rest.
static String ascii(const String &s) {
  String out;
  for (size_t i = 0; i < s.length(); i++) {
    const uint8_t c = s[i];
    if (c < 0x80) {
      out += (char)c;
      continue;
    }
    if (c == 0xE2 && i + 2 < s.length()) {  // U+2000 block: quotes, dashes, ellipsis
      const uint8_t b = s[i + 2];
      if (b == 0x98 || b == 0x99) out += '\'';
      else if (b == 0x9C || b == 0x9D) out += '"';
      else if (b == 0x93 || b == 0x94) out += '-';
      else if (b == 0xA6) out += "...";
      i += 2;
      continue;
    }
    if (c == 0xC3 && i + 1 < s.length()) {  // Latin-1 letters lose their accents
      static const char *const FOLD = "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYTsaaaaaaaceeeeiiiidnooooo/ouuuuyty";
      const uint8_t b = s[i + 1];
      if (b >= 0x80 && b < 0xC0) out += FOLD[b - 0x80];
      i += 1;
      continue;
    }
    while (i + 1 < s.length() && (s[i + 1] & 0xC0) == 0x80) i++;  // skip the rest of anything else
    out += '?';
  }
  return out;
}

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
  else wrap("Not transcribed yet. Press B on the Notes screen to sync it when WiFi is set up.", lines);
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
    if (!line.length() && !credHas("wifi_ssid")) line = "Set up WiFi on the website";
    else if (!line.length() && !credHas("openai_key")) line = "Add an OpenAI key on the website";
    else if (!line.length() && !hasCard()) line = githubOn() ? "No SD card: GitHub only" : "No SD card: nothing is kept";
    else if (!line.length() && waiting()) line = String(waiting()) + " waiting: B to sync";
    else if (!line.length()) line = "to record a note";
    drawCentered(fitText(line.c_str(), display.width() - 2 * MARGIN).c_str(), 128);
  }
  drawHints(hasCard() ? "list" : "", waiting() && !jobBusy() ? "sync" : "", jobCanTake() ? "record" : "");
}

static void drawRecording() {
  char time[12];
  const uint32_t s = recordMs / 1000;
  snprintf(time, sizeof time, "%lu:%02lu", (unsigned long)(s / 60), (unsigned long)(s % 60));
  drawHeader("Recording", time);
  display.setFont(FONT_LARGE);
  drawCentered(time, 66);
  // Level bar: how loud the last second was.
  const int16_t barW = display.width() - 4 * MARGIN, x = 2 * MARGIN, y = 92;
  display.drawRect(x, y, barW, 12, BLACK);
  display.fillRect(x + 2, y + 2, (barW - 4) * recordLevel / 100, 8, BLACK);
  display.setFont(FONT_SMALL);
  drawCentered(recordMs / 1000 + 10 >= MAX_SECONDS ? "nearly at the limit" : "let go to stop", 132);
  drawHints("", "", "");
}

static void drawListRows() {
  ensureList();
  drawHeader("Notes");
  display.setFont(FONT_SMALL);
  const int first = cursor / ROWS * ROWS;
  for (int r = 0; r < ROWS && first + r <= (int)notes.size(); r++) {
    const int16_t top = CONTENT_TOP + 4 + r * ROW_H, baseline = top + 17;
    const bool sel = first + r == cursor;
    if (sel) display.fillRect(MARGIN - 4, top, display.width() - 2 * (MARGIN - 4), ROW_H - 2, BLACK);
    display.setTextColor(sel ? WHITE : BLACK);
    if (first + r == 0) {
      display.setCursor(MARGIN, baseline);
      display.print("Open on phone");
      display.setTextColor(BLACK);
      continue;
    }
    const NoteInfo &n = notes[first + r - 1];
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
  drawHints(notes.size() ? "next" : "", "open", cursor ? "delete" : "");
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
  drawHints(viewPages() > 1 ? "more" : "", "", hasCard() ? "delete" : "");
}

static void drawConfirm() {
  bool pushed = false;
  String title = openId;
  for (const NoteInfo &n : notes)
    if (n.id == openId) title = titleOf(n), pushed = n.pushed;
  if (confirmFrom == VIEW) drawViewText();
  else drawListRows();
  drawSheet("Delete this note?", title.c_str(), pushed ? "GitHub copy stays." : "Can't be undone.");
  drawHints("keep", "delete", "");
}

// --- recording ---

static void record() {
  if (!jobCanTake()) {  // the last note is still in memory (no card) or one is already queued
    launcherToast("Still sending a note");
    return;
  }
  card = -1;  // look again: a card may have gone in since
  const bool onCard = hasCard();  // mounted here, on the screen's side, before the job uses it
  int16_t *samples = (int16_t *)ps_malloc(MAX_SECONDS * AUDIO_RATE * sizeof(int16_t));
  if (!samples || !audioBegin()) {
    free(samples);
    message = samples ? "Microphone not answering" : "Not enough memory";
    return;
  }
  size_t count = 0;
  const size_t limit = MAX_SECONDS * AUDIO_RATE;
  const uint32_t t0 = millis();
  uint32_t shown = 0;
  int peak = 0;
  recordMs = 0;
  recordLevel = 0;
  displayFrame(drawRecording);
  while (inputBHeld() && count < limit) {
    const size_t n = audioRead(samples + count, min((size_t)1024, limit - count));
    for (size_t i = 0; i < n; i++) peak = max(peak, abs((int)samples[count + i]));
    count += n;
    recordMs = millis() - t0;
    if (recordMs - shown >= 1000) {  // the refresh blocks ~0.4 s; the 1 s DMA buffer keeps recording
      shown = recordMs;
      recordLevel = min(100, peak * 300 / 32767);  // x3: speech rarely gets near full scale
      peak = 0;
      displayFrame(drawRecording);
    }
  }
  audioEnd();
  powerActivity();
  if (recordMs < MIN_MS) {
    free(samples);
    message = "Too short: hold B longer";
    return;
  }
  message = "";
  listed = false;
  jobStartNote(storeNewId(), samples, count, onCard);  // the buffer is the job's now; this returns at once
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
        cursor = notes.size() ? 1 : 0;  // the newest note ("Open on phone" is the row above it)
        return Redraw::Partial;
      }
      return Redraw::None;
    case PHONE:
      phoneActivity();  // any press keeps the hotspot up
      return Redraw::None;
    case LIST:
      if (e == Event::AShort && notes.size()) cursor = (cursor + 1) % (notes.size() + 1);
      else if (e == Event::BShort && cursor == 0) {
        if (!phoneStart()) {
          launcherToast("Sending a note: try soon");
          return Redraw::Partial;
        }
        screen = PHONE;
      } else if (e == Event::BShort) {
        strlcpy(openId, notes[cursor - 1].id.c_str(), sizeof openId);
        page = 0;
        screen = VIEW;
      } else if (e == Event::BLong && cursor) {
        strlcpy(openId, notes[cursor - 1].id.c_str(), sizeof openId);
        confirmFrom = LIST;
        screen = CONFIRM;
      } else
        return Redraw::None;
      return Redraw::Partial;
    case VIEW:
      if (e == Event::AShort) {
        page++;  // wraps in drawView
        return Redraw::Partial;
      }
      if (e == Event::BLong && hasCard()) {
        confirmFrom = VIEW;
        screen = CONFIRM;
        return Redraw::Partial;
      }
      return Redraw::None;
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
        screen = confirmFrom;
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
    case CONFIRM: screen = confirmFrom; break;
    case PHONE:
      phoneStop();
      screen = LIST;
      break;
  }
  return Redraw::Partial;
}

static void draw() {
  if (screen == PHONE && !phoneOn()) screen = LIST;  // e.g. after a restart
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
      screen = LIST;
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

extern const App notesApp = {"Notes", ICON_NOTES, onEnter, onButton, draw, onExit, onBack, status, tick};
