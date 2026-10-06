// Chooser: pick 2-6 squares, spin, land on a random winner. A = more squares, B = spin, hold B = tally
// (wins per square number, over every spin; hold B there clears it), hold A = back.
#include <bootloader_random.h>
#include <math.h>
#include "../../core/app.h"
#include "../../core/devtools.h"
#include "../../core/display.h"
#include "../../core/launcher.h"
#include "../../core/power.h"
#include "../../core/storage.h"
#include "../../core/theme.h"

static const int MIN_N = 2, MAX_N = 6, MIN_HOPS = 6;
static const int16_t GAP = 4, GRID_TOP = CONTENT_TOP + 6, TEXT_Y = HINTS_TOP - 14;

enum Screen : uint8_t { COUNT, RESULT, TALLY, CLEAR };
RTC_DATA_ATTR static uint8_t screen, returnTo, n = MIN_N;
RTC_DATA_ATTR static int8_t winner = -1, winnerOf;  // the last result, and how many squares it was out of
static int8_t highlight = -1;  // the cell lit during the spin

static void tallyKey(char *buf, size_t len, int square) { snprintf(buf, len, "ch_w%d", square + 1); }

// `count` square cells in reading order, centred between the header and the text line.
static void drawGrid(int count, int8_t inverted, bool showTally) {
  const int cols = ceil(sqrt(count)), rows = (count + cols - 1) / cols;
  const int16_t areaW = display.width() - 2 * MARGIN, areaH = TEXT_Y - 16 - GRID_TOP;
  const int16_t cell = min((areaW + GAP) / cols, (areaH + GAP) / rows) - GAP;
  const int16_t x0 = (display.width() - (cols * (cell + GAP) - GAP)) / 2;
  const int16_t y0 = GRID_TOP + (areaH - (rows * (cell + GAP) - GAP)) / 2;
  display.setFont(FONT_SMALL);
  for (int i = 0; i < count; i++) {
    const int16_t x = x0 + (i % cols) * (cell + GAP), y = y0 + (i / cols) * (cell + GAP);
    uint16_t ink = BLACK;
    if (i == inverted) {
      display.fillRect(x, y, cell, cell, BLACK);
      ink = WHITE;
    } else {
      display.drawRect(x, y, cell, cell, BLACK);
      display.drawRect(x + 1, y + 1, cell - 2, cell - 2, BLACK);
    }
    char label[12];
    display.setTextColor(ink);
    if (showTally) {  // the square's number small in the corner, its wins in the middle
      char key[8];
      tallyKey(key, sizeof key, i);
      snprintf(label, sizeof label, "%ld", (long)storageGetInt(key));
      display.setFont(FONT_TINY);
      display.setCursor(x + 5, y + 13);
      display.printf("#%d", i + 1);
      display.setFont(FONT_SMALL);
    } else {
      snprintf(label, sizeof label, "%d", i + 1);
    }
    const String fit = fitText(label, cell - 6);
    int16_t tx, ty;
    uint16_t tw, th;
    display.getTextBounds(fit.c_str(), 0, 0, &tx, &ty, &tw, &th);
    display.setCursor(x + (cell - tw) / 2 - tx, y + (cell - th) / 2 - ty);
    display.print(fit);
  }
  display.setTextColor(BLACK);
}

static void drawSpin() {
  drawHeader("Chooser");
  drawGrid(n, highlight, false);
  display.setFont(FONT_SMALL);
  drawCentered("...", TEXT_Y);
  drawHints("", "", "");
}

static void spin() {
  bootloader_random_enable();  // with the radios off, esp_random() needs this for real entropy
  winner = esp_random() % n;
  const int hops = MIN_HOPS + esp_random() % n;
  bootloader_random_disable();
  winnerOf = n;

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

static void clearTally() {
  if (devDryRun()) {
    launcherToast("Dry run: not cleared");
    return;
  }
  char key[8];
  for (int i = 0; i < MAX_N; i++) {
    tallyKey(key, sizeof key, i);
    storageRemoveKey(key);
  }
  launcherToast("Tally cleared");
}

static void onEnter() { screen = COUNT; }  // keeps the number of squares from last time

static Redraw onButton(Event e) {
  switch (screen) {
    case TALLY:
      if (e != Event::BLong) return Redraw::None;
      screen = CLEAR;
      return Redraw::Partial;
    case CLEAR:
      if (e == Event::BShort) clearTally();
      else if (e != Event::AShort) return Redraw::None;
      screen = TALLY;
      return Redraw::Partial;
    default:
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
}

static Redraw onBack() {
  switch (screen) {
    case COUNT: return Redraw::Exit;
    case TALLY: screen = returnTo; break;
    case CLEAR: screen = TALLY; break;
    default: screen = COUNT; break;
  }
  return Redraw::Partial;
}

static void draw() {
  static const char *SAYINGS[] = {"It's #%d.", "#%d it is.", "#%d wins."};
  char line[24];
  if (screen == TALLY || screen == CLEAR) {
    drawHeader("Tally");
    drawGrid(MAX_N, -1, true);
    snprintf(line, sizeof line, "wins over every spin");
  } else if (screen == RESULT) {
    drawHeader("Chooser");
    drawGrid(n, winner, false);
    snprintf(line, sizeof line, SAYINGS[(winner + n) % 3], winner + 1);
  } else {
    drawHeader("Chooser");
    drawGrid(n, -1, false);
    snprintf(line, sizeof line, "%d squares", n);
  }
  display.setFont(FONT_SMALL);
  drawCentered(line, TEXT_Y);
  if (screen == CLEAR) {
    drawSheet("Clear the tally?", "Wins go back to 0.", "Can't be undone.");
    drawHints("keep", "clear", "");
  } else if (screen == TALLY) {
    drawHints("", "", "clear");
  } else if (screen == RESULT) {
    drawHints("change", "again", "tally");
  } else {
    drawHints("+1", "spin", "tally");
  }
}

static void onExit() {}

static void status(char *out, size_t len) {
  if (winner >= 0) snprintf(out, len, "Last: #%d of %d", winner + 1, winnerOf);
  else snprintf(out, len, "Pick at random");
}

extern const App chooserApp = {"Chooser", ICON_CHOOSER, onEnter, onButton, draw, onExit, onBack, status};
