#if UNIDEX_DEV
#include "devtools.h"
#include <Arduino.h>
#include <esp_heap_caps.h>
#include <esp_rom_crc.h>
#include <esp_system.h>
#include <sys/time.h>
#include <Wire.h>
#include "audio.h"
#include "battery.h"
#include "clock.h"
#include "display.h"
#include "input.h"
#include "launcher.h"
#include "power.h"
#include "storage.h"
#include "../apps/notes/store.h"
#include "../apps/timetable/callink.h"

static bool dryRun, fakeCloud, netFail, noCard, noPush, noSave, demo, manualFrames, emptyHeard;
static int failNotes;
static String sweepPrefix;
static uint32_t seed, autoSync;
static void (*frameStepper)(int, const char *);
static const char *(*detailFn)();

bool devDryRun() { return dryRun; }
bool devFakeCloud() { return fakeCloud; }
bool devNetFail() { return netFail; }
bool devNoCard() { return noCard; }
bool devNoPush() { return noPush; }
bool devNoSave() { return noSave; }
bool devEmptyHeard() { return emptyHeard; }
bool devFailNote() { return failNotes > 0 && failNotes--; }
bool devSweepOn() { return fakeCloud && sweepPrefix.length(); }
bool devSweepAllows(const String &id) { return !fakeCloud || (sweepPrefix.length() && id.startsWith(sweepPrefix)); }
bool devDemo() { return demo; }
uint32_t devSeed() { return seed; }
uint32_t devAutoSync() { return autoSync; }
bool devManualFrames() { return manualFrames; }
void devSetFrameStepper(void (*fn)(int, const char *)) { frameStepper = fn; }
void devSetDetail(const char *(*fn)()) { detailFn = fn; }

static void printHex(const char *s) {
  for (; *s; s++) Serial.printf("%02x", (uint8_t)*s);
}

static void shot() {
  const DisplayRefresh r = displayLastRefresh();
  Serial.printf("XS 200 200 %c %lu ", r.kind ? r.kind : '-', (unsigned long)r.ms);
  printHex(launcherScreenName());
  Serial.print('\n');
  char hex[201];
  uint8_t px[100];
  uint32_t crc = 0;
  for (int row = 0; row < 50; row++) {  // 100 bytes = 4 pixel rows per line, 1 = black
    for (int i = 0; i < 100; i++) px[i] = ~display.buf[row * 100 + i], sprintf(hex + 2 * i, "%02x", px[i]);
    crc = esp_rom_crc32_le(crc, px, sizeof px);
    Serial.printf("XD %s\n", hex);
  }
  Serial.printf("OK X SHOT %lu\n", (unsigned long)crc);
}

void devShotDuringAnimation() {
  static char line[16];
  static int n;
  while (Serial.available()) {
    const char c = Serial.read();
    if (c == '\n') {
      line[n] = 0;
      if (strcmp(line, "X SHOT") == 0 || strcmp(line, "X STATE") == 0) devUsb(line);  // safe: they only report
      else if (n) Serial.println("ERR busy");  // anything else would act in the middle of the animation
      if (Serial) Serial.flush();
      n = 0;
    } else if (c != '\r' && n < (int)sizeof line - 1) {
      line[n++] = c;
    }
  }
}

static void press(Event e) {
  const uint32_t count = displayLastRefresh().count;
  powerActivity();
  launcherHandle(e);
  powerActivity();
  const DisplayRefresh r = displayLastRefresh();
  Serial.printf("OK X BTN %c %lu\n", r.count != count ? r.kind : '-', (unsigned long)r.ms);
}

// The battery log: every 5 s the reading, USB and the % shown, for the last hour (X BATTLOG prints it). It keeps
// going with USB unplugged, so a charge/unplug can be read back afterwards.
struct BattEntry {
  uint32_t s;
  int16_t mv;
  int8_t pct;
  uint8_t usb;
};
static BattEntry battLog[720];
static uint32_t battCount, battNext;

void devTick() {
  if (millis() < battNext) return;
  battNext = millis() + 5000;
  battLog[battCount++ % 720] = {millis() / 1000, (int16_t)batteryMillivolts(), (int8_t)batteryPercent(),
                                (uint8_t)HWCDC::isPlugged()};
}

bool devUsb(const char *l) {
  if (l[0] != 'X' || l[1] != ' ') return false;
  const char *c = l + 2;
  if (strcmp(c, "SHOT") == 0) {
    shot();
  } else if (strncmp(c, "BTN ", 4) == 0 && c[4] && !c[5]) {
    const char k = c[4];
    const Event e = k == 'a' ? Event::AShort : k == 'A' ? Event::ALong : k == 'b' ? Event::BShort
                  : k == 'B' ? Event::BLong : k == 'R' ? Event::Reset : Event::None;
    if (e == Event::None) Serial.println("ERR");
    else press(e);
  } else if (strncmp(c, "HOLD B ", 7) == 0) {
    inputVirtualHoldB(strtoul(c + 7, nullptr, 10));
    press(Event::BLong);
  } else if (strcmp(c, "STATE") == 0) {
    Serial.print("OK X STATE screen=");
    printHex(launcherScreenName());
    Serial.print(" sel=");
    printHex(launcherSelectedName());
    Serial.printf(" clock=%d dry=%d fake=%d netfail=%d nocard=%d nopush=%d reset=%d up=%lu refreshes=%lu lastms=%lu detail=",
                  clockValid(), dryRun, fakeCloud, netFail, noCard, noPush, (int)esp_reset_reason(),
                  (unsigned long)(millis() / 1000), (unsigned long)displayLastRefresh().count,
                  (unsigned long)displayLastRefresh().ms);
    printHex(detailFn ? detailFn() : "");
    Serial.printf(" flat=%d toast=", (int)powerFlatShown());
    printHex(launcherToastText());
    Serial.print('\n');
  } else if (strcmp(c, "BATT") == 0) {
    const int mv = batteryMillivolts(), pct = batteryPercent();
    Serial.printf("OK X BATT mv=%d pct=%d usb=%d up=%lu kept=%d\n", mv, pct, (int)batteryCharging(),
                  (unsigned long)(millis() / 1000), (int)batteryKept());
  } else if (strcmp(c, "BATTLOG") == 0) {  // X BATTLOG: "BL <s> <mv> <usb> <pct>" lines, oldest first
    for (uint32_t i = battCount > 720 ? battCount - 720 : 0; i < battCount; i++) {
      const BattEntry &e = battLog[i % 720];
      Serial.printf("BL %lu %d %d %d\n", (unsigned long)e.s, e.mv, e.usb, e.pct);
    }
    Serial.println("OK X BATTLOG");
  } else if (strcmp(c, "BATTCALLS") == 0) {  // X BATTCALLS: every % worked out lately, with what it saw
    batteryPrintCalls();
    Serial.println("OK X BATTCALLS");
  } else if (strcmp(c, "I2C") == 0) {  // the clock chip's bus: which addresses answer
    Serial.printf("OK X I2C sda=%d scl=%d", digitalRead(47), digitalRead(48));  // both 1 when the bus is free
    for (uint8_t a = 1; a < 127; a++) {
      Wire.beginTransmission(a);
      const uint8_t err = Wire.endTransmission();
      if (err == 0) Serial.printf(" %02x", a);
      if (err == 5) {  // a timeout: the bus is held, so every other address would wait as long
        Serial.print(" timeout");
        break;
      }
    }
    Serial.print('\n');
  } else if (strcmp(c, "BEEP") == 0) {
    Serial.println(audioChime() ? "OK X BEEP" : "ERR codec");
  } else if (strcmp(c, "MEM") == 0) {
    Serial.printf("OK X MEM internal=%u block=%u psram=%u psblock=%u\n", heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                  heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL), heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
                  heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
  } else if (strncmp(c, "SLEEP ", 6) == 0) {
    Serial.println("OK X SLEEP");
    Serial.flush();
    delay(50);
    powerSleepFor(strtoul(c + 6, nullptr, 10));  // doesn't return: the board boots again after the timer
  } else if (strncmp(c, "MANUAL ", 7) == 0) {
    manualFrames = c[7] == '1';
    Serial.println("OK X MANUAL");
  } else if (strncmp(c, "SEED ", 5) == 0) {
    seed = strtoul(c + 5, nullptr, 10);
    Serial.println("OK X SEED");
  } else if (strncmp(c, "FRAMES ", 7) == 0) {
    if (frameStepper) frameStepper(atoi(c + 7), nullptr);
    powerActivity();
    Serial.println("OK X FRAMES");
  } else if (strncmp(c, "PLAY ", 5) == 0) {
    if (frameStepper) frameStepper(strlen(c + 5), c + 5);
    powerActivity();
    Serial.println("OK X PLAY");
  } else if (strncmp(c, "FILL ", 5) == 0) {  // X FILL b|w: the whole screen black or white, with a full refresh
    static bool black;
    black = c[5] == 'b';
    displayShow([] { display.fillScreen(black ? BLACK : WHITE); }, true);
    Serial.printf("OK X FILL %c %lu\n", c[5], (unsigned long)displayLastRefresh().busyMs);
  } else if (strncmp(c, "GAMEWAVE ", 9) == 0) {  // X GAMEWAVE <frames> [rate]: the games' waveform, next round on
    int frames = 0, rate = 0;
    sscanf(c + 9, "%d %d", &frames, &rate);
    displaySetGameFrames(frames, rate);
    Serial.printf("OK X GAMEWAVE %d %d\n", frames, rate);
  } else if (strncmp(c, "FTEST ", 6) == 0) {  // X FTEST <SPI MHz> <fast frames, 0 = panel's own> <rate>
    int mhz = 4, frames = 0, rate = 2;
    sscanf(c + 6, "%d %d %d", &mhz, &frames, &rate);
    displaySetSpiHz(mhz * 1000000UL);
    displayFastWave(frames, rate);
    uint32_t total = 0, busy = 0;
    static int at;
    for (int i = 0; i < 20; i++) {  // a moving box and a counter, like a game frame
      at = i;
      const uint32_t t0 = millis();
      displayFrame([] {
        display.fillRect(10 + (at * 16) % 140, 60, 40, 40, BLACK);
        display.setCursor(10, 150);
        display.print(at);
      });
      total += millis() - t0, busy += displayLastRefresh().busyMs;
    }
    displayFastFrames(false);
    Serial.printf("OK X FTEST ms=%lu busy=%lu\n", (unsigned long)(total / 20), (unsigned long)(busy / 20));
  } else if (strcmp(c, "CLOCK UNSET") == 0) {
    const timeval tv = {0, 0};
    settimeofday(&tv, nullptr);
    Serial.println("OK X CLOCK");
  } else if (!strncmp(c, "DRY ", 4) || !strncmp(c, "FAKE ", 5) || !strncmp(c, "NETFAIL ", 8) ||
             !strncmp(c, "NOCARD ", 7) || !strncmp(c, "NOPUSH ", 7)) {
    const bool on = l[strlen(l) - 1] == '1';
    (c[0] == 'D' ? dryRun : c[0] == 'F' ? fakeCloud : c[1] == 'E' ? netFail : c[2] == 'C' ? noCard : noPush) = on;
    Serial.printf("OK X %s\n", c);
  } else if (!strncmp(c, "SWEEPONLY", 9)) {  // X SWEEPONLY <prefix>: fake syncs touch only these notes
    sweepPrefix = c[9] == ' ' ? String(c + 10) : String();
    Serial.printf("OK X %s\n", c);
  } else if (!strncmp(c, "FAILNOTE ", 9)) {
    failNotes = atoi(c + 9);
    Serial.printf("OK X %s\n", c);
  } else if (!strncmp(c, "AUTOSYNC ", 9)) {  // X AUTOSYNC <s>: Notes' automatic sync interval (0 = the usual)
    autoSync = atoi(c + 9);
    Serial.printf("OK X %s\n", c);
  } else if (!strncmp(c, "NOSAVE ", 7)) {  // X NOSAVE <0|1>: Notes can't write a note's text (as on a full card)
    noSave = l[strlen(l) - 1] == '1';
    Serial.printf("OK X %s\n", c);
  } else if (!strncmp(c, "EMPTY ", 6)) {  // X EMPTY <0|1>: the canned transcription hears nothing
    emptyHeard = l[strlen(l) - 1] == '1';
    Serial.printf("OK X %s\n", c);
  } else if (!strncmp(c, "WELCOME", 7)) {  // X WELCOME: the first-start screens, without wiping anything
    launcherShowWelcome();
    Serial.println("OK X WELCOME");
  } else if (!strncmp(c, "NOTES ", 6)) {  // X NOTES <n>: n made-up notes (test-0001 ...) for long lists; 0 removes them
    const int n = atoi(c + 6);
    for (int i = 1; i <= 999; i++) {
      char id[16];
      snprintf(id, sizeof id, "test-%04d", i);
      if (i <= n) storeSaveNote(id, String("---\ntitle: \"Test ") + i + "\"\n---\n\nA made-up note.\n");
      else if (storeReadNote(id).length()) storeDelete(id);
      powerAlive();  // hundreds of files take longer than the watchdog's 30 s
    }
    Serial.printf("OK X NOTES %d\n", n);
  } else if (!strcmp(c, "CALSYNC")) {  // X CALSYNC: what Settings > Sync does (clock, then the calendar link)
    const SyncResult r = calendarSync();
    Serial.printf("OK X CALSYNC clock=%s link=%d calendar=%s events=%d\n", r.clock ? r.clock : "ok", r.link,
                  r.calendar ? r.calendar : "ok", r.events);
  } else if (!strcmp(c, "HANG")) {  // X HANG: the main loop stops; the watchdog should restart the device in 30 s
    Serial.println("OK X HANG");
    Serial.flush();
    for (;;) delay(100);
  } else if (!strncmp(c, "BATTMV ", 7)) {  // X BATTMV <mv>: pretend the battery reads this (0 = real)
    batteryFake(atoi(c + 7));
    Serial.printf("OK X BATTMV %d\n", atoi(c + 7));
  } else if (!strncmp(c, "KEY ", 4)) {
    char name[16] = "", value[16] = "";
    if (sscanf(c + 4, "%15s %15s", name, value) == 2) {
      if (strcmp(value, "none") == 0) storageRemoveKey(name);
      else storagePutInt(name, strtol(value, nullptr, 10));
    }
    const int32_t v = storageGetInt(name, INT32_MIN);  // INT32_MIN: not set
    if (v == INT32_MIN) Serial.printf("OK X KEY %s none\n", name);
    else Serial.printf("OK X KEY %s %ld\n", name, (long)v);
  } else if (!strncmp(c, "DEMO ", 5)) {
    demo = c[5] == '1';
    Serial.printf("OK X %s\n", c);
  } else {
    Serial.println("ERR");
  }
  return true;  // (usbSyncPoll flushes the reply)
}
#endif
