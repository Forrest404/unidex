#pragma once
#include <Adafruit_GFX.h>

// Shared look for every screen: two font sizes, fixed margins, thin header and footer.
extern const GFXfont *const FONT_SMALL;  // header, footer, secondary text
extern const GFXfont *const FONT_LARGE;  // the one focal element

const int16_t MARGIN = 8;
const int16_t HEADER_H = 24;  // title + 1 px rule
const int16_t FOOTER_H = 22;  // 1 px rule + button hints
const int16_t CONTENT_TOP = HEADER_H;
const int16_t CONTENT_BOTTOM = 200 - FOOTER_H;

void drawHeader(const char *title);
void drawFooter(const char *aHint, const char *bHint);  // e.g. "next", "select"; "" hides one
void drawCentered(const char *text, int16_t cy);        // centred horizontally, cy = vertical centre
void drawRight(const char *text, int16_t baseline);     // right-aligned to the margin
String fitText(const char *text, int16_t maxWidth);     // cut from the end to fit the current font

// 40x40 launcher icons, one string per row, '#' = ink.
const int16_t ICON_SIZE = 40;
extern const char *const ICON_TIMETABLE[], *const ICON_BADGE[], *const ICON_DEX[], *const ICON_CHOOSER[];
void drawIcon(const char *const *icon, int16_t x, int16_t y, uint16_t color);
