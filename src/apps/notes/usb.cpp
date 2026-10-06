#include "usb.h"
#include <Arduino.h>
#include <esp_rom_crc.h>
#include <math.h>
#include "cloud.h"
#include "job.h"
#include "store.h"
#include "../../core/audio.h"
#include "../../core/credentials.h"
#include "../../core/net.h"
#include "../../core/power.h"
#include "../../core/storage.h"

static int hexValue(char c) {
  return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
}

static bool fromHex(const char *hex, String &out) {
  out = "";
  const size_t n = strlen(hex);
  if (n % 2) return false;
  for (size_t i = 0; i < n; i += 2) {
    const int hi = hexValue(hex[i]), lo = hexValue(hex[i + 1]);
    if (hi < 0 || lo < 0) return false;
    out += (char)(hi << 4 | lo);
  }
  return true;
}

static void printHex(const String &s) {
  for (size_t i = 0; i < s.length(); i++) Serial.printf("%02x", (uint8_t)s[i]);
}

// A note id from the page: digits, letters and dashes only, so it can't reach outside /notes.
// Notes still to transcribe or push (`notes` gets how many there are). The card must be readable.
static int waitingNotes(int &notes) {
  const bool gh = credGet("gh_on") == "1";
  int waiting = 0;
  notes = 0;
  for (const NoteInfo &n : storeList()) {
    notes++;
    waiting += !n.text || (!n.pushed && gh);
  }
  return waiting;
}

static void status() {
  // Each line printed separately: credStatus writes them straight to Serial with the "NS " prefix added here.
  struct Prefixed : Print {
    bool start = true;
    size_t write(uint8_t c) override {
      if (start) Serial.print("NS ");
      start = c == '\n';
      return Serial.write(c);
    }
  } out;
  credStatus(out);
  int notes = 0;
  const bool card = storeReady();
  const int waiting = card ? waitingNotes(notes) : 0;
  Serial.printf("NC %d %d %d\nOK N ?\n", card, notes, waiting);
}

// The website's one-click sync: sends waiting notes in the background, as B on the Notes screen does.
static void startSync() {
  int notes = 0;
  if (jobBusy()) Serial.println("OK N SYNC busy");
  else if (!storeReady()) Serial.println("OK N SYNC fail no SD card");
  else if (!credHas("wifi_ssid")) Serial.println("OK N SYNC fail WiFi not set up");
  else if (const int waiting = waitingNotes(notes)) {
    if (jobStartSweep()) Serial.printf("OK N SYNC started %d\n", waiting);
    else Serial.println("OK N SYNC busy");
  } else {
    Serial.println("OK N SYNC none");
  }
}

// Records 2 s and reports how loud it was. With `radio`, WiFi is connecting meanwhile (to check it adds no hum).
static void micTest(bool radio) {
  if (radio && !jobBusy()) netBegin();
  if (!audioBegin()) {
    Serial.println("OK N MIC fail codec not answering");
    return;
  }
  static int16_t buf[512];
  int peak = 0;
  double sum = 0;
  size_t total = 0;
  const uint32_t t0 = millis();
  while (millis() - t0 < 2000) {
    const size_t n = audioRead(buf, 512);
    if (millis() - t0 < 300) continue;  // the first moments after power-up are a click
    for (size_t i = 0; i < n; i++) {
      peak = max(peak, abs((int)buf[i]));
      sum += (double)buf[i] * buf[i];
    }
    total += n;
  }
  audioEnd();
  if (radio && !jobBusy()) netOff();
  powerActivity();
  if (!total) Serial.println("OK N MIC fail no samples");
  else Serial.printf("OK N MIC %d %d\n", peak, (int)sqrt(sum / total));
}

static void list() {
  for (const NoteInfo &n : storeList()) {
    Serial.printf("NF %s %u %d %d ", n.id.c_str(), (unsigned)storeReadNote(n.id).length(), n.text, n.pushed);
    printHex(n.title);
    Serial.println();
  }
  Serial.println("OK N LIST");
}

static void read(const String &id) {
  if (!storeValidId(id)) {
    Serial.println("ERR");
    return;
  }
  const String md = storeReadNote(id);
  if (!md.length()) {
    Serial.println("ERR");
    return;
  }
  for (size_t i = 0; i < md.length(); i += 64) {
    Serial.print("ND ");
    printHex(md.substring(i, i + 64));
    Serial.println();
  }
  const uint32_t crc = esp_rom_crc32_le(0, (const uint8_t *)md.c_str(), md.length());
  Serial.printf("OK N READ %u %lu\n", (unsigned)md.length(), (unsigned long)crc);
}

static String caChunks;  // N ADD wifi_ca chunks waiting for the final N SET wifi_ca

bool notesUsb(const char *l) {
  if (strncmp(l, "N ", 2) != 0) return false;
  char cmd[8] = "", arg[24] = "";
  const int n = sscanf(l + 2, "%7s %23s", cmd, arg);
  if (n < 1) return false;
  if (strcmp(cmd, "?") == 0) {
    status();
  } else if (strcmp(cmd, "SET") == 0) {
    String value;
    const char *hex = n == 2 ? strchr(l + 6, ' ') : nullptr;  // after "N SET <name>"
    const bool parsed = n == 2 && credKnown(arg) && fromHex(hex ? hex + 1 : "", value);
    if (strcmp(arg, "wifi_ca") == 0) value = caChunks + value;  // the CA is saved once, with its last chunk
    caChunks = "";
    if (parsed && credSet(arg, value))
      Serial.printf("OK N SET %s\n", arg);
    else
      Serial.println("ERR");
  } else if (strcmp(cmd, "ADD") == 0) {  // a wifi_ca chunk, kept in RAM until N SET wifi_ca
    String value;
    const char *hex = n == 2 ? strchr(l + 6, ' ') : nullptr;
    if (n == 2 && strcmp(arg, "wifi_ca") == 0 && hex && fromHex(hex + 1, value) &&
        caChunks.length() + value.length() <= 3000) {
      caChunks += value;
      Serial.printf("OK N ADD %s\n", arg);
    } else {
      caChunks = "";
      Serial.println("ERR");
    }
  } else if (strcmp(cmd, "CLR") == 0) {
    if (jobBusy()) {  // the note being sent still needs its keys
      Serial.println("ERR busy");
      return true;
    }
    if (strcmp(arg, "all") == 0) credClearAll();
    else if (credKnown(arg)) credClear(arg);
    Serial.println("OK N CLR");
  } else if (strcmp(cmd, "TEST") == 0) {
    const char *err = cloudTest(arg);
    if (err) Serial.printf("OK N TEST %s fail %s\n", arg, err);
    else Serial.printf("OK N TEST %s ok\n", arg);
    powerActivity();
  } else if (strcmp(cmd, "MIC") == 0) {
    micTest(strcmp(arg, "wifi") == 0);
  } else if (strcmp(cmd, "LIST") == 0) {
    list();
  } else if (strcmp(cmd, "READ") == 0) {
    read(arg);
  } else if (strcmp(cmd, "DEL") == 0) {
    if (jobBusy() && strcmp(arg, jobStatus().noteId) == 0) {
      Serial.println("ERR busy");
      return true;
    }
    if (storeValidId(arg)) storeDelete(arg);
    Serial.println("OK N DEL");
  } else if (strcmp(cmd, "SYNC") == 0) {
    startSync();
  } else if (strcmp(cmd, "JOB") == 0) {
    const JobStatus js = jobStatus();
    Serial.printf("OK N JOB %d %d %lu ", jobBusy(), (int)js.step, (unsigned long)js.gen);
    printHex(String(js.result));
    Serial.print(' ');
    printHex(String(js.noteId));
    Serial.printf(" %u\n", (unsigned)jobStackLeft());
  } else {
    return false;
  }
  return true;
}
