#include "credentials.h"
#include <Preferences.h>

static const char *NAMESPACE = "unidex_cred";

struct Field {
  const char *name;
  bool secret;
  uint16_t maxLen;
  const char *fallback;
};

static const Field FIELDS[] = {
  {"wifi_ssid", false, 32, ""},      {"wifi_pass", true, 128, ""},     {"wifi_user", false, 128, ""},
  {"openai_key", true, 256, ""},     {"anthropic_key", true, 256, ""},
  {"cleanup", false, 12, "openai"},  {"cleanup_model", false, 64, ""},
  {"gh_on", false, 1, "0"},          {"gh_repo", false, 100, ""},
  {"gh_branch", false, 64, "main"},  {"gh_dir", false, 64, "VoiceNotes"},
  {"gh_token", true, 256, ""},
};

// Opening a namespace read-only fails (and logs an error onto the USB line) until it exists: create it once.
static void ensureNamespace() {
  static bool done;
  if (done) return;
  Preferences p;
  p.begin(NAMESPACE, false);
  p.end();
  done = true;
}

static const Field *find(const char *name) {
  for (auto &f : FIELDS)
    if (strcmp(f.name, name) == 0) return &f;
  return nullptr;
}

bool credKnown(const char *name) { return find(name); }

bool credSecret(const char *name) {
  const Field *f = find(name);
  return f && f->secret;
}

bool credHas(const char *name) {
  ensureNamespace();
  Preferences p;
  p.begin(NAMESPACE, true);
  bool has = p.isKey(name) && p.getString(name).length() > 0;
  p.end();
  return has;
}

String credGet(const char *name) {
  const Field *f = find(name);
  if (!f) return "";
  ensureNamespace();
  Preferences p;
  p.begin(NAMESPACE, true);
  String v = p.isKey(name) ? p.getString(name) : String();
  p.end();
  return v.length() ? v : String(f->fallback);
}

bool credSet(const char *name, const String &value) {
  const Field *f = find(name);
  if (!f || value.length() > f->maxLen) return false;
  for (size_t i = 0; i < value.length(); i++)
    if (value[i] < 32 || value[i] > 126) return false;  // keys, SSIDs and repo names are printable ASCII
  if (!value.length()) {
    credClear(name);
    return true;
  }
  Preferences p;
  p.begin(NAMESPACE, false);
  if (!p.isKey(name) || p.getString(name) != value) p.putString(name, value);
  p.end();
  return true;
}

void credClear(const char *name) {
  Preferences p;
  p.begin(NAMESPACE, false);
  if (p.isKey(name)) p.remove(name);  // removing a missing key logs an error onto the USB line
  p.end();
}

void credClearAll() {
  Preferences p;
  p.begin(NAMESPACE, false);
  p.clear();
  p.end();
}

static void printHex(Print &out, const String &s) {
  for (size_t i = 0; i < s.length(); i++) out.printf("%02x", (uint8_t)s[i]);
}

void credStatus(Print &out) {
  for (auto &f : FIELDS) {
    const bool has = credHas(f.name);
    out.printf("%s %s ", f.name, has ? "set" : "unset");
    const String v = credGet(f.name);
    if (!f.secret) printHex(out, v);
    else if (has && strcmp(f.name, "wifi_pass") != 0)  // a hint for long random keys, never a password
      printHex(out, v.substring(v.length() > 4 ? v.length() - 4 : v.length()));
    out.print("\n");
  }
}
