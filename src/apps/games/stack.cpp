// Stack: a block slides side to side; B drops it on the tower and the overhang is cut off. Rules in stack_logic.h.
#include "stack_logic.h"
#include "games.h"
#include "../../core/display.h"
#include "../../core/theme.h"

static stack::State s;

static const int ROW_H = 14, GROUND = 184, SLIDE_Y = 60;  // the tower scrolls to keep the sliding row at SLIDE_Y
static const int SHOW_FRAMES = 15;  // "Perfect!" and the cut-off piece stay up ~0.7 s after a drop

static void start(uint32_t seed) { stack::reset(s, seed); }

static Step step(bool tapped, bool) {
  if (stack::waiting(s) && !tapped) return Step::Still;  // nothing slides before the first press
  return stack::step(s, tapped) ? Step::Moved : Step::Over;
}

static int score() { return s.score; }

// The top of row `row` on the screen (the sliding block is row score + 1).
static int rowY(int row) {
  const int slide = GROUND - ROW_H * (s.score + 2), scroll = slide < SLIDE_Y ? SLIDE_Y - slide : 0;
  return GROUND - ROW_H * (row + 1) + scroll;
}

// The piece a drop cut off: a dotted outline, shown for one frame.
static void drawDotted(int x, int y, int w, int h) {
  for (int i = 0; i < w; i += 2) display.drawPixel(x + i, y, GxEPD_BLACK), display.drawPixel(x + i, y + h - 1, GxEPD_BLACK);
  for (int i = 0; i < h; i += 2) display.drawPixel(x, y + i, GxEPD_BLACK), display.drawPixel(x + w - 1, y + i, GxEPD_BLACK);
}

static void draw() {
  using namespace stack;
  if (rowY(0) + ROW_H < 198) display.fillRect(0, rowY(0) + ROW_H, SCREEN, 2, GxEPD_BLACK);  // the ground
  for (int row = s.score; row >= 0 && row > s.score - KEEP; row--) {  // placed rows, a white line between
    const int y = rowY(row);
    if (y >= 200) break;
    const Row &r = s.rows[row % KEEP];
    display.fillRect(r.x, y, r.w, ROW_H - 1, GxEPD_BLACK);
  }
  const bool justDropped = s.score && s.since < SHOW_FRAMES;
  if (justDropped && s.cut.w) drawDotted(s.cut.x, rowY(s.score), s.cut.w, ROW_H - 1);
  const int x = s.x / U, y = rowY(s.score + 1);  // the sliding block: an outline, not placed yet
  display.drawRect(x, y, s.w, ROW_H - 1, GxEPD_BLACK);
  display.drawRect(x + 1, y + 1, s.w - 2, ROW_H - 3, GxEPD_BLACK);

  char text[8];
  snprintf(text, sizeof text, "%d", s.score);
  display.setFont(FONT_MEDIUM);
  drawCentered(text, 14);
  if (justDropped && s.perfect) {
    display.setFont(FONT_SMALL);
    drawCentered("Perfect!", 38);
  }
  if (waiting(s)) {  // before the first press
    display.fillRoundRect(28, 76, 144, 48, 6, GxEPD_WHITE);
    display.drawRoundRect(28, 76, 144, 48, 6, GxEPD_BLACK);
    display.setFont(FONT_SMALL);
    drawCentered("Press B to drop", 92);
    display.setFont(FONT_TINY);
    drawCentered("hold A: back", 112);
  }
}

extern const Game stackGame = {"Stack", "g_stack", FRAME_MS, start, step, draw, score};
