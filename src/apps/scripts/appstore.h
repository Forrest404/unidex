#pragma once
#include <Arduino.h>
#include <vector>
#include "apps_logic.h"

// Third-party apps on the SD card (docs/APPS.md): /apps/<id>/app.json, main.lua, ..., data/.
// An install is staged in /apps/.staging/<id>/ and moved into place only when complete (appsCommit), so a cable
// pulled halfway leaves the old version (or nothing) behind, never half an app.

struct AppEntry {
  String id, name, version;
  String extends;      // a plugin: the built-in app it adds to ("badge"); "" for an app
  String problem;      // why it can't run ("needs newer firmware"), or "" if it can
  bool offloaded;      // only its data/ is left
  uint64_t code, data; // bytes (only when asked for: reading sizes walks the folders)
};

String appsDir(const String &id);                     // "/apps/<id>"
String appsDataPath(const String &id, const char *name);  // "/apps/<id>/data/<name>"
// Every folder in /apps, by name. withSizes: fill code and data (slower).
std::vector<AppEntry> appsList(bool withSizes);
// Reads and checks /apps/<id>/app.json (or the staged copy). false with the reason in `problem`.
bool appsManifest(const String &id, apps::Manifest &m, String &problem, bool staged = false);

// Installing: begin clears any unfinished install of `id`; each file is then written to the staging folder
// (appsStagePath), and commit checks the whole app and moves it into place, keeping the app's data/.
bool appsBegin(const String &id);
String appsStagePath(const String &id, const char *path);
bool appsStaging(const String &id);  // an install of `id` has begun (A BEGIN)
const char *appsCommit(const String &id);  // nullptr when installed, else a short reason (and nothing changes)
// Removes an app: keepData leaves data/ (an offload), otherwise everything goes.
bool appsRemove(const String &id, bool keepData);
