#include "theme.h"
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans18pt7b.h>
#include "display.h"

const GFXfont *const FONT_SMALL = &FreeSans9pt7b;
const GFXfont *const FONT_LARGE = &FreeSans18pt7b;

void drawCentered(const char *text, int16_t cy) {
  int16_t x, y;
  uint16_t w, h;
  display.getTextBounds(text, 0, 0, &x, &y, &w, &h);
  display.setCursor((display.width() - w) / 2 - x, cy - h / 2 - y);
  display.print(text);
}

void drawHeader(const char *title) {
  display.setFont(FONT_SMALL);
  display.setCursor(MARGIN, 16);
  display.print(title);
  display.drawFastHLine(MARGIN, HEADER_H - 1, display.width() - 2 * MARGIN, GxEPD_BLACK);
}

void drawFooter(const char *aHint, const char *bHint) {
  const int16_t baseline = display.height() - 6;
  display.setFont(FONT_SMALL);
  display.drawFastHLine(MARGIN, CONTENT_BOTTOM, display.width() - 2 * MARGIN, GxEPD_BLACK);
  if (*aHint) {
    display.setCursor(MARGIN, baseline);
    display.printf("A %s", aHint);
  }
  if (*bHint) {
    char b[24];
    snprintf(b, sizeof b, "B %s", bHint);
    int16_t x, y;
    uint16_t w, h;
    display.getTextBounds(b, 0, 0, &x, &y, &w, &h);
    display.setCursor(display.width() - MARGIN - w - x, baseline);
    display.print(b);
  }
}
