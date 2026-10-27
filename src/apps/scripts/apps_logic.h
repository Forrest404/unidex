#pragma once
// The rules for third-party apps (docs/APPS.md), kept free of Arduino so they can be tested on a computer
// (tools/apptest). Names, paths, the manifest and versions; nothing here touches a file.
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace apps {

const int API_LEVEL = 2;                // what this firmware offers (unidex.api)
const size_t MAX_FILE = 64 * 1024;      // one file, code or data
const size_t MAX_APP = 512 * 1024;      // an app's code and assets
const size_t MAX_DATA = 512 * 1024;     // an app's data/ folder
const int MAX_APPS = 24;
const size_t ID_LEN = 24, TITLE_LEN = 20, AUTHOR_LEN = 40, DESC_LEN = 140, FILE_LEN = 32;  // (NAME_MAX is a libc macro)

enum Permission : uint8_t { PERM_WIFI = 1, PERM_MIC = 2, PERM_MEET = 4 };

inline bool lowerDigit(char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'); }

// An app's id (its folder name): a-z, 0-9 and '-', not starting with '-', up to 24 characters.
inline bool idOk(const char *s) {
  const size_t n = strlen(s);
  if (!n || n > ID_LEN || s[0] == '-') return false;
  for (size_t i = 0; i < n; i++)
    if (!lowerDigit(s[i]) && s[i] != '-') return false;
  return true;
}

// One file or folder name inside an app: a-z, 0-9, '.', '_', '-'; not starting with '.', no "..".
inline bool nameOk(const char *s, size_t len) {
  if (!len || len > FILE_LEN || s[0] == '.') return false;
  for (size_t i = 0; i < len; i++) {
    const char c = s[i];
    if (!lowerDigit(c) && c != '.' && c != '_' && c != '-') return false;
    if (c == '.' && i + 1 < len && s[i + 1] == '.') return false;
  }
  return true;
}
inline bool nameOk(const char *s) { return nameOk(s, strlen(s)); }

inline bool endsWith(const char *s, const char *tail) {
  const size_t n = strlen(s), t = strlen(tail);
  return n >= t && strcmp(s + n - t, tail) == 0;
}

// A file an install may write, relative to the app's folder: "main.lua", "lib/util.lua", "img/a.bmp". At most two
// folders deep, only .lua .json .bmp .txt, and never inside data/ (that belongs to the device).
inline bool installPathOk(const char *p) {
  if (!*p || *p == '/') return false;
  int depth = 0;
  const char *seg = p;
  for (const char *c = p;; c++) {
    if (*c == '/' || *c == 0) {
      if (!nameOk(seg, c - seg)) return false;
      if (seg == p && *c == '/' && (c - seg) == 4 && strncmp(seg, "data", 4) == 0) return false;
      if (*c == 0) break;
      if (++depth > 2) return false;
      seg = c + 1;
    }
  }
  return endsWith(p, ".lua") || endsWith(p, ".json") || endsWith(p, ".bmp") || endsWith(p, ".txt");
}

// "major.minor.patch" (each 0-9999) into one comparable number; -1 if it isn't one.
inline int64_t versionValue(const char *v) {
  int64_t out = 0;
  for (int part = 0; part < 3; part++) {
    if (*v < '0' || *v > '9') return -1;
    int64_t n = 0;
    int digits = 0;
    while (*v >= '0' && *v <= '9') {
      if (++digits > 4) return -1;
      n = n * 10 + (*v++ - '0');
    }
    out = out * 10000 + n;
    if (part < 2) {
      if (*v != '.') return -1;
      v++;
    }
  }
  return *v ? -1 : out;
}

// <0, 0 or >0 like strcmp; an invalid version sorts lowest.
inline int versionCompare(const char *a, const char *b) {
  const int64_t x = versionValue(a), y = versionValue(b);
  return x < y ? -1 : x > y ? 1 : 0;
}

inline bool permissionBit(const std::string &name, uint8_t &bit) {
  if (name == "wifi") bit = PERM_WIFI;
  else if (name == "mic") bit = PERM_MIC;
  else if (name == "meet") bit = PERM_MEET;
  else return false;
  return true;
}

// The built-in apps plugins can extend, and the function each one calls (docs/APPS.md, Plugins).
//   badge: function badge() draws a whole badge (Badge lists it after the image badges)
inline bool extendable(const std::string &app) { return app == "badge"; }

// What app.json says, already read (the device fills it with ArduinoJson; the tests by hand).
struct Manifest {
  std::string id, name, version, author, description, type = "app", extends;
  int api = 0;
  std::vector<std::string> permissions;
  std::vector<std::string> icon;  // empty, or 40 rows of 40
  uint8_t perms = 0;              // set by check()
};

// nullptr if the app can run here, else a short reason for the screen. `folder` is the folder it was found in.
inline const char *check(Manifest &m, const char *folder) {
  if (!idOk(m.id.c_str())) return "bad id";
  if (m.id != folder) return "id isn't its folder";
  if (m.name.empty() || m.name.size() > TITLE_LEN) return "bad name";
  if (m.author.size() > AUTHOR_LEN || m.description.size() > DESC_LEN) return "text too long";
  if (versionValue(m.version.c_str()) < 0) return "bad version";
  if (m.api < 1) return "no api level";
  if (m.api > API_LEVEL) return "needs newer firmware";
  if (m.type == "plugin") {
    if (m.extends.empty()) return "a plugin needs \"extends\"";
    if (!extendable(m.extends)) return "can't extend that app";
  } else if (m.type != "app") {
    return "unknown type";
  } else if (!m.extends.empty()) {
    return "only plugins extend an app";
  }
  m.perms = 0;
  for (const std::string &p : m.permissions) {
    uint8_t bit;
    if (!permissionBit(p, bit)) return "unknown permission";
    m.perms |= bit;
  }
  if (!m.icon.empty()) {
    if (m.icon.size() != 40) return "icon must be 40 rows";
    for (const std::string &row : m.icon)
      if (row.size() != 40) return "icon rows must be 40 wide";
  }
  return nullptr;
}

}  // namespace apps
