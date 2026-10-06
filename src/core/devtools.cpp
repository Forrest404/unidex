#if UNIDEX_DEV
#include "devtools.h"
#include <Arduino.h>
#include <esp_heap_caps.h>
#include <esp_rom_crc.h>
#include <esp_system.h>
#include <sys/time.h>
#include "battery.h"
#include "clock.h"
#include "display.h"
#include "input.h"
#include "launcher.h"
#include "power.h"

static bool dryRun, fakeCloud, netFail, noCard, noPush, manualFrames;
static uint32_t seed;
static void (*frameStepper)(int, const char *);
static const char *(*detailFn)();

bool devDryRun() { return dryRun; }
bool devFakeCloud() { return fakeCloud; }
bool devNetFail() { return netFail; }
bool devNoCard() { return noCard; }
bool devNoPush() { return noPush; }
uint32_t devSeed() { return seed; }
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
  for (int row = 0; row < 50; row++) {  // 100 bytes = 4 pixel rows per line
    for (int i = 0; i < 100; i++) sprintf(hex + 2 * i, "%02x", display.shadow[row * 100 + i]);
    Serial.printf("XD %s\n", hex);
  }
  Serial.printf("OK X SHOT %lu\n", (unsigned long)esp_rom_crc32_le(0, display.shadow, sizeof display.shadow));
}

static void press(Event e) {
  const uint32_t count = displayLastRefresh().count;
  powerActivity();
  launcherHandle(e);
  powerActivity();
  const DisplayRefresh r = displayLastRefresh();
  Serial.printf("OK X BTN %c %lu\n", r.count != count ? r.kind : '-', (unsigned long)r.ms);
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
    Serial.print('\n');
  } else if (strcmp(c, "BATT") == 0) {
    const int mv = batteryMillivolts(), pct = batteryPercent();
    Serial.printf("OK X BATT mv=%d pct=%d usb=%d up=%lu\n", mv, pct, (int)batteryCharging(),
                  (unsigned long)(millis() / 1000));
  } else if (strcmp(c, "MEM") == 0) {
    Serial.printf("OK X MEM internal=%u block=%u psram=%u\n", heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                  heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL), heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
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
        display.fillRect(10 + (at * 16) % 140, 60, 40, 40, GxEPD_BLACK);
        display.setCursor(10, 150);
        display.print(at);
      });
      total += millis() - t0, busy += display.epd2.busyMs;
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
  } else {
    Serial.println("ERR");
  }
  return true;  // (usbSyncPoll flushes the reply)
}
#endif
