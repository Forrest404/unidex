// STEP 2: show each button event on screen.
#include <Arduino.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans18pt7b.h>
#include "core/display.h"
#include "core/input.h"

static const int PIN_LATCH = 17;      // battery power latch: HIGH keeps the board on
static const int PIN_AUDIO_PWR = 42;  // audio amp power, active LOW

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
  pinMode(PIN_LATCH, OUTPUT);
  digitalWrite(PIN_LATCH, HIGH);
  pinMode(PIN_AUDIO_PWR, OUTPUT);
  digitalWrite(PIN_AUDIO_PWR, HIGH);

#if DEBUG
  Serial.begin(115200);
  uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 3000) delay(10);  // native USB needs a moment to enumerate
#endif

  inputInit();
  displayInit();

  display.setTextColor(GxEPD_BLACK);
  display.setFullWindow();
  display.fillScreen(GxEPD_WHITE);
  display.setFont(&FreeSans9pt7b);
  drawCentered("input test", 16);
  display.display();
  display.hibernate();  // hibernated e-ink draws no power and keeps the image
}

void loop() {
  Event e = inputPoll();
  if (e != Event::None) {
#if DEBUG
    Serial.println(eventName(e));
#endif
    // Partial window x/width must be multiples of 8 on this controller.
    display.setPartialWindow(0, 80, display.width(), 40);
    display.firstPage();
    do {
      display.fillScreen(GxEPD_WHITE);
      display.setFont(&FreeSans18pt7b);
      drawCentered(eventName(e), 100);
    } while (display.nextPage());
    display.hibernate();
  }
  delay(5);
}
