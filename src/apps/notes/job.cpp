#include "job.h"
#include "cloud.h"
#include "store.h"
#include <esp_heap_caps.h>
#include "../../core/audio.h"
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
  int16_t *samples;  // from jobTakeBuffer, owned by the job; given back once it's on the card (or sent)
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
static const char *const NOTHING_KEPT = "Heard nothing: kept";  // finish(): nothing heard, but the recording stays
static const char *const FULL_PUSHED = "Card full: on GitHub", *const FULL_SHOWN = "Card full: shown once";

// Transcribe (from the audio parts), tidy up, save and push one note. WiFi must be up.
// Returns nullptr or the reason it stopped; `title` gets the note's title.
static const char *finish(const String &id, const NetPart *audio, int nParts, bool onCard, String &title) {
  NoteText note;
  publish(JobStep::Transcribing);
  if (const char *err = cloudTranscribe(audio, nParts, note.transcript)) return err;
  if (!note.transcript.length()) {
    // Whisper can be wrong about quiet or noisy speech, so a recording on the card is never deleted for it: it
    // stays as a note that says so (and counts as on GitHub, so it isn't sent or transcribed again).
    note.title = "Heard nothing";
    note.body = "Nothing was heard in this recording. The audio is kept.";
    if (onCard && storeSaveNote(id, noteMarkdown(note, id, localIso(id)))) {
      storeMarkPushed(id, "");
      title = note.title;
      return NOTHING_KEPT;
    }
    return NO_SPEECH;
  }
  publish(JobStep::Tidying);
  const char *cleanupErr = cloudCleanup(note, localIso(id));  // on failure the raw transcript is kept
  title = note.title;
  const String md = noteMarkdown(note, id, localIso(id));
  // A full card doesn't lose a note that's already transcribed (and paid for): it still goes to GitHub, and the
  // screen shows it once.
  const bool saved = onCard && storeSaveNote(id, md);
  if (!saved && (onCard || !githubOn())) {  // nowhere else to keep it: shown once (read after busy goes false)
    shownMd = md;
    shownTitle = title;
    hasShown = true;
  }
  if (githubOn()) {
    publish(JobStep::Pushing);
    String path;
    if (const char *err = cloudPush(fileName(title, id), md, path)) return err;
    if (saved) storeMarkPushed(id, path);
  }
  if (onCard && !saved) return githubOn() ? FULL_PUSHED : FULL_SHOWN;
  return cleanupErr;
}

// True for an error that is about one note (the service refused this recording or file), so the others can go on.
// Anything else (no WiFi, a bad key, no credit, no key set) would fail the same way for every note.
static bool aboutThisNote(const char *err) {
  return strstr(err, "error 400") || strstr(err, "error 413") || strstr(err, "error 422") ||
         strstr(err, "bad reply") || strstr(err, "name taken") || strstr(err, "Couldn't read");
}

// Transcribes waiting recordings and pushes notes that aren't on GitHub yet (WiFi is up). Skips `skip`.
// A note that can't be done is passed over (it stays waiting) and the rest carry on; the first such reason is
// returned at the end, so the screen still says something went wrong.
static const char *sweep(const char *skip, int &done, int &stuck) {
  static char firstErr[48];
  firstErr[0] = 0;
  for (const NoteInfo &n : storeList()) {
    if (n.id == skip) continue;
    if (!devSweepAllows(n.id)) continue;
    String title = n.title;
    const char *err = nullptr;
    setNote(n.id.c_str());
    if (!n.text) {
      fs::File f = storageOpen(storeWavPath(n.id).c_str());
      if (!f) continue;
      const NetPart audio[] = {NetPart::rest(f)};
      err = finish(n.id, audio, 1, true, title);
      f.close();
      if (err == NOTHING_KEPT) continue;  // kept as a note; never sent again
      if (err == NO_SPEECH) {  // nothing said and it couldn't be kept: drop it, so it isn't retried
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
    if (err && aboutThisNote(err)) {
      if (!firstErr[0]) snprintf(firstErr, sizeof firstErr, "%s", err);
      stuck++;
      continue;
    }
    if (err) return err;
    done++;
  }
  return firstErr[0] ? firstErr : nullptr;
}

static void runNote(Item &it) {
  char result[48];
  setNote(it.id);
  publish(JobStep::Saving, "");
  const bool saved = it.onCard && storeSaveWav(it.id, it.samples, it.count);
  if (saved) {  // it's on the card: the memory can go, and a new recording can start
    jobGiveBuffer(it.samples);
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
    int done = 0, stuck = 0;
    // Older notes still waiting go in the same WiFi session (never with the test build's fake cloud: it would
    // give real notes the canned text).
    if (!err && it.onCard && (!devFakeCloud() || devSweepOn())) sweep(it.id, done, stuck);
  }
  jobGiveBuffer(it.samples);
  it.samples = nullptr;
  if (err == NOTHING_KEPT) snprintf(result, sizeof result, "%s", err);
  else if (err == NO_SPEECH) {  // like a press too short to be a note: nothing is kept
    if (saved) storeDelete(it.id);
    snprintf(result, sizeof result, "Heard nothing: not kept");
  } else if (!err) snprintf(result, sizeof result, "Saved: %s", title.c_str());
  else if (err == FULL_PUSHED || err == FULL_SHOWN) snprintf(result, sizeof result, "%s", err);
  else if (saved) snprintf(result, sizeof result, "%s (kept)", err);
  else snprintf(result, sizeof result, "%s: not kept", err);
  publish(JobStep::Idle, result);
}

static void runSweep() {
  char result[48];
  publish(JobStep::Connecting, "");
  const char *err = netConnect();
  int done = 0, stuck = 0;
  if (!err && devFakeCloud() && !devSweepOn()) err = "fake cloud: sync skipped";
  if (!err) err = sweep("", done, stuck);
  if (stuck && done) snprintf(result, sizeof result, "%d synced, %d stuck", done, stuck);  // (the reason won't fit)
  else if (err) snprintf(result, sizeof result, done ? "%s (%d done)" : "%s", err, done);
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

static int16_t *pool;
static size_t poolSamples;
static volatile bool poolBusy;

int16_t *jobTakeBuffer(size_t want, size_t &got) {
  if (poolBusy) return nullptr;
  if (!pool) {
    pool = (int16_t *)ps_malloc(want * sizeof(int16_t));
    poolSamples = want;
    if (!pool) {  // memory already split up: take the biggest piece left, if it holds a useful recording (1 min)
      const size_t block = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
      const size_t fit = block > 4096 ? (block - 4096) / sizeof(int16_t) : 0;
      if (fit >= 60 * AUDIO_RATE) pool = (int16_t *)ps_malloc(fit * sizeof(int16_t)), poolSamples = fit;
    }
    if (!pool) return nullptr;
  }
  poolBusy = true;
  got = poolSamples;
  return pool;
}

void jobGiveBuffer(int16_t *buffer) {
  if (buffer == pool) poolBusy = false;
  else free(buffer);
}

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
