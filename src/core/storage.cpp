#include "storage.h"
#include <LittleFS.h>
#include <Preferences.h>
#include <SD_MMC.h>

static const char *NVS_NAMESPACE = "unidex";
static const int SD_CLK = 39, SD_CMD = 41, SD_D0 = 40;  // Waveshare 04_SD_Card example + schematic
static bool cardMounted;

bool storageInit() {
  return LittleFS.begin(false);  // never auto-format: that would erase the uploaded files
}

fs::File storageOpen(const char *path, const char *mode) {
  return LittleFS.open(path, mode);
}

bool storageExists(const char *path) {
  return LittleFS.exists(path);
}

bool storageRename(const char *from, const char *to) {
  LittleFS.remove(to);
  return LittleFS.rename(from, to);
}

bool storageRemove(const char *path) {
  return LittleFS.remove(path);
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

void storageUsage(size_t &used, size_t &total) {
  used = LittleFS.usedBytes();
  total = LittleFS.totalBytes();
}

bool storageCardMount() {
  if (cardMounted) return true;
  SD_MMC.setPins(SD_CLK, SD_CMD, SD_D0);
  cardMounted = SD_MMC.begin("/sdcard", true /*1-bit*/, false /*never format: it would erase the card*/);
  if (!cardMounted) SD_MMC.end();  // missing, not FAT, or broken
  return cardMounted;
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
