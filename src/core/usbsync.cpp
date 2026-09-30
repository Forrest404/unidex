#include "usbsync.h"
#include <Arduino.h>
#include <esp_rom_crc.h>
#include "clock.h"
#include "power.h"
#include "storage.h"

// Protocol (text lines, Mac -> device):
//   ?                      -> "unidex 1"
//   T <unix seconds>       -> sets the clock, "OK T"
//   E <count> <crc32>      then <count> lines "YYYY-MM-DD,HH:MM,HH:MM,title,location"
//                          -> "OK E <crc>" or "ERR"; crc32 (zlib) covers each line plus '\n'
static const char *EVENTS = "/events.csv", *EVENTS_TMP = "/events.tmp";
static const uint32_t STALL_MS = 3000;  // give up on a transfer that stops halfway

static char line[160];
static size_t len;
static int remaining;              // event lines still to come
static uint32_t expectedCrc, crc, lastLineAt, generation;
static bool unchanged;             // same crc as the saved file: check it, but don't rewrite flash
static fs::File out;

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
  Serial.printf("OK E %lu\n", (unsigned long)crc);
}

static void handle(const char *l) {
  if (remaining > 0) {
    crc = esp_rom_crc32_le(crc, (const uint8_t *)l, strlen(l));
    crc = esp_rom_crc32_le(crc, (const uint8_t *)"\n", 1);
    if (out) out.println(l);
    if (--remaining == 0) finishEvents();
    return;
  }
  if (strcmp(l, "?") == 0) {
    Serial.println("unidex 1");
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
