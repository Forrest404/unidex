#pragma once
// Installing an app from the store (tools.unidex.shop/store/index.json) by the device itself, for the phone portal:
// what an entry of the catalogue must say before anything is downloaded, and where each file is. No Arduino in
// here, so tools/apptest checks it on a computer.
#include <cstdint>
#include <string>
#include <vector>
#include "apps_logic.h"

namespace appfetch {

static const char *const HOST = "tools.unidex.shop";
static const size_t MAX_CATALOGUE = 256 * 1024;  // index.json is ~40 KB for 18 apps
static const size_t MAX_FILES = 32;

struct File {
  std::string path, sha256;  // sha256: 64 lower-case hex digits
  uint64_t size = 0;
};

// One app of the catalogue, already read (the device fills it with ArduinoJson; the tests by hand).
struct Entry {
  std::string id, version;
  int api = 0;  // the app's own level; 0 in a catalogue from before it was listed
  std::vector<File> files;
};

inline bool hexDigest(const std::string &s) {
  if (s.size() != 64) return false;
  for (char c : s)
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
  return true;
}

// nullptr if the device may download this app, else a short reason for the phone. app.json itself is checked
// again (apps::check) before the app is put in place.
inline const char *check(const Entry &e) {
  if (!apps::idOk(e.id.c_str())) return "Not an app the store lists";
  if (apps::versionValue(e.version.c_str()) < 0) return "The store lists a bad version";
  if (e.api > apps::API_LEVEL) return "Needs newer firmware";
  if (e.files.empty() || e.files.size() > MAX_FILES) return "The store lists bad files";
  uint64_t total = 0;
  bool main = false, manifest = false;
  for (const File &f : e.files) {
    if (!apps::installPathOk(f.path.c_str()) || !hexDigest(f.sha256) || f.size > apps::MAX_FILE)
      return "The store lists bad files";
    total += f.size;
    main |= f.path == "main.lua";
    manifest |= f.path == "app.json";
  }
  if (!main || !manifest) return "The store lists bad files";
  if (total > apps::MAX_APP) return "Too big";
  return nullptr;
}

// Where a file is on the store: /store/apps/<id>/<version>/<path>. Every part has passed check(), so nothing in
// it needs escaping.
inline std::string filePath(const Entry &e, const File &f) {
  return "/store/apps/" + e.id + "/" + e.version + "/" + f.path;
}

inline std::string hex(const uint8_t *p, size_t n) {
  static const char digits[] = "0123456789abcdef";
  std::string out;
  for (size_t i = 0; i < n; i++) out += digits[p[i] >> 4], out += digits[p[i] & 15];
  return out;
}

}  // namespace appfetch
