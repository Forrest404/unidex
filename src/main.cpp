#include <Arduino.h>
#include "core/clock.h"
#include "core/display.h"
#include "core/input.h"
#include "core/launcher.h"
#include "core/power.h"
#include "core/storage.h"
#include "core/usbsync.h"

void setup() {
  powerInit();
  bool woke = powerWokeFromSleep();

  Serial.setRxBufferSize(4096);  // a whole calendar sync fits, even if a screen refresh blocks the loop
  Serial.begin(115200);
#if DEBUG
  // Native USB needs a moment to enumerate. Skipped after a wake: the port dropped during sleep anyway.
  uint32_t t0 = millis();
  while (!woke && !Serial && millis() - t0 < 3000) delay(10);
#endif

  clockBegin();  // local time for every app (one I2C read)
  inputInit();
  displayInit(!woke);
  if (!storageInit()) {
#if DEBUG
    Serial.println("filesystem not mounted: run pio run -t uploadfs");
#endif
  }
  launcherBegin(woke);
}

void loop() {
  Event e = inputPoll();
  if (e != Event::None) {
    powerActivity();
    launcherHandle(e);
  }
  usbSyncPoll();
  powerSleepIfIdle();
  powerNap();
}
