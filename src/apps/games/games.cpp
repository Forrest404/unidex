// Games: a list of one-button games. A = next game, B = play, hold A = back (from a game: to this list).
// In a game B is the one action; the round runs a frame at a time (e-ink: about 2 frames a second).
#include "games.h"
#include <Arduino.h>
#include "../../core/app.h"
#include "../../core/devtools.h"
#include "../../core/display.h"
#include "../../core/input.h"
#include "../../core/launcher.h"
#include "../../core/power.h"
#include "../../core/storage.h"
#include "../../core/theme.h"

extern const Game flappyGame, dinoGame, stackGame, jetpackGame;
static const Game *const GAMES[] = {&flappyGame, &dinoGame, &stackGame, &jetpackGame};
static const int GAME_COUNT = sizeof(GAMES) / sizeof(GAMES[0]);
static const int ROW_H = 26;
static const uint32_t IDLE_MS = 30000;  // no B for this long mid-round: leave it (so the board can sleep)

enum Screen : uint8_t { LIST, PLAY, OVER };
// Kept through deep sleep (the board can sleep on the game-over screen): which game, and how it went.
RTC_DATA_ATTR static uint8_t screen, cursor, playing;  // playing: index of the game in GAMES
RTC_DATA_ATTR static int8_t lastGame = -1;              // for the home line
RTC_DATA_ATTR static int16_t lastBest, lastScore;
RTC_DATA_ATTR static bool newBest;
static uint32_t frameAt;   // when the last frame was due (frames keep a steady beat)
static uint32_t bAt;       // the last time B was down (for IDLE_MS)
static bool tapped;        // B was tapped since the last frame (a quick tap can fall between frames)
static bool wasHeld;       // B was down at the last frame
static bool pressCounted;  // the press that's down now already counted as a tap

static const Game *game() { return GAMES[playing]; }

static int best(const Game *g) { return storageGetInt(g->bestKey, 0); }

static void drawList() {
  drawHeader("Games");
  display.setFont(FONT_SMALL);
  for (int i = 0; i < GAME_COUNT; i++) {
    const int16_t top = CONTENT_TOP + 4 + i * ROW_H, baseline = top + 17;
    const bool sel = i == cursor;
    if (sel) display.fillRect(MARGIN - 4, top, display.width() - 2 * (MARGIN - 4), ROW_H - 2, BLACK);
    display.setTextColor(sel ? WHITE : BLACK);
    display.setCursor(MARGIN, baseline);
    display.print(GAMES[i]->name);
    char right[16];
    snprintf(right, sizeof right, "best %d", best(GAMES[i]));
    display.setFont(FONT_TINY);
    drawRight(right, baseline);
    display.setFont(FONT_SMALL);
    display.setTextColor(BLACK);
  }
  drawHints("next", "play", "");
}

static void drawOver() {
  char line[32];
  drawHeader(game()->name);
  display.setFont(FONT_LARGE);
  snprintf(line, sizeof line, "%d", lastScore);
  drawCentered(line, 74);
  display.setFont(FONT_SMALL);
  if (newBest) snprintf(line, sizeof line, "New best!");
  else snprintf(line, sizeof line, "Best: %d", best(game()));
  drawCentered(line, 112);
  drawCentered("Game over", 136);
  drawHints("", "again", "");
}

static void drawPlay() { game()->draw(); }

static void start(uint8_t index) {
  playing = index;
  const uint32_t seed = devSeed();
  game()->start(seed ? seed : esp_random());
  tapped = pressCounted = false;
  wasHeld = inputBHeld();
  frameAt = bAt = millis();
  powerHold();  // no light or deep sleep mid-round: the frames would stop
  displayFastFrames(true);  // ~40 ms frames; the next normal screen puts the panel back to its own waveform
  screen = PLAY;
}

static void finish() {
  powerRelease();
  displayFastFrames(false);
  lastScore = game()->score();
  newBest = lastScore > best(game());
  screen = OVER;
  if (devDryRun()) return;  // test build: a test round leaves the scores and the home line alone
  if (newBest) {
    storagePutInt(game()->bestKey, lastScore);
    if (best(game()) != lastScore) launcherToast("Couldn't save the best");  // settings storage full
  }
  lastGame = playing;
  lastBest = max<int>(lastScore, best(game()));
}

// B went down since the last frame: a press seen now (still held), or a quick one that came and went.
static bool takeTap(bool held) {
  bool tap = tapped;
  tapped = false;
  if (held && !wasHeld && !pressCounted) tap = pressCounted = true;
  if (!held) pressCounted = false;
  wasHeld = held;
  if (tap || held) bAt = millis();
  return tap;
}

// One frame of play: step the game, then show it (or end the round).
static Redraw frame() {
  const bool held = inputBHeld(), tap = takeTap(held);
  // On the beat; if a frame ran long (more than a whole beat behind), start the beat again from now.
  frameAt = millis() - frameAt < 2u * game()->frameMs ? frameAt + game()->frameMs : millis();
  const Step r = game()->step(tap, held);
  if (r == Step::Over) {
    finish();
    return Redraw::Full;  // the game-over screen, clearing the frames' ghosting
  }
  if (r == Step::Moved) displayFrame(drawPlay);
  powerActivity();
  return Redraw::None;
}

#if UNIDEX_DEV
// Test build: X FRAMES n plays n frames at once (no waiting), then shows the result. X PLAY gives B for
// each frame (`pattern`: '1' = down) instead of the real button.
static void stepFrames(int n, const char *pattern) {
  if (screen != PLAY) return;
  for (int i = 0; i < n; i++) {
    const bool held = pattern ? pattern[i] == '1' : inputBHeld(), tap = takeTap(held);
    if (game()->step(tap, held) == Step::Over) {
      finish();
      displayShow(drawOver, true);
      return;
    }
  }
  frameAt = millis();
  displayFrame(drawPlay);
}
#endif

#if UNIDEX_DEV
// Test build: "list:Dino" (the highlighted game), "play:Dino" or "over:Dino", for X STATE.
static const char *detail() {
  static char text[24];
  const char *where = screen == PLAY ? "play" : screen == OVER ? "over" : "list";
  snprintf(text, sizeof text, "%s:%s", where, (screen == LIST ? GAMES[cursor] : game())->name);
  return text;
}
#endif

static void onEnter() { screen = LIST; }

static Redraw onButton(Event e) {
  switch (screen) {
    case LIST:
      if (e == Event::AShort) {
        cursor = (cursor + 1) % GAME_COUNT;
        return Redraw::Partial;
      }
      if (e != Event::BShort) return Redraw::None;
      start(cursor);
      return Redraw::Full;
    case PLAY:
      // A press the frames didn't see go down (it came and went between two frames): count it now.
      if ((e == Event::BShort || e == Event::BLong) && !pressCounted) tapped = true;
      if (e == Event::BShort) pressCounted = false;  // released
      else if (e == Event::BLong) pressCounted = true;  // still held: don't count it again
      bAt = millis();
      return Redraw::None;
    case OVER:
      if (e != Event::BShort) return Redraw::None;
      start(playing);
      return Redraw::Full;
  }
  return Redraw::None;
}

static Redraw onBack() {
  if (screen == LIST) return Redraw::Exit;
  if (screen == PLAY) powerRelease(), displayFastFrames(false);  // leaving mid-round: no score
  screen = LIST;
  return Redraw::Full;
}

static void draw() {
  if (screen == PLAY) drawPlay();
  else if (screen == OVER) drawOver();
  else drawList();
}

static Redraw tick() {
#if UNIDEX_DEV
  devSetFrameStepper(stepFrames);  // here, not in onEnter: waking from sleep reopens the app without onEnter
  devSetDetail(detail);
#endif
  if (screen != PLAY || devManualFrames()) return Redraw::None;
  if (millis() - bAt > IDLE_MS) {  // left alone mid-round: leave it, so the board can sleep
    powerRelease();
    displayFastFrames(false);
    screen = LIST;
    return Redraw::Full;
  }
  if (millis() - frameAt < game()->frameMs) return Redraw::None;
  return frame();
}

static void onExit() {
  if (screen == PLAY) powerRelease(), displayFastFrames(false);
  screen = LIST;
#if UNIDEX_DEV
  devSetFrameStepper(nullptr);
  devSetDetail(nullptr);
#endif
}

static void status(char *out, size_t len) {
  if (lastGame >= 0) snprintf(out, len, "Best: %s %d", GAMES[lastGame]->name, lastBest);
  else snprintf(out, len, "%d one-button games", GAME_COUNT);
}

extern const App gamesApp = {"Games", ICON_GAMES, onEnter, onButton, draw, onExit, onBack, status, tick};
