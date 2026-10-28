#pragma once
#include <Arduino.h>

// The device installing apps from the store itself (for the phone portal, whose phone has no internet while it's
// on the hotspot). The catalogue is saved on the card, so the portal can show it with no WiFi too; files come from
// tools.unidex.shop, checked against the catalogue's sizes and SHA-256, then go through appsBegin/appsCommit like
// a USB install. WiFi must already be up.

static const char *const APPFETCH_CATALOGUE = "/apps/.store.json";

const char *appfetchCatalogue();  // downloads the catalogue to the card: nullptr, or why not

// One install at a time, a file per step, so whoever drives it (the portal's tick) keeps answering in between.
const char *appfetchStart(const String &id);  // nullptr once begun, else why not (nothing changes)
bool appfetchBusy();
void appfetchStep();   // the next file, or the commit after the last
void appfetchCancel(); // drops an unfinished install (its staging copy is cleared by the next begin)

// What the last install did: its id, files done of total, and when it has finished, ok or why not.
struct AppfetchStatus {
  String id, why;
  int done = 0, total = 0;
  bool busy = false, ok = false;
};
const AppfetchStatus &appfetchStatus();
