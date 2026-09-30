// STEP 3: show each button event and the wake count; deep sleep after 10 s idle.
#include <Arduino.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans18pt7b.h>
#include "core/display.h"
#include "core/input.h"
#include "core/power.h"

RTC_DATA_ATTR static uint32_t wakeCount;  // survives deep sleep, reset on power loss

static const char *eventName(Event e) {
  switch (e) {
    case Event::AShort: return "A short";
    case Event::ALong:  return "A long";
    case Event::BShort: return "B short";
    case Event::BLong:  return "B long";
    default:            return "";
  }
}

static void drawCentered(const char *text, int16_t cy) {
  int16_t x, y;
  uint16_t w, h;
  display.getTextBounds(text, 0, 0, &x, &y, &w, &h);
  display.setCursor((display.width() - w) / 2 - x, cy - h / 2 - y);
  display.print(text);
}

void setup() {
  powerInit();
  bool woke = powerWokeFromSleep();
  if (woke) wakeCount++;

#if DEBUG
  Serial.begin(115200);
  // Native USB needs a moment to enumerate. Skipped after a wake: the port dropped during sleep anyway.
  uint32_t t0 = millis();
  while (!woke && !Serial && millis() - t0 < 3000) delay(10);
#endif

  inputInit();
  displayInit(!woke);
  display.setTextColor(GxEPD_BLACK);

  if (!woke) {
    display.setFullWindow();
    display.fillScreen(GxEPD_WHITE);
    display.setFont(&FreeSans9pt7b);
    drawCentered("sleep test", 16);
    display.display();
    display.hibernate();  // hibernated e-ink keeps the image
  }
}

void loop() {
  Event e = inputPoll();
  if (e != Event::None) {
    powerActivity();
#if DEBUG
    Serial.printf("%s, wake %lu\n", eventName(e), (unsigned long)wakeCount);
#endif
    char wakes[16];
    snprintf(wakes, sizeof wakes, "wake %lu", (unsigned long)wakeCount);
    // Partial window x/width must be multiples of 8 on this controller.
    display.setPartialWindow(0, 72, display.width(), 72);
    display.firstPage();
    do {
      display.fillScreen(GxEPD_WHITE);
      display.setFont(&FreeSans18pt7b);
      drawCentered(eventName(e), 96);
      display.setFont(&FreeSans9pt7b);
      drawCentered(wakes, 130);
    } while (display.nextPage());
    display.hibernate();
  }
  powerSleepIfIdle();
  delay(5);
}
