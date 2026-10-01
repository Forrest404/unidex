// Notes: hold B to record a voice note, let go to stop. It's transcribed (Whisper), tidied up (OpenAI or
// Claude), kept on the SD card and, if switched on, pushed to GitHub as Markdown (for Obsidian). Set up
// from the website's Notes page. Without a card it still works online, but nothing is kept on the device.
// Main: B hold = record, B = sync waiting notes, A = list. List: A = next, B = open, B hold = back.
// Note: A = next page, B = back, B hold = delete (then B = yes, A = no).
#include <vector>
#include "cloud.h"
#include "store.h"
#include "../../core/app.h"
#include "../../core/audio.h"
#include "../../core/clock.h"
#include "../../core/credentials.h"
#include "../../core/display.h"
#include "../../core/input.h"
#include "../../core/power.h"
#include "../../core/storage.h"
#include "../../core/theme.h"

static const uint32_t MAX_SECONDS = 180, MIN_MS = 600;  // 3 min = 5.8 MB of PSRAM
static const int ROWS = 5, ROW_H = 26, LINES = 7, LINE_H = 19;

enum Screen : uint8_t { MAIN, LIST, VIEW, CONFIRM };
RTC_DATA_ATTR static uint8_t screen, cursor, page;
RTC_DATA_ATTR static char openId[24];  // the note on screen in VIEW / CONFIRM

// RAM caches: lost in deep sleep, rebuilt on first use after a wake.
static std::vector<NoteInfo> notes;
static bool listed;
static std::vector<String> lines;  // the open note, wrapped
static String linesFor, viewTitle;  // viewTitle: a note shown but not kept (no card, no GitHub)
static String message;   // one line under the main screen's prompt: the last result or a problem
static String working;   // "Transcribing..." while a step runs
static uint32_t recordMs;
static int recordLevel;  // 0-100

static int8_t card = -1;  // SD card readable: checked once per visit (with no card, each check takes a while)

static bool githubOn() { return credGet("gh_on") == "1"; }

static bool hasCard() {
  if (card < 0) card = storeReady();
  return card;
}

static void ensureList() {
  if (listed) return;
  notes = hasCard() ? storeList() : std::vector<NoteInfo>();
  listed = true;
  if (cursor >= notes.size()) cursor = 0;
}

static int waiting() {
  int n = 0;
  for (const NoteInfo &i : notes) n += !i.text || (githubOn() && !i.pushed);
  return n;
}

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
  const String md = storeReadNote(openId);
  if (md.length()) wrap(readable(md), lines);
  else wrap("Not transcribed yet. Press B on the Notes screen to sync it when WiFi is set up.", lines);
  linesFor = openId;
}

static String titleOf(const NoteInfo &n) {
  if (!n.text) return "Waiting...";
  return n.title.length() ? ascii(n.title) : n.id;
}

// --- drawing ---

static void drawMain() {
  ensureList();
  drawHeader("Notes");
  char count[16];
  if (hasCard()) {
    snprintf(count, sizeof count, "%d note%s", (int)notes.size(), notes.size() == 1 ? "" : "s");
    drawRight(count, 16);
  }
  drawIcon(ICON_NOTES, (display.width() - ICON_SIZE) / 2, CONTENT_TOP + 10, GxEPD_BLACK);
  display.setFont(FONT_LARGE);
  drawCentered(working.length() ? working.c_str() : "Hold B", 100);
  display.setFont(FONT_SMALL);
  String line = message;
  if (working.length()) line = "";
  else if (!line.length() && !credHas("wifi_ssid")) line = "set up WiFi on the website";
  else if (!line.length() && !credHas("openai_key")) line = "add an OpenAI key on the website";
  else if (!line.length() && !hasCard()) line = githubOn() ? "no SD card: GitHub only" : "no SD card: nothing is kept";
  else if (!line.length() && waiting()) line = String(waiting()) + " waiting: B to sync";
  else if (!line.length()) line = "to record a note";
  drawCentered(fitText(line.c_str(), display.width() - 2 * MARGIN).c_str(), 130);
  drawFooter(hasCard() ? "list" : "", working.length() ? "" : "sync");
}

static void drawRecording() {
  drawHeader("Recording");
  char time[12];
  const uint32_t s = recordMs / 1000;
  snprintf(time, sizeof time, "%lu:%02lu", (unsigned long)(s / 60), (unsigned long)(s % 60));
  display.setFont(FONT_LARGE);
  drawCentered(time, 74);
  // Level bar: how loud the last second was.
  const int16_t barW = display.width() - 4 * MARGIN, x = 2 * MARGIN, y = 102;
  display.drawRect(x, y, barW, 12, GxEPD_BLACK);
  display.fillRect(x + 2, y + 2, (barW - 4) * recordLevel / 100, 8, GxEPD_BLACK);
  display.setFont(FONT_SMALL);
  drawCentered(recordMs / 1000 + 10 >= MAX_SECONDS ? "nearly at the limit" : "let go to stop", 140);
  drawFooter("", "");
}

static void drawList() {
  ensureList();
  char title[16];
  snprintf(title, sizeof title, "%d of %d", notes.size() ? cursor + 1 : 0, (int)notes.size());
  drawHeader("Notes");
  drawRight(title, 16);
  display.setFont(FONT_SMALL);
  if (notes.empty()) drawCentered("no notes yet", (CONTENT_TOP + CONTENT_BOTTOM) / 2);
  const int first = cursor / ROWS * ROWS;
  for (int r = 0; r < ROWS && first + r < (int)notes.size(); r++) {
    const NoteInfo &n = notes[first + r];
    const int16_t top = CONTENT_TOP + 4 + r * ROW_H, baseline = top + 17;
    const bool sel = first + r == cursor;
    if (sel) display.fillRect(MARGIN - 4, top, display.width() - 2 * (MARGIN - 4), ROW_H - 2, GxEPD_BLACK);
    display.setTextColor(sel ? GxEPD_WHITE : GxEPD_BLACK);
    const char *tag = !n.text || (githubOn() && !n.pushed) ? "wait" : "";
    drawRight(tag, baseline);
    int16_t x, y;
    uint16_t tagW = 0, h;
    if (*tag) display.getTextBounds(tag, 0, 0, &x, &y, &tagW, &h);
    display.setCursor(MARGIN, baseline);
    display.print(fitText(titleOf(n).c_str(), display.width() - 2 * MARGIN - tagW - 8));
    display.setTextColor(GxEPD_BLACK);
  }
  drawFooter("next", "open");
}

static void drawView() {
  ensureLines();
  const int pages = max(1, ((int)lines.size() + LINES - 1) / LINES);
  if (page >= pages) page = 0;
  String title = viewTitle.length() && linesFor == openId ? viewTitle : String(openId);
  for (const NoteInfo &n : notes)
    if (n.id == openId) title = titleOf(n);
  drawHeader(fitText(title.c_str(), 120).c_str());
  char pos[8];
  snprintf(pos, sizeof pos, "%d/%d", page + 1, pages);
  drawRight(pos, 16);
  display.setFont(FONT_SMALL);
  for (int i = 0; i < LINES && page * LINES + i < (int)lines.size(); i++) {
    display.setCursor(MARGIN, CONTENT_TOP + 18 + i * LINE_H);
    display.print(lines[page * LINES + i]);
  }
  drawFooter(pages > 1 ? "more" : "", "back");
}

static void drawConfirm() {
  drawHeader("Delete note");
  display.setFont(FONT_SMALL);
  drawCentered("Delete this note?", 84);
  drawCentered(githubOn() ? "the GitHub copy stays" : "this can't be undone", 108);
  drawFooter("no", "yes");
}

static void showWorking(const char *what) {
  working = what;
  displayShow(drawMain, false);
}

// --- recording and the cloud steps ---

static String localIso(const String &id) {  // the note's own time, from its id (YYYYMMDD-HHMMSS)
  if (id.length() >= 15 && isdigit((unsigned char)id[0]))
    return id.substring(0, 4) + "-" + id.substring(4, 6) + "-" + id.substring(6, 8) + "T" + id.substring(9, 11) + ":" +
           id.substring(11, 13) + ":" + id.substring(13, 15);
  char now[24] = "";
  if (clockValid()) {
    time_t t = time(nullptr);
    struct tm tm;
    localtime_r(&t, &tm);
    strftime(now, sizeof now, "%Y-%m-%dT%H:%M:%S", &tm);
  }
  return now;
}

static String fileName(const String &title, const String &id) {  // a safe GitHub file name
  String out;
  for (size_t i = 0; i < title.length() && out.length() < 40; i++) {
    const char c = title[i];
    if (isalnum((unsigned char)c) || c == ' ' || c == '-' || c == '_') out += c;
  }
  out.trim();
  return out.length() ? out : id;
}

// Transcribe (from the audio parts), tidy up, save and push one note. WiFi must be up.
// Returns nullptr or the reason it stopped; `title` gets the note's title.
static const char *finish(const String &id, const NetPart *audio, int nParts, String &title) {
  NoteText note;
  showWorking("Transcribing");
  if (const char *err = cloudTranscribe(audio, nParts, note.transcript)) return err;
  if (!note.transcript.length()) return "heard nothing";
  showWorking("Tidying up");
  const char *cleanupErr = cloudCleanup(note, localIso(id));  // on failure the raw transcript is kept
  title = note.title;
  const String md = noteMarkdown(note, id, localIso(id));
  const bool onCard = hasCard();
  if (onCard && !storeSaveNote(id, md)) return "couldn't save to card";
  if (githubOn()) {
    showWorking("Saving to GitHub");
    String path;
    if (const char *err = cloudPush(fileName(title, id), md, path)) return err;
    if (onCard) storeMarkPushed(id, path);
  }
  if (!onCard && !githubOn()) {  // nowhere to keep it: at least show it
    lines.clear();
    wrap(readable(md), lines);
    linesFor = id;
    viewTitle = ascii(title);
    strlcpy(openId, id.c_str(), sizeof openId);
  }
  return cleanupErr;
}

static void record() {
  card = -1;  // look again: a card may have gone in since
  int16_t *samples = (int16_t *)ps_malloc(MAX_SECONDS * AUDIO_RATE * sizeof(int16_t));
  if (!samples || !audioBegin()) {
    free(samples);
    message = samples ? "microphone not answering" : "not enough memory";
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
  listed = false;

  if (recordMs < MIN_MS) {
    free(samples);
    message = "too short: hold B while you talk";
    return;
  }
  const String id = storeNewId();
  const bool onCard = hasCard();
  const bool saved = onCard && storeSaveWav(id, samples, count);
  if (onCard && !saved) message = "couldn't save to card";

  const char *err = netConnect();
  String title;
  if (!err) {
    uint8_t header[44];
    storeWavHeader(header, count * 2);
    const NetPart audio[] = {NetPart::bytes(header, sizeof header), NetPart::bytes(samples, count * 2)};
    err = finish(id, audio, 2, title);
    netOff();
  }
  free(samples);
  working = "";
  listed = false;
  if (!err) message = "saved: " + ascii(title);
  else if (saved) message = String(err) + " (kept)";
  else message = String(err) + ": not kept";
  if (!onCard && !githubOn() && !err) screen = VIEW;  // nowhere to keep it: show it now
  powerActivity();
}

// Transcribes waiting recordings and pushes notes that aren't on GitHub yet, one WiFi session.
static void syncWaiting() {
  card = -1;
  listed = false;
  ensureList();
  if (!hasCard()) {
    message = "no SD card: nothing waiting";
    return;
  }
  if (!waiting()) {
    message = "nothing waiting";
    return;
  }
  showWorking("Connecting");
  const char *err = netConnect();
  int done = 0;
  for (const NoteInfo &n : notes) {
    if (err) break;
    String title = n.title;
    if (!n.text) {
      fs::File f = storageOpen(storeWavPath(n.id).c_str());
      if (!f) continue;
      const NetPart audio[] = {NetPart::rest(f)};
      err = finish(n.id, audio, 1, title);
      if (err && storeReadNote(n.id).length()) err = nullptr;  // saved; only the cleanup or push failed
    } else if (githubOn() && !n.pushed) {
      showWorking("Saving to GitHub");
      String path;
      const String md = storeReadNote(n.id);
      err = cloudPush(fileName(n.title, n.id), md, path);
      if (!err) storeMarkPushed(n.id, path);
    } else {
      continue;
    }
    if (!err) done++;
    powerActivity();
  }
  netOff();
  working = "";
  listed = false;
  ensureList();
  if (err) message = String(err) + (done ? " (" + String(done) + " done)" : "");
  else message = String(done) + " synced";
}

// --- app interface ---

static void onEnter() {
  screen = MAIN;
  card = -1;
  listed = false;
  linesFor = "";
  message = "";
  working = "";
}

static Redraw onButton(Event e) {
  ensureList();
  switch (screen) {
    case MAIN:
      if (e == Event::BLong) {
        message = "";
        record();
        return Redraw::Full;  // clears the recording screen's ghosting
      }
      if (e == Event::BShort) {
        syncWaiting();
        return Redraw::Full;
      }
      if (e == Event::AShort && hasCard()) {
        screen = LIST;
        cursor = 0;
        return Redraw::Full;
      }
      return Redraw::None;
    case LIST:
      if (e == Event::AShort && notes.size()) cursor = (cursor + 1) % notes.size();
      else if (e == Event::BShort && notes.size()) {
        strlcpy(openId, notes[cursor].id.c_str(), sizeof openId);
        page = 0;
        screen = VIEW;
        return Redraw::Full;
      } else if (e == Event::BLong) {
        screen = MAIN;
        message = "";
        return Redraw::Full;
      } else
        return Redraw::None;
      return Redraw::Partial;
    case VIEW:
      if (e == Event::AShort) {
        page++;  // wraps in drawView
        return Redraw::Partial;
      }
      if (e == Event::BLong && hasCard()) {
        screen = CONFIRM;
        return Redraw::Partial;
      }
      screen = hasCard() ? LIST : MAIN;
      return Redraw::Full;
    case CONFIRM:
      if (e == Event::BShort) {
        storeDelete(openId);
        listed = false;
        ensureList();
        screen = LIST;
        return Redraw::Full;
      }
      if (e == Event::AShort) {
        screen = VIEW;
        return Redraw::Partial;
      }
      return Redraw::None;
  }
  return Redraw::None;
}

static void draw() {
  if (screen == LIST) drawList();
  else if (screen == VIEW) drawView();
  else if (screen == CONFIRM) drawConfirm();
  else drawMain();
}

static void onExit() {
  notes.clear();
  lines.clear();
  listed = false;
  linesFor = "";
}

extern const App notesApp = {"Notes", ICON_NOTES, onEnter, onButton, draw, onExit};
