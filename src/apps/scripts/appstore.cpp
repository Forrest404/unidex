#include "appstore.h"
#include <algorithm>
#if UNIDEX_EMU  // the emulator's Arduino stand-in has no Stream or Printable (and this reads from a buffer anyway)
#define ARDUINOJSON_ENABLE_ARDUINO_STREAM 0
#define ARDUINOJSON_ENABLE_ARDUINO_PRINT 0
#define ARDUINOJSON_ENABLE_PRINTABLE 0
#endif
#include <ArduinoJson.h>
#include "../../core/storage.h"

static const char *APPS_DIR = "/apps", *STAGING = "/apps/.staging";

String appsDir(const String &id) { return String(APPS_DIR) + "/" + id; }
String appsDataPath(const String &id, const char *name) { return appsDir(id) + "/data/" + name; }
String appsStagePath(const String &id, const char *path) { return String(STAGING) + "/" + id + "/" + path; }
bool appsStaging(const String &id) { return storageExists((String(STAGING) + "/" + id).c_str()); }  // (FAT: no trailing /)

bool appsManifest(const String &id, apps::Manifest &m, String &problem, bool staged) {
  const String path = (staged ? String(STAGING) + "/" + id : appsDir(id)) + "/app.json";
  fs::File f = storageOpen(path.c_str());
  if (!f) {
    problem = "no app.json";
    return false;
  }
  if (f.size() > 16384) {  // a manifest is a few hundred bytes (an icon makes it ~2 KB)
    problem = "app.json too big";
    return false;
  }
  std::vector<char> text(f.size());
  const bool read = text.empty() || f.read((uint8_t *)text.data(), text.size()) == (int)text.size();
  f.close();
  JsonDocument doc;
  const DeserializationError e = read ? deserializeJson(doc, text.data(), text.size()) : DeserializationError::IncompleteInput;
  if (e || !doc.is<JsonObject>()) {
    problem = "app.json isn't valid JSON";
    return false;
  }
  m = apps::Manifest();
  m.id = (const char *)(doc["id"] | "");
  m.name = (const char *)(doc["name"] | "");
  m.version = (const char *)(doc["version"] | "");
  m.author = (const char *)(doc["author"] | "");
  m.description = (const char *)(doc["description"] | "");
  m.type = (const char *)(doc["type"] | "app");
  m.extends = (const char *)(doc["extends"] | "");
  m.category = (const char *)(doc["category"] | "");
  m.frameMs = doc["frame_ms"] | 100;
  m.api = doc["api"] | 0;
  for (JsonVariant p : doc["permissions"].as<JsonArray>()) m.permissions.push_back((const char *)(p | ""));
  for (JsonVariant r : doc["icon"].as<JsonArray>()) m.icon.push_back((const char *)(r | ""));
  if (const char *why = apps::check(m, id.c_str())) {
    problem = why;
    return false;
  }
  problem = "";
  return true;
}

std::vector<AppEntry> appsList(bool withSizes) {
  std::vector<AppEntry> out;
  if (!storageExists(APPS_DIR)) return out;  // opening a missing folder logs an error onto the USB line
  fs::File dir = storageOpen(APPS_DIR);
  std::vector<String> ids;
  for (fs::File f = dir ? dir.openNextFile() : fs::File(); f; f = dir.openNextFile())
    if (f.isDirectory() && apps::idOk(f.name())) ids.push_back(f.name());  // skips .staging and stray folders
  dir.close();
  for (const String &id : ids) {
    AppEntry e;
    e.id = id;
    apps::Manifest m;
    e.offloaded = !storageExists((appsDir(id) + "/app.json").c_str());
    const bool ok = !e.offloaded && appsManifest(id, m, e.problem);
    if (!e.offloaded) {  // (read even when it can't run here: a game says why in Games)
      e.inGames = apps::inGames(m);
      e.game = m.type == "game";
    }
    if (ok) {
      e.name = m.name.c_str();
      e.version = m.version.c_str();
      e.extends = m.type == "plugin" ? m.extends.c_str() : "";
    } else if (!e.offloaded) {
      e.name = id;  // shown with its problem, so it can be deleted
    }
    e.code = e.data = 0;
    if (withSizes) {
      e.data = storageTreeBytes((appsDir(id) + "/data").c_str());
      e.code = storageTreeBytes(appsDir(id).c_str()) - e.data;
    }
    out.push_back(e);
  }
  for (AppEntry &e : out)
    if (!e.name.length()) e.name = e.id;  // offloaded: listed by its id
  std::sort(out.begin(), out.end(), [](const AppEntry &a, const AppEntry &b) {
    return strcasecmp(a.name.c_str(), b.name.c_str()) < 0;
  });
  return out;
}

bool appsBegin(const String &id) {
  if (!apps::idOk(id.c_str())) return false;
  const String stage = String(STAGING) + "/" + id;
  return storageRemoveTree(stage.c_str()) && storageMakeDirs(stage.c_str());
}

static uint32_t generation;
uint32_t appsGeneration() { return generation; }

const char *appsCommit(const String &id) {
  if (!apps::idOk(id.c_str())) return "bad id";
  const String stage = String(STAGING) + "/" + id, live = appsDir(id);
  apps::Manifest m;
  String problem;
  static char why[48];
  if (!appsManifest(id, m, problem, true)) {
    snprintf(why, sizeof why, "%s", problem.c_str());
    return why;
  }
  if (!storageExists((stage + "/main.lua").c_str())) return "no main.lua";
  if (storageTreeBytes(stage.c_str()) > apps::MAX_APP) return "app too big";
  const bool isNew = !storageExists((live + "/app.json").c_str()) && !storageExists(live.c_str());
  if (isNew && (int)appsList(false).size() >= apps::MAX_APPS) return "too many apps";

  // In with the new: the old code goes (its data/ stays), then each staged entry is moved across, app.json last
  // so the app only shows once the rest is there.
  if (!storageRemoveTree(live.c_str(), "data") || !storageMakeDirs(live.c_str())) return "couldn't clear old";
  std::vector<String> names;
  fs::File d = storageOpen(stage.c_str());
  for (fs::File f = d ? d.openNextFile() : fs::File(); f; f = d.openNextFile()) names.push_back(f.name());
  d.close();
  std::sort(names.begin(), names.end(), [](const String &a, const String &b) { return a != "app.json" && b == "app.json"; });
  for (const String &n : names) {
    if (n == "data") continue;  // never sent (installPathOk), but never let one replace the real data
    if (!storageRename((stage + "/" + n).c_str(), (live + "/" + n).c_str())) return "couldn't move files";
  }
  storageRemoveTree(stage.c_str());
  generation++;
  return nullptr;
}

// A folder with nothing in it (or missing).
static bool emptyFolder(const char *path) {
  if (!storageExists(path)) return true;
  fs::File d = storageOpen(path);
  fs::File f = d ? d.openNextFile() : fs::File();
  return d && d.isDirectory() && !f;
}

bool appsRemove(const String &id, bool keepData) {
  if (!apps::idOk(id.c_str())) return false;
  generation++;
  const bool ok = storageRemoveTree(appsDir(id).c_str(), keepData ? "data" : nullptr);
  storageRemoveTree((String(STAGING) + "/" + id).c_str());  // an unfinished install of it, too
  // Leave the card as it was before the first app: no empty folders behind.
  if (emptyFolder(STAGING)) storageRemoveTree(STAGING);
  if (emptyFolder(APPS_DIR)) storageRemoveTree(APPS_DIR);
  return ok;
}
