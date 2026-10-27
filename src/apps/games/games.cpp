// Games: one-button games. The built-in four, then the store's games: ones made as games (type "game": they play
// on the same frame loop as the built-ins) and apps in the games category (they open as apps, back here after).
// A = next, B = play, hold B on one from the store = offload or delete it, hold A = back (from a game: this list).
// In a game B is the one action; the round runs a frame at a time on the fast e-ink waveform (~7 frames a second).
#include "games.h"
#include <Arduino.h>
#include <vector>
#include "../../core/app.h"
#include "../../core/devtools.h"
#include "../../core/display.h"
#include "../../core/input.h"
#include "../../core/launcher.h"
#include "../../core/power.h"
#include "../../core/storage.h"
#include "../../core/theme.h"
#include "../scripts/appstore.h"
#include "../scripts/luahost.h"
#include "../scripts/scripts.h"

extern const Game flappyGame, dinoGame, stackGame, jetpackGame;
extern const App scriptsApp;
static const Game *const BUILT_IN[] = {&flappyGame, &dinoGame, &stackGame, &jetpackGame};
static const int BUILT_IN_COUNT = sizeof(BUILT_IN) / sizeof(BUILT_IN[0]);
static const int ROWS = 5, ROW_H = 26;
static const uint32_t IDLE_MS = 30000;  // no B for this long mid-round: leave it (so the board can sleep)

// One row of the list: a built-in game, or one from the store.
struct Entry {
  const Game *builtIn;  // nullptr for one from the store
  String id, name;      // a store one's id
  bool game;            // a store game (else an app in the games category, opened as an app)
  String problem;       // why it can't run here ("needs newer firmware"), or ""
  int best;
};
static std::vector<Entry> entries;
static bool listed;
static uint32_t listedAt;  // appsGeneration() when the list was read

enum Screen : uint8_t { LIST, PLAY, OVER, FAILED };
// Kept through deep sleep (the board can sleep on the game-over screen): which game, and how it went.
RTC_DATA_ATTR static uint8_t screen, cursor;
RTC_DATA_ATTR static int8_t playBuiltIn = -1;         // the built-in game played, or -1 for a store game (playId)
RTC_DATA_ATTR static char playId[25], playName[21];
RTC_DATA_ATTR static char lastName[21];               // for the home line
RTC_DATA_ATTR static int16_t lastBest, lastScore;
RTC_DATA_ATTR static int8_t gameCount = -1;           // likewise
RTC_DATA_ATTR static bool newBest;
static uint16_t storeFrameMs = 100;  // a store game's frame_ms
static String failMsg;
static uint32_t frameAt;   // when the last frame was due (frames keep a steady beat)
static uint32_t bAt;       // the last time B was down (for IDLE_MS)
static bool tapped;        // B was tapped since the last frame (a quick tap can fall between frames)
static bool wasHeld;       // B was down at the last frame
static bool pressCounted;  // the press that's down now already counted as a tap

static bool storeGame() { return playBuiltIn < 0; }
static const Game *builtIn() { return BUILT_IN[playBuiltIn]; }
static uint16_t frameMs() { return storeGame() ? storeFrameMs : builtIn()->frameMs; }

bool gamesPlaying(const char *id) { return screen == PLAY && storeGame() && strcmp(playId, id) == 0; }

// --- best scores: the built-ins' in NVS, a store game's in its own data/best ---
static int storeBest(const String &id) {
  String text;
  const String path = appsDataPath(id, "best");
  if (!storageExists(path.c_str())) return 0;  // (opening a missing file logs an error on USB)
  fs::File f = storageOpen(path.c_str());
  if (f) storageReadLine(f, text, 16), f.close();
  return text.toInt();
}

static bool saveStoreBest(const String &id, int score) {
  const String path = appsDataPath(id, "best"), tmp = appsDataPath(id, ".write.tmp"), text = String(score);
  if (!storageMakeDirs((appsDir(id) + "/data").c_str())) return false;
  fs::File f = storageOpen(tmp.c_str(), "w");
  const size_t wrote = f ? f.print(text) : 0;
  if (f) f.close();
  return wrote == text.length() && storageReplace(tmp.c_str(), path.c_str(), wrote);
}

static int bestNow() { return storeGame() ? storeBest(playId) : storageGetInt(builtIn()->bestKey, 0); }

// The built-in games, then the store's (read again whenever an app is installed or removed).
static void ensureList() {
  if (listed && listedAt == appsGeneration()) return;
  listed = true;
  listedAt = appsGeneration();
  entries.clear();
  for (const Game *g : BUILT_IN) entries.push_back({g, "", g->name, false, "", storageGetInt(g->bestKey, 0)});
  if (storageCardMount())
    for (const AppEntry &a : appsList(false))
      if (a.inGames && !a.offloaded) entries.push_back({nullptr, a.id, a.name, a.game, a.problem, a.game ? storeBest(a.id) : 0});
  gameCount = entries.size();
  if (cursor >= entries.size()) cursor = 0;
  if (screen != PLAY && hostRunning() && hostId() == playId) hostStop();  // a new version: loaded afresh next round
}

static void drawList() {
  ensureList();
  const int pages = (entries.size() + ROWS - 1) / ROWS, first = cursor / ROWS * ROWS;
  drawHeader("Games", pages > 1 ? (String(cursor / ROWS + 1) + "/" + pages).c_str() : nullptr);
  display.setFont(FONT_SMALL);
  for (int r = 0; r < ROWS && first + r < (int)entries.size(); r++) {
    const Entry &e = entries[first + r];
    const int16_t top = CONTENT_TOP + 4 + r * ROW_H, baseline = top + 17;
    const bool sel = first + r == cursor;
    if (sel) display.fillRect(MARGIN - 4, top, display.width() - 2 * (MARGIN - 4), ROW_H - 2, BLACK);
    display.setTextColor(sel ? WHITE : BLACK);
    char right[16] = "";
    if (e.problem.length()) snprintf(right, sizeof right, "can't run");
    else if (e.builtIn || e.game) snprintf(right, sizeof right, "best %d", e.best);
    display.setFont(FONT_TINY);
    drawRight(right, baseline);
    const int16_t rightW = *right ? textWidth(right) + 6 : 0;
    display.setFont(FONT_SMALL);
    display.setCursor(MARGIN, baseline);
    display.print(fitText(e.name.c_str(), display.width() - 2 * MARGIN - rightW));
    display.setTextColor(BLACK);
  }
  const Entry &e = entries[cursor];
  drawHints(entries.size() > 1 ? "next" : "", e.problem.length() ? "" : e.builtIn || e.game ? "play" : "open",
            e.builtIn ? "" : "remove");
}

static void drawOver() {
  char line[32];
  drawHeader(playName);
  display.setFont(FONT_LARGE);
  snprintf(line, sizeof line, "%d", lastScore);
  drawCentered(line, 74);
  display.setFont(FONT_SMALL);
  if (newBest) snprintf(line, sizeof line, "New best!");
  else snprintf(line, sizeof line, "Best: %d", bestNow());
  drawCentered(line, 112);
  drawCentered("Game over", 136);
  drawHints("", "again", "");
}

static void drawFailed() {
  drawHeader("This game failed");
  display.setFont(FONT_SMALL);
  String lines[6];
  const int n = wrapText(failMsg.length() ? failMsg.c_str() : "Something went wrong.", display.width() - 2 * MARGIN,
                         lines, 6);
  for (int i = 0; i < n && i < 6; i++) {
    display.setCursor(MARGIN, CONTENT_TOP + 20 + i * 19);
    display.print(lines[i]);
  }
  drawHints("", "ok", "");
}

static void drawPlay() {
  if (storeGame()) hostDraw("draw");
  else builtIn()->draw();
}

// A store game stops: its error (or why it couldn't open) on the screen, instead of the round.
static void failed(const String &why) {
  if (screen == PLAY) powerRelease(), displayFastFrames(false);
  failMsg = why;
  screen = FAILED;
}

// A store game's Lua is open (it isn't after a deep sleep, or when the Apps app used the host in between).
static bool storeReady() {
  if (hostRunning() && hostId() == playId) return true;
  apps::Manifest m;
  String problem;
  if (!appsManifest(playId, m, problem)) {
    failed(problem);
    return false;
  }
  storeFrameMs = m.frameMs;
  if (!hostStart(playId, m.perms)) {
    failed(hostError());
    return false;
  }
  return true;
}

// A new round of the game picked (playBuiltIn or playId).
static void startRound() {
  const uint32_t seed = devSeed() ? devSeed() : esp_random();
  if (storeGame()) {
    if (!storeReady()) return;
    if (!hostGameStart(seed)) {
      failed(hostError());
      return;
    }
  } else {
    builtIn()->start(seed);
  }
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
  int score = 0;
  if (storeGame()) {
    if (!hostGameScore(score)) {
      failed(hostError());
      return;
    }
  } else {
    score = builtIn()->score();
  }
  const int best = bestNow();
  lastScore = score;
  newBest = score > best;
  screen = OVER;
  if (devDryRun()) return;  // test build: a test round leaves the scores and the home line alone
  if (newBest) {
    const bool saved = storeGame() ? saveStoreBest(playId, score) : (storagePutInt(builtIn()->bestKey, score), true);
    if (!saved || bestNow() != score) launcherToast("Couldn't save the best");  // storage full
    listed = false;  // the list shows the new best
  }
  strlcpy(lastName, playName, sizeof lastName);
  lastBest = max(score, best);
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

// One step of whichever game: 0 over, 1 moved, 2 still, -1 a store game failed.
static int stepOnce(bool tap, bool held) {
  if (storeGame()) return hostGameStep(tap, held);
  const Step r = builtIn()->step(tap, held);
  return r == Step::Over ? 0 : r == Step::Moved ? 1 : 2;
}

// Shows the frame; a store game's draw() can fail too.
static bool showFrame() {
  displayFrame(drawPlay);
  if (storeGame() && !hostRunning()) {
    failed(hostError());
    return false;
  }
  return true;
}

// One frame of play: step the game, then show it (or end the round). Showing a frame can take longer than a step,
// so the game takes the steps it's behind by (up to 3) before showing: it plays at the same speed either way.
static Redraw frame() {
  const bool held = inputBHeld(), tap = takeTap(held);
  bool moved = false;
  for (int steps = 0; steps < 3; steps++) {
    frameAt += frameMs();
    const int r = stepOnce(tap && steps == 0, held);
    if (r < 0) {
      failed(hostError());
      return Redraw::Full;
    }
    if (r == 0) {
      finish();
      return Redraw::Full;  // the game-over screen, clearing the frames' ghosting
    }
    moved |= r == 1;
    if (millis() - frameAt < frameMs()) break;  // caught up
  }
  if (millis() - frameAt >= 2u * frameMs()) frameAt = millis();  // far behind (a stall): the beat from now
  if (moved && !showFrame()) return Redraw::Full;
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
    const int r = stepOnce(tap, held);
    if (r < 0) failed(hostError());
    else if (r == 0) finish();
    if (r <= 0) {
      displayShow(screen == FAILED ? drawFailed : drawOver, true);
      return;
    }
  }
  frameAt = millis();
  if (!showFrame()) displayShow(drawFailed, true);
}

// Test build: "list:Dino" (the highlighted game), "play:Dino", "over:Dino" or "failed:Dino", for X STATE.
static const char *detail() {
  static char text[40];
  if (screen == LIST) {
    ensureList();
    snprintf(text, sizeof text, "list:%s", entries[cursor].name.c_str());
  } else {
    snprintf(text, sizeof text, "%s:%s", screen == PLAY ? "play" : screen == OVER ? "over" : "failed", playName);
  }
  return text;
}
#endif

// B on the list: a game starts, an app opens (in Apps, back here after).
static Redraw choose() {
  ensureList();
  const Entry &e = entries[cursor];
  if (e.problem.length()) {
    launcherToast(e.problem.c_str());
    return Redraw::Partial;
  }
  if (!e.builtIn && !e.game) {
    scriptsOpenForGames(e.id);
    launcherInterrupt(&scriptsApp);
    return Redraw::None;
  }
  strlcpy(playName, e.name.c_str(), sizeof playName);
  playBuiltIn = -1;
  playId[0] = 0;
  for (int i = 0; i < BUILT_IN_COUNT; i++)
    if (BUILT_IN[i] == e.builtIn) playBuiltIn = i;
  if (!e.builtIn) strlcpy(playId, e.id.c_str(), sizeof playId);
  startRound();
  return Redraw::Full;
}

static void onEnter() {
  screen = LIST;
  listed = false;
}

static Redraw onButton(Event e) {
  switch (screen) {
    case LIST:
      ensureList();
      if (e == Event::AShort && entries.size() > 1) {
        cursor = (cursor + 1) % entries.size();
        return Redraw::Partial;
      }
      if (e == Event::BShort) return choose();
      if (e == Event::BLong && !entries[cursor].builtIn) {  // offload or delete one from the store, in Apps
        scriptsManageForGames(entries[cursor].id);
        launcherInterrupt(&scriptsApp);
      }
      return Redraw::None;
    case PLAY:
      // A press the frames didn't see go down (it came and went between two frames): count it now.
      if ((e == Event::BShort || e == Event::BLong) && !pressCounted) tapped = true;
      if (e == Event::BShort) pressCounted = false;  // released
      else if (e == Event::BLong) pressCounted = true;  // still held: don't count it again
      bAt = millis();
      return Redraw::None;
    case OVER:
      if (e != Event::BShort) return Redraw::None;
      startRound();
      return Redraw::Full;
    case FAILED:
      if (e != Event::BShort) return Redraw::None;
      screen = LIST;
      listed = false;
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
  else if (screen == FAILED) drawFailed();
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
  if (millis() - frameAt < frameMs()) return Redraw::None;
  return frame();
}

static void onExit() {
  if (screen == PLAY) powerRelease(), displayFastFrames(false);
  if (hostRunning() && hostId() == playId) hostStop();
  screen = LIST;
#if UNIDEX_DEV
  devSetFrameStepper(nullptr);
  devSetDetail(nullptr);
#endif
}

static void status(char *out, size_t len) {
  if (lastName[0]) snprintf(out, len, "Best: %s %d", lastName, lastBest);
  else if (gameCount > 0) snprintf(out, len, "%d games", gameCount);
  else snprintf(out, len, "Play a game");  // (the store's aren't counted until the list has been read)
}

extern const App gamesApp = {"Games", ICON_GAMES, onEnter, onButton, draw, onExit, onBack, status, tick};
