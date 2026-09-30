// STEP 4: read a file from LittleFS, keep a counter in NVS, draw with the theme.
#include <Arduino.h>
#include "core/display.h"
#include "core/input.h"
#include "core/power.h"
#include "core/storage.h"
#include "core/theme.h"

static void drawCount() {
  char text[24];
  snprintf(text, sizeof text, "saved: %ld", (long)storageGetInt("test_count"));
  display.setFont(FONT_LARGE);
  drawCentered(text, 130);
}

void setup() {
  powerInit();
  bool woke = powerWokeFromSleep();

#if DEBUG
  Serial.begin(115200);
  // Native USB needs a moment to enumerate. Skipped after a wake: the port dropped during sleep anyway.
  uint32_t t0 = millis();
  while (!woke && !Serial && millis() - t0 < 3000) delay(10);
#endif

  inputInit();
  displayInit(!woke);
  display.setTextColor(GxEPD_BLACK);
  bool fsOk = storageInit();
  if (woke) return;  // the panel still shows the last screen

  String line = "no filesystem";
  if (fsOk) {
    fs::File f = storageOpen("/hello.txt");
    line = f ? f.readStringUntil('\n') : "no /hello.txt";
    f.close();
  }
#if DEBUG
  Serial.println(line);
#endif

  display.setFullWindow();
  display.fillScreen(GxEPD_WHITE);
  drawHeader("storage");
  display.setFont(FONT_SMALL);
  drawCentered(line.c_str(), 70);
  drawCount();
  drawFooter("", "+1");
  display.display();
  display.hibernate();  // hibernated e-ink keeps the image
}

void loop() {
  Event e = inputPoll();
  if (e != Event::None) powerActivity();
  if (e == Event::BShort) {
    storagePutInt("test_count", storageGetInt("test_count") + 1);
    // Partial window x/width must be multiples of 8 on this controller.
    display.setPartialWindow(0, 104, display.width(), 56);
    display.firstPage();
    do {
      display.fillScreen(GxEPD_WHITE);
      drawCount();
    } while (display.nextPage());
    display.hibernate();
  }
  powerSleepIfIdle();
  delay(5);
}
