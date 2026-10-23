#include "waiting.h"
#include <algorithm>
#include <esp_random.h>
#include "reminders_logic.h"
#include "../notes/store.h"
#include "../../core/clock.h"
#include "../../core/storage.h"

static const char *const DIR = "/reminders";

static String path(const String &id, const char *ext) { return String(DIR) + "/" + id + ext; }

String waitingNewId() {
  if (!clockValid()) {
    char id[12];
    snprintf(id, sizeof id, "x%08lx", (unsigned long)esp_random());
    return id;
  }
  const struct tm t = clockLocal();
  char id[20];
  strftime(id, sizeof id, "%Y%m%d-%H%M%S", &t);
  String out = id;
  for (char c = 'b'; c <= 'z' && storageExists(path(out, ".wav").c_str()); c++) out = String(id) + c;  // same second
  return out;
}

bool waitingSave(const String &id, const int16_t *samples, size_t count) {
  const String tmp = path(id, ".tmp");
  fs::File f = storageOpen(tmp.c_str(), "w");
  if (!f) return false;
  uint8_t header[44];
  storeWavHeader(header, count * 2);
  bool ok = f.write(header, sizeof header) == sizeof header;
  ok = ok && f.write((const uint8_t *)samples, count * 2) == count * 2;  // one write: the card does the buffering
  f.close();
  if (!ok) storageRemove(tmp.c_str());
  return ok && storageReplace(tmp.c_str(), path(id, ".wav").c_str(), sizeof header + count * 2);
}

String waitingWavPath(const String &id) { return path(id, ".wav"); }

static std::vector<String> idsWith(const char *ext) {
  std::vector<String> ids;
  if (!storageExists(DIR)) return ids;  // opening a missing folder logs an error onto the USB line
  fs::File dir = storageOpen(DIR);
  if (!dir || !dir.isDirectory()) return ids;
  for (fs::File f = dir.openNextFile(); f; f = dir.openNextFile()) {
    const String n = f.name();
    if (!n.startsWith(".") && n.endsWith(ext)) ids.push_back(n.substring(0, n.length() - strlen(ext)));
  }
  std::sort(ids.begin(), ids.end(), [](const String &a, const String &b) { return a < b; });
  return ids;
}

std::vector<String> waitingRecordings() { return idsWith(".wav"); }

bool waitingSetWords(const String &id, const String &words) {
  const String tmp = path(id, ".tmp");
  fs::File f = storageOpen(tmp.c_str(), "w");
  if (!f) return false;
  const bool ok = f.print(words) == words.length();
  f.close();
  if (!ok) storageRemove(tmp.c_str());
  if (!ok || !storageReplace(tmp.c_str(), path(id, ".txt").c_str(), words.length())) return false;
  storageRemove(path(id, ".wav").c_str());  // the words are safe: the recording isn't needed any more
  return true;
}

std::vector<String> waitingWords() { return idsWith(".txt"); }

String waitingReadWords(const String &id) {
  fs::File f = storageOpen(path(id, ".txt").c_str());
  String words, line;
  while (f && storageReadLine(f, line, 2000)) words += (words.length() ? " " : "") + line;  // one sentence or a few
  return words;
}

void waitingRemoveWords(const String &id) { storageRemove(path(id, ".txt").c_str()); }

long waitingSaidAt(const String &id) {
  int y, mo, d, h, mi;
  if (id.length() < 15 || sscanf(id.c_str(), "%4d%2d%2d-%2d%2d", &y, &mo, &d, &h, &mi) != 5) return -1;
  return (long)reminders::daysFromCivil(y, mo, d) * 1440 + h * 60 + mi;
}
