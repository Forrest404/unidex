#include "usb.h"
#include <Arduino.h>
#include <esp_rom_crc.h>
#include <math.h>
#include "cloud.h"
#include "store.h"
#include "../../core/audio.h"
#include "../../core/credentials.h"
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
static bool validId(const String &id) {
  if (!id.length() || id.length() > 24) return false;
  for (size_t i = 0; i < id.length(); i++)
    if (!isalnum((unsigned char)id[i]) && id[i] != '-') return false;
  return true;
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
  int notes = 0, waiting = 0;
  const bool card = storeReady();
  if (card)
    for (const NoteInfo &n : storeList()) {
      notes++;
      waiting += !n.text || (!n.pushed && credGet("gh_on") == "1");
    }
  Serial.printf("NC %d %d %d\nOK N ?\n", card, notes, waiting);
}

static void micTest() {
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
  if (!validId(id)) {
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
    if (strcmp(arg, "all") == 0) credClearAll();
    else if (credKnown(arg)) credClear(arg);
    Serial.println("OK N CLR");
  } else if (strcmp(cmd, "TEST") == 0) {
    const char *err = cloudTest(arg);
    if (err) Serial.printf("OK N TEST %s fail %s\n", arg, err);
    else Serial.printf("OK N TEST %s ok\n", arg);
    powerActivity();
  } else if (strcmp(cmd, "MIC") == 0) {
    micTest();
  } else if (strcmp(cmd, "LIST") == 0) {
    list();
  } else if (strcmp(cmd, "READ") == 0) {
    read(arg);
  } else if (strcmp(cmd, "DEL") == 0) {
    if (validId(arg)) storeDelete(arg);
    Serial.println("OK N DEL");
  } else {
    return false;
  }
  return true;
}
