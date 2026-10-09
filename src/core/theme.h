#pragma once
#include <Adafruit_GFX.h>

// Shared look for every screen: fixed margins, a thin header, the two-row button hints at the bottom.
extern const GFXfont *const FONT_SMALL;  // header, footer, secondary text
extern const GFXfont *const FONT_LARGE;  // the one focal element
extern const GFXfont *const FONT_MEDIUM; // app names on the home screen (12 pt)
extern const GFXfont *const FONT_BOLD;   // headlines of empty states and confirm sheets (9 pt bold)
extern const GFXfont *const FONT_TINY;   // the "hold" row of the hints (7 pt)

const int16_t MARGIN = 8;
const int16_t HEADER_H = 24;  // title + 1 px rule
const int16_t CONTENT_TOP = HEADER_H;
const int16_t HINTS_TOP = 163;  // drawHints' rule: screens using it end their content above this

void drawHeader(const char *title, const char *right = nullptr);  // right: a count, page or time

// The button hints: what A and B do, and below on the right a hold-B extra if the screen has one. "" leaves one
// out. Hold A always goes back, so it isn't shown.
//   A next              B open
//                       hold B: delete
void drawHints(const char *a, const char *b, const char *bHold);
void drawCentered(const char *text, int16_t cy);        // centred horizontally, cy = vertical centre
void drawCenteredLine(const char *text, int16_t baseline);  // centred horizontally on a fixed baseline
void drawRight(const char *text, int16_t baseline);     // right-aligned to the margin
String fitText(const char *text, int16_t maxWidth);     // cut to fit the current font, ending in "..." if cut
int16_t textWidth(const char *text);                    // in the current font
// Word-wraps text to maxW in the current font into up to maxLines lines (the last ends in "..." if the text
// didn't fit); returns how many lines the whole text needs.
int wrapText(const char *text, int16_t maxW, String *out, int maxLines);

void drawEmpty(const char *headline, const char *line1, const char *line2 = "");  // centred in the content
void drawSheet(const char *title, const char *line1, const char *line2 = "",
               const char *line3 = "");  // a "sure?" box over the screen; each line fits about 164 px
void drawProgress(int16_t cy, int pct);    // a bar; pct < 0 = a block that moves while waiting
void drawPageDots(int count, int current, int16_t cy);
void drawToast(const char *text);          // a short message in a black pill just above the hints

// 40x40 launcher icons, one string per row, '#' = ink.
const int16_t ICON_SIZE = 40;
extern const char *const ICON_TIMETABLE[], *const ICON_BADGE[], *const ICON_DEX[], *const ICON_CHOOSER[],
    *const ICON_NOTES[], *const ICON_SETTINGS[], *const ICON_GAMES[], *const ICON_PET[];
void drawIcon(const char *const *icon, int16_t x, int16_t y, uint16_t color, int scale = 1);  // scale 2 = 80x80
