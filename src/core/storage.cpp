#include "storage.h"
#include <LittleFS.h>  // only for the one-time copy from internal flash
#include <Preferences.h>
#include <SD_MMC.h>
#include <nvs_flash.h>
#include "devtools.h"

static const char *NVS_NAMESPACE = "unidex";
static const int SD_CLK = 39, SD_CMD = 41, SD_D0 = 40;  // Waveshare 04_SD_Card example + schematic
static bool cardMounted;

// A power cut between the two renames in storageRename leaves only the old copy, under "<path>.old": put it back.
static void recover(const char *path) {
  if (SD_MMC.exists(path)) return;
  const String old = String(path) + ".old";
  if (SD_MMC.exists(old)) SD_MMC.rename(old, path);
}

fs::File storageOpen(const char *path, const char *mode) {
  if (!storageCardMount()) return fs::File();
  if (*mode == 'r') recover(path);
  if (*mode != 'r') {  // FAT won't create a file in a missing folder (e.g. /badges on a new card)
    String dir = String(path).substring(0, String(path).lastIndexOf('/'));
    if (dir.length() && !SD_MMC.exists(dir)) SD_MMC.mkdir(dir);
  }
  return SD_MMC.open(path, mode);
}

bool storageExists(const char *path) {
  if (!storageCardMount()) return false;
  recover(path);
  return SD_MMC.exists(path);
}

// FAT won't rename onto an existing file. The old copy is moved aside first and removed only once the new one is in
// place, so at every moment one of them is there whole.
bool storageRename(const char *from, const char *to) {
  if (!storageCardMount() || !SD_MMC.exists(from)) return false;
  const String old = String(to) + ".old";
  const bool had = SD_MMC.exists(to);
  if (had) {
    if (SD_MMC.exists(old)) SD_MMC.remove(old);  // checked first: removing a missing file logs an error on USB
    if (!SD_MMC.rename(to, old)) return false;
  }
  if (!SD_MMC.rename(from, to)) {
    if (had) SD_MMC.rename(old, to);
    return false;
  }
  if (had) SD_MMC.remove(old);
  return true;
}

bool storageReplace(const char *tmp, const char *to, size_t bytes) {
  bool ok = storageCardMount();
  if (ok) {
    fs::File f = SD_MMC.open(tmp);  // the size the card really has: a full card fails when the file is closed
    ok = f && !f.isDirectory() && f.size() == bytes;
  }
  ok = ok && storageRename(tmp, to);
  if (!ok) storageRemove(tmp);
  return ok;
}

bool storageReadLine(fs::File &f, String &line, size_t max) {
  line = "";
  if (!f || !f.available()) return false;
  for (int c; (c = f.read()) >= 0 && c != '\n';)
    if (line.length() < max) line += (char)c;
  return true;
}

bool storageRemove(const char *path) {
  // Checked first: removing a missing file would log an error onto the USB line.
  return storageCardMount() && SD_MMC.exists(path) && SD_MMC.remove(path);
}

// A read-only open of a namespace that doesn't exist yet (a new or fully erased board) prints an error line on
// the USB port, so the first read makes sure it exists.
static void ensureNamespace() {
  static bool done;
  if (done) return;
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, false);
  prefs.end();
  done = true;
}

int32_t storageGetInt(const char *key, int32_t fallback) {
  Preferences prefs;
  ensureNamespace();
  prefs.begin(NVS_NAMESPACE, true);
  int32_t v = prefs.getInt(key, fallback);
  prefs.end();
  return v;
}

void storagePutInt(const char *key, int32_t value) {
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, false);
  if (!prefs.isKey(key) || prefs.getInt(key) != value) prefs.putInt(key, value);
  prefs.end();
}

String storageGetString(const char *key, const char *fallback) {
  Preferences prefs;
  ensureNamespace();
  prefs.begin(NVS_NAMESPACE, true);
  // A missing key: no lookup, since getString() prints an error line on the USB port for one
  String v = prefs.isKey(key) ? prefs.getString(key, fallback) : String(fallback);
  prefs.end();
  return v;
}

void storagePutString(const char *key, const char *value) {
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, false);
  if (!prefs.isKey(key) || prefs.getString(key) != value) prefs.putString(key, value);
  prefs.end();
}

size_t storageGetBytes(const char *key, void *out, size_t len) {
  ensureNamespace();
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, true);
  size_t n = prefs.isKey(key) ? prefs.getBytesLength(key) : 0;
  if (n) n = n <= len ? prefs.getBytes(key, out, n) : 0;  // too big for `out`: an older or damaged block
  prefs.end();
  return n;
}

void storagePutBytes(const char *key, const void *data, size_t len) {
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, false);
  prefs.putBytes(key, data, len);
  prefs.end();
}

void storageRemoveKey(const char *key) {
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, false);
  prefs.remove(key);
  prefs.end();
}

void storageClearKeys() {
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, false);
  prefs.clear();
  prefs.end();
}

void storageEraseAll() {
  nvs_flash_deinit();
  nvs_flash_erase();  // the whole NVS partition: our two namespaces and the WiFi driver's saved network
}

bool storageUsage(uint64_t &used, uint64_t &total) {
  if (!storageCardMount()) return false;
  used = SD_MMC.usedBytes();
  total = SD_MMC.totalBytes();
  return true;
}

// Files used to live in internal flash (LittleFS). The first time a card mounts, copy any of them the
// card doesn't have yet (never overwriting), once. Each file is streamed and written in one go.
static void copyFile(const String &path) {
  if (SD_MMC.exists(path) || !LittleFS.exists(path)) return;
  fs::File in = LittleFS.open(path, "r"), out = SD_MMC.open(path, "w");
  uint8_t buf[512];
  for (size_t n; in && out && (n = in.read(buf, sizeof buf)) > 0;) out.write(buf, n);
}

static void copyFromFlashOnce() {
  if (storageGetInt("sd_copied", 0)) return;
  if (LittleFS.begin(false)) {
    copyFile("/timetable.csv");
    copyFile("/dex.csv");
    copyFile("/events.csv");
    if (!SD_MMC.exists("/badges")) SD_MMC.mkdir("/badges");
    fs::File dir = LittleFS.open("/badges");
    for (fs::File f = dir ? dir.openNextFile() : fs::File(); f; f = dir.openNextFile()) {
      String n = f.name();
      if (n.endsWith(".bmp")) copyFile("/badges/" + n);
    }
    LittleFS.end();
  }
  storagePutInt("sd_copied", 1);
}

bool storageCardMount() {
  if (devNoCard()) return false;  // test build switch (X NOCARD 1)
  if (cardMounted) return true;
  SD_MMC.setPins(SD_CLK, SD_CMD, SD_D0);
  cardMounted = SD_MMC.begin("/sdcard", true /*1-bit*/, false /*never format: it would erase the card*/);
  if (!cardMounted) {
    SD_MMC.end();  // missing, not FAT, or broken
    return false;
  }
  copyFromFlashOnce();
  return true;
}

void storageEnd() {
  if (!cardMounted) return;
  SD_MMC.end();
  cardMounted = false;
}

static void listCard(const char *dir) {
  fs::File d = SD_MMC.open(dir);
  for (fs::File f = d ? d.openNextFile() : fs::File(); f; f = d.openNextFile()) {
    String path = String(dir) + (strcmp(dir, "/") ? "/" : "") + f.name();
    if (f.isDirectory()) {
      Serial.printf("D %s\n", path.c_str());
      if (strcmp(dir, "/") == 0) listCard(path.c_str());  // one level down is enough (badges/)
    } else {
      Serial.printf("F %s %u\n", path.c_str(), (unsigned)f.size());
    }
  }
}

void storageCardTest() {
  if (!storageCardMount()) {
    Serial.println("OK S none (no card, or not FAT32)");
    return;
  }
  static const char *TYPES[] = {"none", "MMC", "SD", "SDHC", "unknown"};
  const uint8_t type = SD_MMC.cardType();
  Serial.printf("OK S ok %s %llu MB, %llu MB used\n", TYPES[type < 4 ? type : 4],
                SD_MMC.cardSize() / (1024 * 1024), SD_MMC.usedBytes() / (1024 * 1024));
  listCard("/");
  Serial.println("OK S end");
}

void storageCardUnmount() {
  if (!cardMounted) return;
  SD_MMC.end();
  cardMounted = false;
}
