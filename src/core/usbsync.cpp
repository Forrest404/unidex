#include "usbsync.h"
#include <Arduino.h>
#include <esp_rom_crc.h>
#include "clock.h"
#include "power.h"
#include "storage.h"

// Protocol (text lines, Mac -> device):
//   ?                      -> "unidex 1"
//   T <unix seconds>       -> sets the clock, "OK T"
//   C                      -> "OK C <clock chip registers and system time>" (diagnostics)
//   S                      -> SD card test: "OK S ...", "F <path> <bytes>" / "D <dir>" lines, "OK S end"
//   E <count> <crc32>      then <count> lines "YYYY-MM-DD,HH:MM,HH:MM,title,location"
//                          -> "OK E <crc>" or "ERR"; crc32 (zlib) covers each line plus '\n'
// Badge upload (tools/badge-maker.html over Web Serial):
//   L                      -> "F <name>" per badge, then "OK L"
//   B <name> <bytes> <crc32> -> "OK B" or "ERR"; then
//   D <hex>                -> "K" per line (max 64 bytes; the ack is the flow control), and after the
//                          last byte "OK F <name>" or "ERR". Written to a temp file, then renamed.
static const char *EVENTS = "/events.csv", *EVENTS_TMP = "/events.tmp";
static const char *BADGE_TMP = "/badges/upload.tmp";
static const int32_t MAX_BADGE_BYTES = 16384;  // a 200x200 1-bit BMP is 5062
static const uint32_t STALL_MS = 3000;  // give up on a transfer that stops halfway

static char line[160];
static size_t len;
static int remaining;              // event lines still to come
static uint32_t expectedCrc, crc, lastLineAt, generation;
RTC_DATA_ATTR static time_t lastSync;
static bool unchanged;             // same crc as the saved file: check it, but don't rewrite flash
static fs::File out;

static char badgeName[32], newBadge[32];  // newBadge: finished upload, not yet shown
static int32_t badgeLeft;          // bytes still to come
static uint32_t badgeCrc, badgeExpected;
static fs::File badgeOut;

static void abortBadge() {
  badgeLeft = 0;
  if (badgeOut) badgeOut.close();
  storageRemove(BADGE_TMP);
}

// Lower-case letters, digits and dashes, ending in .bmp: safe as a file name, and sorts like the rest.
static bool validBadgeName(const char *n) {
  const size_t len = strlen(n);
  if (len < 5 || len >= sizeof badgeName || strcmp(n + len - 4, ".bmp") != 0) return false;
  for (size_t i = 0; i < len - 4; i++)
    if (!(islower(n[i]) || isdigit(n[i]) || n[i] == '-')) return false;
  return true;
}

static int hexValue(char c) {
  return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
}

static void badgeData(const char *hex) {
  uint8_t buf[64];
  const size_t n = strlen(hex) / 2;
  bool ok = strlen(hex) % 2 == 0 && n > 0 && n <= sizeof buf && (int32_t)n <= badgeLeft;
  for (size_t i = 0; ok && i < n; i++) {
    int hi = hexValue(hex[2 * i]), lo = hexValue(hex[2 * i + 1]);
    ok = hi >= 0 && lo >= 0;
    buf[i] = hi << 4 | lo;
  }
  if (!ok || badgeOut.write(buf, n) != n) {
    abortBadge();
    Serial.println("ERR");
    return;
  }
  badgeCrc = esp_rom_crc32_le(badgeCrc, buf, n);
  badgeLeft -= n;
  Serial.println("K");
  if (badgeLeft > 0) return;
  badgeOut.close();
  if (badgeCrc != badgeExpected) {
    storageRemove(BADGE_TMP);
    Serial.println("ERR");
    return;
  }
  storageRename(BADGE_TMP, (String("/badges/") + badgeName).c_str());
  strlcpy(newBadge, badgeName, sizeof newBadge);
  Serial.printf("OK F %s\n", badgeName);
}

static void listBadges() {
  fs::File dir = storageOpen("/badges");
  for (fs::File f = dir ? dir.openNextFile() : fs::File(); f; f = dir.openNextFile()) {
    String n = f.name();
    if (n.endsWith(".bmp")) Serial.printf("F %s\n", n.c_str());
  }
  Serial.println("OK L");
}

static void finishEvents() {
  if (out) out.close();
  if (crc != expectedCrc) {
    storageRemove(EVENTS_TMP);
    Serial.println("ERR");
    return;
  }
  if (!unchanged) {
    storageRename(EVENTS_TMP, EVENTS);
    storagePutInt("events_crc", (int32_t)crc);
    generation++;
  }
#if DEBUG
  Serial.println(unchanged ? "events unchanged" : "events saved");
#endif
  lastSync = time(nullptr);
  Serial.printf("OK E %lu\n", (unsigned long)crc);
}

static void handle(const char *l) {
  if (badgeLeft > 0 && l[0] == 'D' && l[1] == ' ') {
    badgeData(l + 2);
    return;
  }
  if (remaining > 0) {
    crc = esp_rom_crc32_le(crc, (const uint8_t *)l, strlen(l));
    crc = esp_rom_crc32_le(crc, (const uint8_t *)"\n", 1);
    if (out) out.println(l);
    if (--remaining == 0) finishEvents();
    return;
  }
  if (strcmp(l, "?") == 0) {
    Serial.println("unidex 1");
  } else if (strcmp(l, "C") == 0) {
    char status[96];
    clockStatus(status, sizeof status);
    Serial.printf("OK C %s\n", status);
  } else if (strcmp(l, "S") == 0) {
    storageCardTest();
  } else if (strcmp(l, "L") == 0) {
    listBadges();
  } else if (l[0] == 'B' && l[1] == ' ') {
    long bytes = 0;
    unsigned long crc32 = 0;
    char name[40] = "";
    abortBadge();  // a new upload replaces any unfinished one
    bool ok = sscanf(l + 2, "%39s %ld %lu", name, &bytes, &crc32) == 3 && validBadgeName(name) &&
              bytes > 0 && bytes <= MAX_BADGE_BYTES;
    if (ok) badgeOut = storageOpen(BADGE_TMP, "w");
    if (!ok || !badgeOut) {
      Serial.println("ERR");
      return;
    }
    strlcpy(badgeName, name, sizeof badgeName);
    badgeLeft = bytes;
    badgeCrc = 0;
    badgeExpected = crc32;
    Serial.println("OK B");
  } else if (l[0] == 'T' && l[1] == ' ') {
    clockSet((time_t)strtoll(l + 2, nullptr, 10));
    Serial.println("OK T");
  } else if (l[0] == 'E' && l[1] == ' ') {
    char *end;
    remaining = strtol(l + 2, &end, 10);
    expectedCrc = strtoul(end, nullptr, 10);
    crc = 0;
    unchanged = (uint32_t)storageGetInt("events_crc", 0) == expectedCrc && storageExists(EVENTS);
    if (!unchanged) out = storageOpen(EVENTS_TMP, "w");  // one file, written once, then renamed
    if (remaining <= 0) {
      remaining = 0;
      finishEvents();
    }
  }
}

void usbSyncPoll() {
  if (remaining > 0 && millis() - lastLineAt > STALL_MS) {
    remaining = 0;
    if (out) out.close();
    storageRemove(EVENTS_TMP);
  }
  if (badgeLeft > 0 && millis() - lastLineAt > STALL_MS) abortBadge();
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\r') continue;
    if (c != '\n') {
      if (len < sizeof line - 1) line[len++] = c;
      continue;
    }
    line[len] = 0;
    len = 0;
    lastLineAt = millis();
    powerActivity();  // don't fall asleep mid-transfer
    handle(line);
  }
}

uint32_t usbSyncGeneration() {
  return generation;
}

time_t usbSyncLastTime() {
  return lastSync;
}

bool usbSyncTakeNewBadge(char *out, size_t len) {
  if (!*newBadge) return false;
  strlcpy(out, newBadge, len);
  *newBadge = 0;
  return true;
}
