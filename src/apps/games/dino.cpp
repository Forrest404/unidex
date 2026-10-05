// Dino runner: tap B to jump, hold B while rising to jump higher; clear the cacti. Rules in dino_logic.h.
#include "dino_logic.h"
#include "games.h"
#include "../../core/display.h"
#include "../../core/theme.h"

static dino::State s;

// 18x20 sprites, 3 bytes a row, 1 = black: two running poses (they alternate) and one for jumping.
static const uint8_t RUN1[] = {
  0x00, 0x3F, 0x80, 0x00, 0x6F, 0xC0, 0x00, 0x7F, 0xC0, 0x00, 0x7F, 0xC0, 0x00, 0x7C, 0x00, 0x00, 0x7F, 0x80,
  0x80, 0xFC, 0x00, 0x81, 0xFC, 0x00, 0xC3, 0xFE, 0x00, 0xE7, 0xFD, 0x00, 0xFF, 0xF8, 0x00, 0x7F, 0xF8, 0x00,
  0x3F, 0xF0, 0x00, 0x1F, 0xE0, 0x00, 0x0F, 0xE0, 0x00, 0x0E, 0x60, 0x00, 0x0C, 0x20, 0x00, 0x08, 0x30, 0x00,
  0x0C, 0x00, 0x00, 0x00, 0x00, 0x00,
};
static const uint8_t RUN2[] = {
  0x00, 0x3F, 0x80, 0x00, 0x6F, 0xC0, 0x00, 0x7F, 0xC0, 0x00, 0x7F, 0xC0, 0x00, 0x7C, 0x00, 0x00, 0x7F, 0x80,
  0x80, 0xFC, 0x00, 0x81, 0xFC, 0x00, 0xC3, 0xFE, 0x00, 0xE7, 0xFD, 0x00, 0xFF, 0xF8, 0x00, 0x7F, 0xF8, 0x00,
  0x3F, 0xF0, 0x00, 0x1F, 0xE0, 0x00, 0x0F, 0xE0, 0x00, 0x0E, 0x60, 0x00, 0x0C, 0x60, 0x00, 0x04, 0x40, 0x00,
  0x00, 0x60, 0x00, 0x00, 0x00, 0x00,
};
static const uint8_t JUMP_POSE[] = {
  0x00, 0x3F, 0x80, 0x00, 0x6F, 0xC0, 0x00, 0x7F, 0xC0, 0x00, 0x7F, 0xC0, 0x00, 0x7C, 0x00, 0x00, 0x7F, 0x80,
  0x80, 0xFC, 0x00, 0x81, 0xFC, 0x00, 0xC3, 0xFE, 0x00, 0xE7, 0xFD, 0x00, 0xFF, 0xF8, 0x00, 0x7F, 0xF8, 0x00,
  0x3F, 0xF0, 0x00, 0x1F, 0xE0, 0x00, 0x0F, 0xE0, 0x00, 0x0E, 0x60, 0x00, 0x0C, 0x30, 0x00, 0x0C, 0x10, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static void start(uint32_t seed) { dino::reset(s, seed); }

static Step step(bool tapped, bool held) {
  if (dino::waiting(s) && !tapped) return Step::Still;  // standing until the first jump
  return dino::step(s, tapped, held) ? Step::Moved : Step::Over;
}

static int score() { return s.score; }

// A cactus: a trunk with an arm on each side, rounded at the top.
static void drawCactus(const dino::Cactus &c) {
  const int x = c.x / dino::U, w = dino::width(c), h = dino::height(c), top = dino::GROUND - h, trunk = x + w / 2 - 2;
  display.fillRoundRect(trunk, top, 4, h, 2, GxEPD_BLACK);
  const int armY = top + h / 4;
  display.fillRoundRect(x, armY, 3, h / 3, 1, GxEPD_BLACK);              // left arm
  display.fillRect(x, armY + h / 3 - 3, trunk - x, 3, GxEPD_BLACK);
  display.fillRoundRect(x + w - 3, armY - 3, 3, h / 3, 1, GxEPD_BLACK);  // right arm
  display.fillRect(trunk + 4, armY + h / 3 - 6, x + w - 3 - trunk - 4, 3, GxEPD_BLACK);
}

static void draw() {
  using namespace dino;
  for (const Cactus &c : s.cacti)
    if (c.x / U < 200 && c.x / U + width(c) > 0) drawCactus(c);
  display.fillRect(0, GROUND, 200, 2, GxEPD_BLACK);  // the ground, with pebbles that scroll past
  const int shift = s.frames * SPEED / U % 40;
  for (int x = 40 - shift; x < 200 + 40; x += 40) {
    display.drawFastHLine(x, GROUND + 8, 4, GxEPD_BLACK);
    display.drawFastHLine(x + 22, GROUND + 15, 6, GxEPD_BLACK);
  }
  const uint8_t *pose = !onGround(s) ? JUMP_POSE : s.frames / 5 % 2 ? RUN2 : RUN1;  // legs change every ~0.25 s
  display.drawBitmap(DINO_X, GROUND - s.h / U - DINO_H + 1, pose, DINO_W, DINO_H, GxEPD_BLACK);

  char text[8];
  snprintf(text, sizeof text, "%05d", s.score);
  display.setFont(FONT_SMALL);
  drawRight(text, 18);
  if (waiting(s)) {  // before the first jump
    display.fillRoundRect(24, 52, 152, 62, 6, GxEPD_WHITE);
    display.drawRoundRect(24, 52, 152, 62, 6, GxEPD_BLACK);
    drawCentered("Press B to jump", 70);
    display.setFont(FONT_TINY);
    drawCentered("hold B: jump higher", 90);
    drawCentered("hold A: back", 104);
  }
}

extern const Game dinoGame = {"Dino", "g_dino", FRAME_MS, start, step, draw, score};
