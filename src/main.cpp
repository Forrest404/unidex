// STEP 1: draw "hello" once, then hibernate the panel.
#include <Arduino.h>
#include <Fonts/FreeSans18pt7b.h>
#include "core/display.h"

static const int PIN_LATCH = 17;      // battery power latch: HIGH keeps the board on
static const int PIN_AUDIO_PWR = 42;  // audio amp power, active LOW

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

  displayInit();

  const char *text = "hello";
  display.setFont(&FreeSans18pt7b);
  display.setTextColor(GxEPD_BLACK);
  int16_t x, y;
  uint16_t w, h;
  display.getTextBounds(text, 0, 0, &x, &y, &w, &h);

  display.setFullWindow();
  display.fillScreen(GxEPD_WHITE);
  display.drawRect(0, 0, display.width(), display.height(), GxEPD_BLACK);  // shows panel edges
  display.setCursor((display.width() - w) / 2 - x, (display.height() - h) / 2 - y);
  display.print(text);
  display.display();

  // Hibernated e-ink draws no power and keeps the image.
  display.hibernate();
#if DEBUG
  Serial.println("drawn");
#endif
}

void loop() {}
