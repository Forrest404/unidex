// Chooser: pick 2-6 squares, spin, land on a random winner. A = more/back, B = go/again,
// B long = tally (wins per square number).
#include <bootloader_random.h>
#include <math.h>
#include "../../core/app.h"
#include "../../core/display.h"
#include "../../core/power.h"
#include "../../core/storage.h"
#include "../../core/theme.h"

static const int MIN_N = 2, MAX_N = 6, MIN_HOPS = 6;
static const int16_t GAP = 4, GRID_TOP = CONTENT_TOP + 6, TEXT_Y = CONTENT_BOTTOM - 14;

enum Screen : uint8_t { COUNT, RESULT, TALLY };
RTC_DATA_ATTR static uint8_t screen, returnTo, n = MIN_N;
RTC_DATA_ATTR static int8_t winner;
static int8_t highlight = -1;  // the cell lit during the spin

static void tallyKey(char *buf, size_t len, int square) { snprintf(buf, len, "ch_w%d", square + 1); }

// Square cells in reading order, centred between the header and the text line.
static void drawGrid(int8_t inverted, bool showTally) {
  const int cols = ceil(sqrt(n)), rows = (n + cols - 1) / cols;
  const int16_t areaW = display.width() - 2 * MARGIN, areaH = TEXT_Y - 16 - GRID_TOP;
  const int16_t cell = min((areaW + GAP) / cols, (areaH + GAP) / rows) - GAP;
  const int16_t x0 = (display.width() - (cols * (cell + GAP) - GAP)) / 2;
  const int16_t y0 = GRID_TOP + (areaH - (rows * (cell + GAP) - GAP)) / 2;
  display.setFont(FONT_SMALL);
  for (int i = 0; i < n; i++) {
    const int16_t x = x0 + (i % cols) * (cell + GAP), y = y0 + (i / cols) * (cell + GAP);
    uint16_t ink = GxEPD_BLACK;
    if (i == inverted) {
      display.fillRect(x, y, cell, cell, GxEPD_BLACK);
      ink = GxEPD_WHITE;
    } else {
      display.drawRect(x, y, cell, cell, GxEPD_BLACK);
      display.drawRect(x + 1, y + 1, cell - 2, cell - 2, GxEPD_BLACK);
    }
    char label[8];
    if (showTally) {
      char key[8];
      tallyKey(key, sizeof key, i);
      snprintf(label, sizeof label, "%ld", (long)storageGetInt(key));
    } else {
      snprintf(label, sizeof label, "%d", i + 1);
    }
    int16_t tx, ty;
    uint16_t tw, th;
    display.getTextBounds(label, 0, 0, &tx, &ty, &tw, &th);
    display.setTextColor(ink);
    display.setCursor(x + (cell - tw) / 2 - tx, y + (cell - th) / 2 - ty);
    display.print(label);
  }
  display.setTextColor(GxEPD_BLACK);
}

static void drawSpin() {
  drawHeader("Chooser");
  drawGrid(highlight, false);
  drawFooter("", "");
}

static void spin() {
  bootloader_random_enable();  // with the radios off, esp_random() needs this for real entropy
  winner = esp_random() % n;
  const int hops = MIN_HOPS + esp_random() % n;
  bootloader_random_disable();

  // Worked backwards: start (hops - 1) cells before the winner, so the walk ends on it.
  highlight = ((winner - (hops - 1)) % n + n) % n;
  for (int k = 1; k <= hops; k++) {
    displayFrame(drawSpin);  // ~0.33 s per partial refresh sets the base pace
    float t = (float)k / hops;
    delay(40 + (int)(560 * t * t * t));  // ease-out: each hop a little slower
    highlight = (highlight + 1) % n;
  }
  highlight = -1;

  char key[8];
  tallyKey(key, sizeof key, winner);
  storagePutInt(key, storageGetInt(key) + 1);  // one write per spin, after it lands
  powerActivity();  // the spin took seconds; don't let the idle timer cut in
  screen = RESULT;
}

static void onEnter() {
  n = MIN_N;  // always opens on 2
  screen = COUNT;
}

static Redraw onButton(Event e) {
  if (screen == TALLY) {
    if (e != Event::BShort && e != Event::BLong) return Redraw::None;
    screen = returnTo;
    return Redraw::Partial;
  }
  if (e == Event::BLong) {
    returnTo = screen;
    screen = TALLY;
    return Redraw::Partial;
  }
  if (e == Event::BShort) {
    spin();
    return Redraw::Full;  // the reveal: also clears the spin's ghosting
  }
  if (e == Event::AShort) {
    if (screen == COUNT) n = n >= MAX_N ? MIN_N : n + 1;
    screen = COUNT;
    return Redraw::Partial;
  }
  return Redraw::None;
}

static void draw() {
  char line[24];
  if (screen == TALLY) {
    drawHeader("Tally");
    drawGrid(-1, true);
    snprintf(line, sizeof line, "wins per square");
  } else if (screen == RESULT) {
    drawHeader("Chooser");
    drawGrid(winner, false);
    snprintf(line, sizeof line, "You got #%d", winner + 1);
  } else {
    drawHeader("Chooser");
    drawGrid(-1, false);
    snprintf(line, sizeof line, "%d squares", n);
  }
  display.setFont(FONT_SMALL);
  drawCentered(line, TEXT_Y);
  if (screen == TALLY) drawFooter("", "back");
  else if (screen == RESULT) drawFooter("count", "again");
  else drawFooter("more", "go");
}

static void onExit() {}

extern const App chooserApp = {"Chooser", ICON_CHOOSER, onEnter, onButton, draw, onExit};
