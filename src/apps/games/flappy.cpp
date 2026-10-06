// Flappy: B flaps, the bird falls between flaps; fly through the gaps. The rules are in flappy_logic.h.
#include "flappy_logic.h"
#include "games.h"
#include "../../core/display.h"
#include "../../core/theme.h"

static flappy::State s;

static const uint8_t BIRD[] = {  // 14x12, 1 = black
  0x00, 0x00, 0x0F, 0x00, 0x3F, 0xC0, 0x7E, 0x60, 0xFE, 0x70, 0xFF, 0xF0,
  0xCF, 0xFC, 0xF3, 0xF0, 0xFF, 0xF0, 0x7F, 0xE0, 0x3F, 0xC0, 0x0F, 0x00,
};

static void start(uint32_t seed) { flappy::reset(s, seed); }

static Step step(bool tapped, bool) {
  if (flappy::waiting(s) && !tapped) return Step::Still;  // nothing moves before the first flap
  return flappy::step(s, tapped) ? Step::Moved : Step::Over;
}

static int score() { return s.score; }

// A pipe: a 2 px outline, with a wider lip at the open end.
static void drawPipe(int x, int y0, int y1, bool lipAtBottom) {
  using namespace flappy;
  if (y1 <= y0) return;
  display.fillRect(x, y0, PIPE_W, y1 - y0, WHITE);
  display.drawRect(x, y0, PIPE_W, y1 - y0, BLACK);
  display.drawRect(x + 1, y0, PIPE_W - 2, y1 - y0, BLACK);
  const int lipY = lipAtBottom ? y1 - 8 : y0;
  display.fillRect(x - LIP, lipY, PIPE_W + 2 * LIP, 8, BLACK);
}

static void draw() {
  using namespace flappy;
  for (const Pipe &p : s.pipes) {
    const int x = p.x / U;
    if (x > 200 || x + PIPE_W < -LIP) continue;
    drawPipe(x, 0, p.gapY - GAP / 2, true);
    drawPipe(x, p.gapY + GAP / 2, GROUND, false);
  }
  // Ground: a line with marks that move with the pipes, so it reads as flying forward.
  display.fillRect(0, GROUND, 200, 2, BLACK);
  const int shift = (s.pipes[0].x / U % 16 + 16) % 16;
  for (int x = shift - 16; x < 200; x += 16) display.drawLine(x, GROUND + 14, x + 8, GROUND + 3, BLACK);
  display.drawBitmap(BIRD_X, s.y / U, BIRD, BIRD_W, BIRD_H, BLACK);

  char text[8];
  snprintf(text, sizeof text, "%d", s.score);
  display.setFont(FONT_MEDIUM);
  const int16_t w = textWidth(text) + 12;
  display.fillRoundRect((200 - w) / 2, 2, w, 24, 6, WHITE);
  drawCentered(text, 14);
  if (!s.started) {  // before the first flap
    display.fillRoundRect(28, 120, 144, 48, 6, WHITE);
    display.drawRoundRect(28, 120, 144, 48, 6, BLACK);
    display.setFont(FONT_SMALL);
    drawCentered("Press B to flap", 136);
    display.setFont(FONT_TINY);
    drawCentered("hold A: back", 156);
  }
}

extern const Game flappyGame = {"Flappy", "g_flap", FRAME_MS, start, step, draw, score};
