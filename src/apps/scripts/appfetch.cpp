#include "appfetch.h"
#include <ArduinoJson.h>
#include <mbedtls/sha256.h>
#include <vector>
#include "appfetch_logic.h"
#include "appstore.h"
#include "scripts.h"
#include "../../core/net.h"
#include "../../core/storage.h"

static AppfetchStatus status;
static appfetch::Entry entry;  // the app being installed

#if MBEDTLS_VERSION_MAJOR < 3  // the core's mbedtls 2.x names the calls that return a result with _ret
#define mbedtls_sha256_starts mbedtls_sha256_starts_ret
#define mbedtls_sha256_update mbedtls_sha256_update_ret
#define mbedtls_sha256_finish mbedtls_sha256_finish_ret
#endif

// The catalogue is kept in memory until the connection has closed, then written: bytes written to the card while
// a download is still coming in were lost (the end of each file, in tests).
const char *appfetchCatalogue() {
  std::vector<uint8_t> data;
  String location;
  const int code = netHttpsGet(appfetch::HOST, "/store/index.json", [&](const uint8_t *p, size_t n) {
    if (data.size() + n > appfetch::MAX_CATALOGUE) return false;
    data.insert(data.end(), p, p + n);
    return true;
  }, location);
  if (code < 0) return "Couldn't reach the store";
  if (code != 200 || data.empty() || data[0] != '{') return "The store's list didn't load";
  static const char *tmp = "/apps/.store.json.tmp";
  fs::File out = storageOpen(tmp, "w");
  if (!out) return "No SD card";
  const bool wrote = out.write(data.data(), data.size()) == data.size();
  out.close();
  return wrote && storageReplace(tmp, APPFETCH_CATALOGUE, data.size()) ? nullptr : "Card full";
}

// The app's entry from the saved catalogue: only the fields an install needs are kept.
static bool findEntry(const String &id) {
  fs::File f = storageOpen(APPFETCH_CATALOGUE);
  if (!f) return false;
  JsonDocument filter;
  JsonObject app = filter["apps"].add<JsonObject>();
  app["id"] = app["version"] = app["api"] = true;
  JsonObject file = app["files"].add<JsonObject>();
  file["path"] = file["size"] = file["sha256"] = true;
  JsonDocument doc;
  const DeserializationError e = deserializeJson(doc, f, DeserializationOption::Filter(filter));
  f.close();
  if (e) return false;
  for (JsonObject a : doc["apps"].as<JsonArray>()) {
    if (id != (const char *)(a["id"] | "")) continue;
    entry = appfetch::Entry();
    entry.id = id.c_str();
    entry.version = (const char *)(a["version"] | "");
    entry.api = a["api"] | 0;
    for (JsonObject x : a["files"].as<JsonArray>()) {
      appfetch::File file;
      file.path = (const char *)(x["path"] | "");
      file.sha256 = (const char *)(x["sha256"] | "");
      file.size = x["size"] | (uint64_t)0;
      entry.files.push_back(file);
    }
    return true;
  }
  return false;
}

static void finish(const char *why) {
  status.busy = false;
  status.ok = !why;
  status.why = why ? why : "";
}

const char *appfetchStart(const String &id) {
  if (status.busy) return "Another app is installing";
  if (!apps::idOk(id.c_str()) || !findEntry(id)) return "Not an app the store lists";
  if (const char *why = appfetch::check(entry)) return why;
  if (scriptsRunning(id.c_str())) return "Close it on your unidex first";
  if (!appsBegin(id)) return "No SD card";
  status = AppfetchStatus();
  status.id = id;
  status.total = entry.files.size();
  status.busy = true;
  return nullptr;
}

bool appfetchBusy() { return status.busy; }
const AppfetchStatus &appfetchStatus() { return status; }

void appfetchCancel() {
  if (status.busy) finish("Stopped");
}

// Downloads one file (64 KB at most) into memory, checks it, then writes it to the staging folder whole (as above).
static const char *fetchFile(const appfetch::File &want) {
  std::vector<uint8_t> data;
  data.reserve(want.size);
  String location;
  const int code = netHttpsGet(appfetch::HOST, appfetch::filePath(entry, want).c_str(), [&](const uint8_t *p, size_t n) {
    if (data.size() + n > want.size) return false;
    data.insert(data.end(), p, p + n);
    return true;
  }, location);
  if (code < 0) return "Couldn't reach the store";
  if (code != 200) return "Download failed";
  uint8_t digest[32];
  mbedtls_sha256_context sha;
  mbedtls_sha256_init(&sha);
  mbedtls_sha256_starts(&sha, 0);
  mbedtls_sha256_update(&sha, data.data(), data.size());
  mbedtls_sha256_finish(&sha, digest);
  mbedtls_sha256_free(&sha);
  if (data.size() != want.size || appfetch::hex(digest, sizeof digest) != want.sha256)
    return "Changed on the store: try again";
  fs::File out = storageOpen(appsStagePath(status.id, want.path.c_str()).c_str(), "w");
  if (!out) return "No SD card";
  const bool wrote = out.write(data.data(), data.size()) == data.size();
  out.close();
  return wrote ? nullptr : "Card full";
}

void appfetchStep() {
  if (!status.busy) return;
  if (status.done < status.total) {
    if (const char *why = fetchFile(entry.files[status.done])) finish(why);
    else status.done++;
    return;
  }
  if (scriptsRunning(status.id.c_str())) return finish("Close it on your unidex first");
  const char *why = appsCommit(status.id);
  if (!why) scriptsListChanged();
  finish(why);
}
