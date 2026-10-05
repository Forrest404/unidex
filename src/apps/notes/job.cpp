#include "job.h"
#include "cloud.h"
#include "store.h"
#include "../../core/clock.h"
#include "../../core/credentials.h"
#include "../../core/devtools.h"
#include "../../core/net.h"
#include "../../core/power.h"
#include "../../core/storage.h"

// One piece of work: a fresh recording (id + samples), or a sweep of everything waiting.
struct Item {
  bool note;
  char id[24];
  int16_t *samples;  // ps_malloc'd, owned by the job; freed once it's on the card (or sent)
  size_t count;
  bool onCard;
};

static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
static JobStatus status;  // read and written under mux
static Item running, waitingItem;
static bool hasWaiting;   // a second recording, queued behind the running one
static volatile bool busy;
static String shownMd, shownTitle;  // no card, no GitHub: the finished note, for the screen to show once
static volatile bool hasShown;

static TaskHandle_t task;
static StaticTask_t taskBuffer;
static StackType_t stack[16384 / sizeof(StackType_t)];  // TLS + JSON need room; static, so it can't fail later

static void publish(JobStep step, const char *result = nullptr) {
  portENTER_CRITICAL(&mux);
  status.step = step;
  if (result) strlcpy(status.result, result, sizeof status.result);
  status.gen++;
  portEXIT_CRITICAL(&mux);
}

static void setNote(const char *id) {
  portENTER_CRITICAL(&mux);
  strlcpy(status.noteId, id, sizeof status.noteId);
  portEXIT_CRITICAL(&mux);
}

static String localIso(const String &id) {  // the note's own time, from its id (YYYYMMDD-HHMMSS)
  if (id.length() >= 15 && isdigit((unsigned char)id[0]))
    return id.substring(0, 4) + "-" + id.substring(4, 6) + "-" + id.substring(6, 8) + "T" + id.substring(9, 11) + ":" +
           id.substring(11, 13) + ":" + id.substring(13, 15);
  char now[24] = "";
  if (clockValid()) {
    const struct tm tm = clockLocal();
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

static bool githubOn() { return credGet("gh_on") == "1"; }

static const char *const NO_SPEECH = "Heard nothing";  // finish(): a recording with nothing to keep

// Transcribe (from the audio parts), tidy up, save and push one note. WiFi must be up.
// Returns nullptr or the reason it stopped; `title` gets the note's title.
static const char *finish(const String &id, const NetPart *audio, int nParts, bool onCard, String &title) {
  NoteText note;
  publish(JobStep::Transcribing);
  if (const char *err = cloudTranscribe(audio, nParts, note.transcript)) return err;
  if (!note.transcript.length()) return NO_SPEECH;
  publish(JobStep::Tidying);
  const char *cleanupErr = cloudCleanup(note, localIso(id));  // on failure the raw transcript is kept
  title = note.title;
  const String md = noteMarkdown(note, id, localIso(id));
  if (onCard && !storeSaveNote(id, md)) return "Couldn't save to card";
  if (githubOn()) {
    publish(JobStep::Pushing);
    String path;
    if (const char *err = cloudPush(fileName(title, id), md, path)) return err;
    if (onCard) storeMarkPushed(id, path);
  }
  if (!onCard && !githubOn()) {  // nowhere to keep it: the screen shows it once (read after busy goes false)
    shownMd = md;
    shownTitle = title;
    hasShown = true;
  }
  return cleanupErr;
}

// Transcribes waiting recordings and pushes notes that aren't on GitHub yet (WiFi is up). Skips `skip`.
static const char *sweep(const char *skip, int &done) {
  for (const NoteInfo &n : storeList()) {
    if (n.id == skip) continue;
    String title = n.title;
    const char *err = nullptr;
    setNote(n.id.c_str());
    if (!n.text) {
      fs::File f = storageOpen(storeWavPath(n.id).c_str());
      if (!f) continue;
      const NetPart audio[] = {NetPart::rest(f)};
      err = finish(n.id, audio, 1, true, title);
      f.close();
      if (err == NO_SPEECH) {  // nothing said: drop it, so it isn't retried (and doesn't hold up the others)
        storeDelete(n.id);
        continue;
      }
      if (err && storeReadNote(n.id).length()) err = nullptr;  // saved; only the cleanup or push failed
    } else if (githubOn() && !n.pushed) {
      publish(JobStep::Pushing);
      String path;
      err = cloudPush(fileName(n.title, n.id), storeReadNote(n.id), path);
      if (!err) storeMarkPushed(n.id, path);
    } else {
      continue;
    }
    if (err) return err;
    done++;
  }
  return nullptr;
}

static void runNote(Item &it) {
  char result[48];
  setNote(it.id);
  publish(JobStep::Saving, "");
  const bool saved = it.onCard && storeSaveWav(it.id, it.samples, it.count);
  if (saved) {  // it's on the card: the memory can go, and a new recording can start
    free(it.samples);
    it.samples = nullptr;
  }
  publish(JobStep::Connecting);
  const char *err = netConnect();
  String title;
  if (!err) {
    if (it.samples) {  // not on the card: send it from memory
      uint8_t header[44];
      storeWavHeader(header, it.count * 2);
      const NetPart audio[] = {NetPart::bytes(header, sizeof header), NetPart::bytes(it.samples, it.count * 2)};
      err = finish(it.id, audio, 2, it.onCard, title);
    } else {
      fs::File f = storageOpen(storeWavPath(it.id).c_str());
      const NetPart audio[] = {NetPart::rest(f)};
      err = f ? finish(it.id, audio, 1, true, title) : "Couldn't read the card";
    }
    int done = 0;
    // Older notes still waiting go in the same WiFi session (never with the test build's fake cloud: it would
    // give real notes the canned text).
    if (!err && it.onCard && !devFakeCloud()) sweep(it.id, done);
  }
  free(it.samples);
  it.samples = nullptr;
  if (err == NO_SPEECH) {  // like a press too short to be a note: nothing is kept
    if (saved) storeDelete(it.id);
    snprintf(result, sizeof result, "Heard nothing: not kept");
  } else if (!err) snprintf(result, sizeof result, "Saved: %s", title.c_str());
  else if (saved) snprintf(result, sizeof result, "%s (kept)", err);
  else snprintf(result, sizeof result, "%s: not kept", err);
  publish(JobStep::Idle, result);
}

static void runSweep() {
  char result[48];
  publish(JobStep::Connecting, "");
  const char *err = netConnect();
  int done = 0;
  if (!err && devFakeCloud()) err = "fake cloud: sync skipped";
  if (!err) err = sweep("", done);
  if (err) snprintf(result, sizeof result, done ? "%s (%d done)" : "%s", err, done);
  else snprintf(result, sizeof result, done == 1 ? "1 note synced" : "%d notes synced", done);
  publish(JobStep::Idle, result);
}

static void worker(void *) {
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    for (;;) {
      if (running.note) runNote(running);
      else runSweep();
      portENTER_CRITICAL(&mux);
      const bool more = hasWaiting;
      if (more) running = waitingItem, hasWaiting = false;
      portEXIT_CRITICAL(&mux);
      if (!more) break;
    }
    netOff();
    netClaim(false);
    busy = false;
    publish(JobStep::Idle);
    powerRelease();
  }
}

static bool start(const Item &it) {
  if (!task) task = xTaskCreateStaticPinnedToCore(worker, "notes", sizeof stack / sizeof stack[0], nullptr, 1, stack,
                                                  &taskBuffer, 0);  // core 0: the screen and buttons stay on core 1
  if (busy) {
    if (hasWaiting || !it.note) return false;
    portENTER_CRITICAL(&mux);
    waitingItem = it;
    hasWaiting = true;
    portEXIT_CRITICAL(&mux);
    return true;
  }
  running = it;
  busy = true;
  netClaim(true);
  powerHold();  // no sleep (light sleep would drop the WiFi) until the work is done
  publish(it.note ? JobStep::Saving : JobStep::Connecting, "");
  xTaskNotifyGive(task);
  return true;
}

bool jobBusy() { return busy; }

bool jobCanTake() {
  // A second note can queue only once the first is safely on the card (its memory freed), and only one.
  return !busy || (!hasWaiting && running.note && !running.samples && running.onCard);
}

bool jobStartNote(const String &id, int16_t *samples, size_t count, bool onCard) {
  Item it = {true, "", samples, count, onCard};
  strlcpy(it.id, id.c_str(), sizeof it.id);
  return start(it);
}

bool jobStartSweep() {
  Item it = {false, "", nullptr, 0, true};
  return start(it);
}

JobStatus jobStatus() {
  portENTER_CRITICAL(&mux);
  const JobStatus s = status;
  portEXIT_CRITICAL(&mux);
  return s;
}

size_t jobStackLeft() { return task ? uxTaskGetStackHighWaterMark(task) * sizeof(StackType_t) : 0; }

bool jobTakeShown(String &markdown, String &title) {
  if (!hasShown || busy) return false;
  markdown = shownMd;
  title = shownTitle;
  shownMd = shownTitle = "";
  hasShown = false;
  return true;
}
