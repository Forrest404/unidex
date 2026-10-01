#include "display.h"
#include <SPI.h>
#include "storage.h"

static const int PIN_SCK = 12, PIN_MOSI = 13, PIN_CS = 11, PIN_DC = 10, PIN_RST = 9, PIN_BUSY = 8;
static const int PIN_EPD_PWR = 6;  // panel power switch, active LOW

Display display(GxEPD2_154_D67(PIN_CS, PIN_DC, PIN_RST, PIN_BUSY));

void displayInit(bool initial) {
  pinMode(PIN_EPD_PWR, OUTPUT);
  digitalWrite(PIN_EPD_PWR, LOW);
  if (initial) delay(10);  // let the panel rail settle; after a wake it stayed powered

  // Must come before display.init(): GxEPD2 calls SPI.begin() with default pins,
  // which is a no-op once SPI is already started, so these pins stick.
  SPI.begin(PIN_SCK, -1, PIN_MOSI, PIN_CS);
  display.init(0, initial);  // no GxEPD2 timing logs: they'd clutter the USB sync line
  display.inverted = storageGetInt("invert", 0);
}

void displaySetInverted(bool on) {
  display.inverted = on;
  storagePutInt("invert", on);
}

static const int FULL_EVERY = 10;
RTC_DATA_ATTR static int partialsSinceFull;  // survives sleep, so the count is honest

static void render(void (*draw)(), bool full) {
  if (full) display.setFullWindow();
  else display.setPartialWindow(0, 0, display.width(), display.height());
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
    draw();
  } while (display.nextPage());
}

void displayShow(void (*draw)(), bool full) {
  full = full || partialsSinceFull >= FULL_EVERY;
  partialsSinceFull = full ? 0 : partialsSinceFull + 1;
  render(draw, full);
  display.hibernate();  // hibernated e-ink keeps the image
}

void displayFrame(void (*draw)()) {
  render(draw, false);
}

void displayTick(void (*draw)()) {
  render(draw, false);
  display.hibernate();
}
