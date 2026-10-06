// Pet: a small creature you dress up (body, eyes, mouth, hat, extra), saved in NVS as one number ("pet_look").
// Main screen: A = say hi (it hops), B = edit, hold A = back. It blinks every few seconds for a minute after the
// last press. Edit: A = next row, B = change it (next option; Random: a new look;
// Done: save), hold B = previous option, hold A = save and back.
#include <esp_random.h>
#include "pet_logic.h"
#include "../../core/app.h"
#include "../../core/devtools.h"
#include "../../core/display.h"
#include "../../core/power.h"
#include "../../core/storage.h"
#include "../../core/theme.h"

static const int MAIN_SCALE = 4, EDIT_SCALE = 3;  // 128 px on the main screen, 96 px while editing
static const int16_t EDIT_AVATAR_Y = CONTENT_TOP + 4, ROW_Y = HINTS_TOP - 14;
static const int RANDOM_ROW = pet::LAYERS, DONE_ROW = pet::LAYERS + 1, ROWS = pet::LAYERS + 2;

enum Screen : uint8_t { MAIN, EDIT };
RTC_DATA_ATTR static uint8_t screen, row;
RTC_DATA_ATTR static uint32_t draft;  // the look being edited (kept through sleep)
static bool blinking, heart;
static int8_t hop;  // pixels above its resting place, while it hops
static uint32_t nextBlinkAt, openAt, lastPressAt;

static const int16_t REST_Y = HINTS_TOP - 3 - pet::SIZE * MAIN_SCALE;  // low on the screen, with room to hop
static const uint32_t AWAKE_MS = 60000;  // blinking stops this long after the last press

static const char *const HEART[] = {  // drawn at 2x beside it, mid-hop
  ".##...##.",
  "####.####",
  "#########",
  "#########",
  ".#######.",
  "..#####..",
  "...###...",
  "....#....",
};

static uint32_t savedBits() { return storageGetInt("pet_look", pet::pack(pet::DEFAULT_LOOK)); }

static void drawAvatar(const pet::Look &look, int scale, int16_t y0, bool eyesClosed = false) {
  const int16_t x0 = (display.width() - pet::SIZE * scale) / 2;
  pet::drawLook(look, eyesClosed, [&](int x, int y, bool ink) {
    display.fillRect(x0 + x * scale, y0 + y * scale, scale, scale, ink ? BLACK : WHITE);
  });
}

static void save() { storagePutInt("pet_look", pet::pack(pet::unpack(draft))); }  // skips the write if unchanged

static uint32_t randomNumber() { return devSeed() ? (uint32_t)rand() : esp_random(); }  // test build: repeatable

static void drawMain() {
  drawHeader("Pet");
  pet::Look look = pet::unpack(savedBits());
  if (hop) look.part[pet::EYES_LAYER] = pet::HAPPY_EYES;
  drawAvatar(look, MAIN_SCALE, REST_Y - hop, blinking);
  if (heart)
    for (int y = 0; y < 8; y++)
      for (int x = 0; x < 9; x++)
        if (HEART[y][x] == '#') display.fillRect(166 + x * 2, CONTENT_TOP + 8 + y * 2, 2, 2, BLACK);
  drawHints("hi", "dress up", "");
}

// A: a little hop with happy eyes and a heart, as quick animation frames (like the games), then a normal partial
// refresh to clean up.
static Redraw sayHi() {
  static const int8_t HOPS[] = {3, 6, 6, 6, 3, 0};
  powerHold();
  displayFastFrames(true);
  for (size_t i = 0; i < sizeof HOPS; i++) {
    hop = HOPS[i];
    heart = i >= 1 && i <= 4;
    displayFrame(drawMain);
  }
  hop = 0;
  heart = false;
  displayFastFrames(false);
  powerRelease();
  return Redraw::Partial;
}

static void onEnter() {
  screen = MAIN;
  lastPressAt = millis();
}

static Redraw onButton(Event e) {
  lastPressAt = millis();
  if (screen == MAIN) {
    if (e == Event::AShort) return sayHi();
    if (e != Event::BShort) return Redraw::None;
    draft = savedBits();
    row = 0;
    screen = EDIT;
    return Redraw::Full;  // the avatar changes size: a full refresh leaves no ghost
  }
  pet::Look look = pet::unpack(draft);
  if (e == Event::AShort) {
    row = (row + 1) % ROWS;
  } else if (e == Event::BShort && row == DONE_ROW) {
    save();
    screen = MAIN;
    return Redraw::Full;
  } else if (e == Event::BShort && row == RANDOM_ROW) {
    if (devSeed()) srand(devSeed() + draft);
    look = pet::randomLook(randomNumber);
  } else if ((e == Event::BShort || e == Event::BLong) && row < pet::LAYERS) {
    pet::stepPart(look, row, e == Event::BShort ? 1 : -1);
  } else {
    return Redraw::None;
  }
  draft = pet::pack(look);
  return Redraw::Partial;
}

static Redraw onBack() {
  lastPressAt = millis();
  if (screen == MAIN) return Redraw::Exit;
  save();
  screen = MAIN;
  return Redraw::Full;
}

static void drawEdit() {
  const pet::Look look = pet::unpack(draft);
  drawHeader("Dress up");
  drawAvatar(look, EDIT_SCALE, EDIT_AVATAR_Y);
  display.setFont(FONT_SMALL);
  char right[16];
  if (row < pet::LAYERS) {
    const pet::LayerParts lp = pet::partsOf(row);
    display.setCursor(MARGIN, ROW_Y);
    display.print(pet::LAYER_NAMES[row]);
    snprintf(right, sizeof right, "%s  %d/%d", lp.parts[look.part[row]].name, look.part[row] + 1, lp.count);
    drawRight(right, ROW_Y);
    drawHints("next", "change", "previous");
  } else if (row == RANDOM_ROW) {
    display.setCursor(MARGIN, ROW_Y);
    display.print("Random look");
    drawHints("next", "shuffle", "");
  } else {
    display.setCursor(MARGIN, ROW_Y);
    display.print("Done");
    drawHints("next", "save", "");
  }
}

static void draw() {
  if (screen == EDIT) drawEdit();
  else drawMain();
}

#if UNIDEX_DEV
// Test build (X STATE detail): "main:<saved look>" or "edit:<row>:<look being edited>", looks in hex.
static const char *detail() {
  static char buf[32];
  if (screen == EDIT) snprintf(buf, sizeof buf, "edit:%d:%05lx", row, (unsigned long)pet::pack(pet::unpack(draft)));
  else snprintf(buf, sizeof buf, "main:%05lx", (unsigned long)pet::pack(pet::unpack(savedBits())));
  return buf;
}
#endif

static Redraw tick() {
#if UNIDEX_DEV
  devSetDetail(detail);  // here rather than onEnter, which a wake from sleep skips
  if (devManualFrames()) return Redraw::None;  // test build: no blinking, for repeatable screenshots
#endif
  // Blink: eyes shut for one refresh every 2.5-6 s. On battery the device naps between presses, so it asks to
  // be woken in time for the next one.
  const uint32_t now = millis();
  if (screen != MAIN) {
    blinking = false;
    return Redraw::None;
  }
  if (blinking) {
    if ((int32_t)(now - openAt) < 0) {
      powerWakeWithin(openAt - now);
      return Redraw::None;
    }
    blinking = false;
    nextBlinkAt = now + 2500 + esp_random() % 3500;
    return Redraw::Tick;
  }
  if (now - lastPressAt > AWAKE_MS) return Redraw::None;
  if (nextBlinkAt == 0) nextBlinkAt = now + 2500 + esp_random() % 3500;
  if ((int32_t)(now - nextBlinkAt) < 0) {
    powerWakeWithin(nextBlinkAt - now);
    return Redraw::None;
  }
  blinking = true;
  openAt = now + 120;
  return Redraw::Tick;
}

static void onExit() {
#if UNIDEX_DEV
  devSetDetail(nullptr);
#endif
}

static void status(char *out, size_t len) { snprintf(out, len, "Say hi!"); }

extern const App petApp = {"Pet", ICON_PET, onEnter, onButton, draw, onExit, onBack, status, tick};
