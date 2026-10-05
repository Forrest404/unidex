// Jetpack: hold B to fly up, let go to fall; dodge the zappers and pick up coins. Rules in jetpack_logic.h.
#include "jetpack_logic.h"
#include "games.h"
#include "../../core/display.h"
#include "../../core/theme.h"

static jetpack::State s;
static bool thrusting;  // B was down on the last frame (the flame shows)

// 16x18 sprites, 2 bytes a row, 1 = black: flying, and two running poses on the floor (they alternate).
static const uint8_t FLY[] = {
  0x03, 0xC0, 0x07, 0xE0, 0x06, 0xB0, 0x07, 0xE0, 0x03, 0xC0, 0x37, 0xE0, 0x7F, 0xF0, 0x77, 0xEC, 0x77, 0xE4,
  0x77, 0xE0, 0x77, 0xE0, 0x37, 0xE0, 0x06, 0x60, 0x06, 0x60, 0x06, 0x60, 0x06, 0x60, 0x0E, 0x70, 0x00, 0x00,
};
static const uint8_t RUN1[] = {
  0x03, 0xC0, 0x07, 0xE0, 0x06, 0xB0, 0x07, 0xE0, 0x03, 0xC0, 0x37, 0xE0, 0x7F, 0xF0, 0x77, 0xEC, 0x77, 0xE4,
  0x77, 0xE0, 0x77, 0xE0, 0x37, 0xE0, 0x06, 0x60, 0x0C, 0x30, 0x0C, 0x30, 0x18, 0x18, 0x18, 0x18, 0x38, 0x1C,
};
static const uint8_t RUN2[] = {
  0x03, 0xC0, 0x07, 0xE0, 0x06, 0xB0, 0x07, 0xE0, 0x03, 0xC0, 0x37, 0xE0, 0x7F, 0xF0, 0x77, 0xEC, 0x77, 0xE4,
  0x77, 0xE0, 0x77, 0xE0, 0x37, 0xE0, 0x06, 0x60, 0x06, 0x60, 0x03, 0xC0, 0x03, 0x60, 0x06, 0x30, 0x0E, 0x38,
};

static void start(uint32_t seed) {
  jetpack::reset(s, seed);
  thrusting = false;
}

static Step step(bool tapped, bool held) {
  if (jetpack::waiting(s) && !tapped && !held) return Step::Still;  // standing until the first press
  thrusting = tapped || held;  // a tap shorter than a frame still counts as a frame of thrust
  return jetpack::step(s, thrusting) ? Step::Moved : Step::Over;
}

static int score() { return jetpack::score(s); }

// A zapper: a knob at each end and a thick zigzag between.
static void drawZapper(const jetpack::Zapper &z) {
  const int cx = z.x + jetpack::ZAP_W / 2, top = z.y + 4, bottom = z.y + z.len - 4;
  for (int y = top, side = -3; y < bottom; y += 6, side = -side) {
    const int y2 = y + 6 < bottom ? y + 6 : bottom;
    display.drawLine(cx + side, y, cx - side, y2, GxEPD_BLACK);
    display.drawLine(cx + side + 1, y, cx - side + 1, y2, GxEPD_BLACK);
  }
  display.fillCircle(cx, top, 4, GxEPD_BLACK);
  display.fillCircle(cx, bottom, 4, GxEPD_BLACK);
}

static void draw() {
  using namespace jetpack;
  display.fillRect(0, CEILING - 2, 200, 2, GxEPD_BLACK);
  display.fillRect(0, FLOOR, 200, 2, GxEPD_BLACK);
  for (const Zapper &z : s.zappers) {
    for (int i = 0; i < COINS; i++) {  // a coin: a ring with a mark in the middle
      const int x = z.coinX + i * COIN_STEP;
      if (!(z.coins & (1 << i)) || x > 200 || x + COIN < 0) continue;
      display.drawCircle(x + COIN / 2, z.coinY + COIN / 2, COIN / 2, GxEPD_BLACK);
      display.drawFastVLine(x + COIN / 2, z.coinY + 3, COIN - 6, GxEPD_BLACK);
    }
    if (z.x < 200 && z.x + ZAP_W > 0) drawZapper(z);
  }
  const uint8_t *pose = !onFloor(s) ? FLY : s.frames % 2 ? RUN2 : RUN1;
  display.drawBitmap(PILOT_X, s.y, pose, PILOT_W, PILOT_H, GxEPD_BLACK);
  if (thrusting && !onFloor(s)) {  // the flame under the jetpack: an outline with a small solid core
    display.drawTriangle(PILOT_X - 1, s.y + 13, PILOT_X + 6, s.y + 13, PILOT_X + 2, s.y + 27, GxEPD_BLACK);
    display.fillTriangle(PILOT_X + 1, s.y + 15, PILOT_X + 4, s.y + 15, PILOT_X + 2, s.y + 21, GxEPD_BLACK);
  }

  char text[8];
  snprintf(text, sizeof text, "%d", score());
  display.setFont(FONT_SMALL);
  drawRight(text, 18);
  if (waiting(s)) {  // before the first press
    display.fillRoundRect(28, 70, 144, 48, 6, GxEPD_WHITE);
    display.drawRoundRect(28, 70, 144, 48, 6, GxEPD_BLACK);
    drawCentered("Hold B to fly", 86);
    display.setFont(FONT_TINY);
    drawCentered("hold A: back", 106);
  }
}

extern const Game jetpackGame = {"Jetpack", "g_jet", 480, start, step, draw, score};
