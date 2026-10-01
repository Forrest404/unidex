#include "store.h"
#include <algorithm>
#include "../../core/audio.h"
#include "../../core/clock.h"
#include "../../core/storage.h"

static const char *DIR = "/notes";

static String path(const String &id, const char *ext) { return String(DIR) + "/" + id + ext; }

bool storeReady() { return storageCardMount(); }

String storeNewId() {
  if (clockValid()) {
    time_t t = time(nullptr);
    struct tm now;
    localtime_r(&t, &now);
    char id[20];
    strftime(id, sizeof id, "%Y%m%d-%H%M%S", &now);
    String s = id;
    for (char c = 'b'; storageExists(path(s, ".wav").c_str()) && c <= 'z'; c++) s = String(id) + c;  // same second
    return s;
  }
  const int32_t n = storageGetInt("note_seq", 0) + 1;  // no clock: numbered, still sorts in order
  storagePutInt("note_seq", n);
  char id[20];
  snprintf(id, sizeof id, "note-%05ld", (long)n);
  return id;
}

void storeWavHeader(uint8_t *h, uint32_t dataBytes) {
  const uint32_t rate = AUDIO_RATE, byteRate = AUDIO_RATE * 2, riff = dataBytes + 36, fmtLen = 16;
  const uint16_t pcm = 1, mono = 1, align = 2, bits = 16;
  memcpy(h, "RIFF", 4);
  memcpy(h + 4, &riff, 4);
  memcpy(h + 8, "WAVEfmt ", 8);
  memcpy(h + 16, &fmtLen, 4);
  memcpy(h + 20, &pcm, 2);
  memcpy(h + 22, &mono, 2);
  memcpy(h + 24, &rate, 4);
  memcpy(h + 28, &byteRate, 4);
  memcpy(h + 32, &align, 2);
  memcpy(h + 34, &bits, 2);
  memcpy(h + 36, "data", 4);
  memcpy(h + 40, &dataBytes, 4);
}

bool storeSaveWav(const String &id, const int16_t *samples, size_t count) {
  const String p = path(id, ".wav"), tmp = path(id, ".tmp");
  fs::File f = storageOpen(tmp.c_str(), "w");
  if (!f) return false;
  uint8_t header[44];
  storeWavHeader(header, count * 2);
  bool ok = f.write(header, sizeof header) == sizeof header;
  ok = ok && f.write((const uint8_t *)samples, count * 2) == count * 2;  // one write: the card does the buffering
  f.close();
  return ok && storageRename(tmp.c_str(), p.c_str());
}

String storeWavPath(const String &id) { return path(id, ".wav"); }

bool storeSaveNote(const String &id, const String &markdown) {
  const String tmp = path(id, ".tmp");
  fs::File f = storageOpen(tmp.c_str(), "w");
  if (!f) return false;
  const bool ok = f.print(markdown) == markdown.length();
  f.close();
  return ok && storageRename(tmp.c_str(), path(id, ".md").c_str());
}

String storeReadNote(const String &id) {
  const String p = path(id, ".md");
  if (!storageExists(p.c_str())) return "";  // opening a missing file logs an error onto the USB line
  fs::File f = storageOpen(p.c_str());
  return f ? f.readString() : String();
}

void storeMarkPushed(const String &id, const String &repoPath) {
  fs::File f = storageOpen(path(id, ".gh").c_str(), "w");
  if (f) f.print(repoPath);
}

void storeDelete(const String &id) {
  for (const char *ext : {".wav", ".md", ".gh", ".tmp"}) storageRemove(path(id, ext).c_str());
}

String storeTitleOf(const String &md) {
  const int at = md.indexOf("\ntitle: \"");
  if (!md.startsWith("---") || at < 0) return "";
  const int from = at + 9, to = md.indexOf("\"\n", from);
  String t = md.substring(from, to < 0 ? from : to);
  t.replace("\\\"", "\"");
  t.replace("\\\\", "\\");
  return t;
}

// Reads just enough of a note to find its title (the front matter is at the top).
static String readTitle(const String &id) {
  fs::File f = storageOpen(path(id, ".md").c_str());
  if (!f) return "";
  char buf[256];
  const size_t n = f.readBytes(buf, sizeof buf - 1);
  buf[n] = 0;
  return storeTitleOf(buf);
}

std::vector<NoteInfo> storeList() {
  std::vector<NoteInfo> notes;
  if (!storageExists(DIR)) return notes;  // opening a missing folder logs an error onto the USB line
  fs::File dir = storageOpen(DIR);
  if (!dir || !dir.isDirectory()) return notes;
  std::vector<String> names;
  for (fs::File f = dir.openNextFile(); f; f = dir.openNextFile()) {
    String n = f.name();
    if (n.startsWith(".") || n.startsWith("._")) continue;  // macOS litter
    names.push_back(n);
  }
  auto has = [&](const String &n) { return std::find(names.begin(), names.end(), n) != names.end(); };
  for (const String &n : names) {
    const bool wav = n.endsWith(".wav"), md = n.endsWith(".md");
    if (!wav && !md) continue;
    const String id = n.substring(0, n.lastIndexOf('.'));
    if (wav && has(id + ".md")) continue;  // listed once, from its .md
    NoteInfo info;
    info.id = id;
    info.text = md;
    info.pushed = has(id + ".gh");
    info.title = md ? readTitle(id) : String();
    notes.push_back(info);
  }
  std::sort(notes.begin(), notes.end(), [](const NoteInfo &a, const NoteInfo &b) { return a.id > b.id; });
  return notes;
}
