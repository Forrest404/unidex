#include "storage.h"
#include <LittleFS.h>
#include <Preferences.h>

static const char *NVS_NAMESPACE = "unidex";

bool storageInit() {
  return LittleFS.begin(false);  // never auto-format: that would erase the uploaded files
}

fs::File storageOpen(const char *path, const char *mode) {
  return LittleFS.open(path, mode);
}

bool storageExists(const char *path) {
  return LittleFS.exists(path);
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
