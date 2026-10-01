#include "storage.h"
#include <LittleFS.h>  // only for the one-time copy from internal flash
#include <Preferences.h>
#include <SD_MMC.h>

static const char *NVS_NAMESPACE = "unidex";
static const int SD_CLK = 39, SD_CMD = 41, SD_D0 = 40;  // Waveshare 04_SD_Card example + schematic
static bool cardMounted;

fs::File storageOpen(const char *path, const char *mode) {
  if (!storageCardMount()) return fs::File();
  if (*mode != 'r') {  // FAT won't create a file in a missing folder (e.g. /badges on a new card)
    String dir = String(path).substring(0, String(path).lastIndexOf('/'));
    if (dir.length() && !SD_MMC.exists(dir)) SD_MMC.mkdir(dir);
  }
  return SD_MMC.open(path, mode);
}

bool storageExists(const char *path) {
  return storageCardMount() && SD_MMC.exists(path);
}

bool storageRename(const char *from, const char *to) {
  if (!storageCardMount()) return false;
  SD_MMC.remove(to);
  return SD_MMC.rename(from, to);
}

bool storageRemove(const char *path) {
  return storageCardMount() && SD_MMC.remove(path);
}

int32_t storageGetInt(const char *key, int32_t fallback) {
  Preferences prefs;
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
  prefs.begin(NVS_NAMESPACE, true);
  String v = prefs.getString(key, fallback);
  prefs.end();
  return v;
}

void storagePutString(const char *key, const char *value) {
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, false);
  if (!prefs.isKey(key) || prefs.getString(key) != value) prefs.putString(key, value);
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
