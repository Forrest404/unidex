#include "display.h"
#include <SPI.h>

static const int PIN_SCK = 12, PIN_MOSI = 13, PIN_CS = 11, PIN_DC = 10, PIN_RST = 9, PIN_BUSY = 8;
static const int PIN_EPD_PWR = 6;  // panel power switch, active LOW

GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT> display(
    GxEPD2_154_D67(PIN_CS, PIN_DC, PIN_RST, PIN_BUSY));

void displayInit() {
  pinMode(PIN_EPD_PWR, OUTPUT);
  digitalWrite(PIN_EPD_PWR, LOW);
  delay(10);  // let the panel rail settle before reset

  // Must come before display.init(): GxEPD2 calls SPI.begin() with default pins,
  // which is a no-op once SPI is already started, so these pins stick.
  SPI.begin(PIN_SCK, -1, PIN_MOSI, PIN_CS);
  display.init(DEBUG ? 115200 : 0);
}
