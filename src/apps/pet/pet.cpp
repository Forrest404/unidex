// Pet: a small creature you dress up (body, eyes, mouth, hat, extra), saved in NVS as one number ("pet_look").
// Main screen: B = edit, hold A = back. Edit: A = next row, B = change it (next option; Random: a new look;
// Done: save), hold B = previous option, hold A = save and back.
#include <esp_random.h>
#include "pet_logic.h"
#include "../../core/app.h"
#include "../../core/devtools.h"
#include "../../core/display.h"
#include "../../core/storage.h"
#include "../../core/theme.h"

static const int MAIN_SCALE = 4, EDIT_SCALE = 3;  // 128 px on the main screen, 96 px while editing
static const int16_t EDIT_AVATAR_Y = CONTENT_TOP + 4, ROW_Y = HINTS_TOP - 14;
static const int RANDOM_ROW = pet::LAYERS, DONE_ROW = pet::LAYERS + 1, ROWS = pet::LAYERS + 2;

enum Screen : uint8_t { MAIN, EDIT };
RTC_DATA_ATTR static uint8_t screen, row;
RTC_DATA_ATTR static uint32_t draft;  // the look being edited (kept through sleep)

static uint32_t savedBits() { return storageGetInt("pet_look", pet::pack(pet::DEFAULT_LOOK)); }

static void drawAvatar(const pet::Look &look, int scale, int16_t y0) {
  const int16_t x0 = (display.width() - pet::SIZE * scale) / 2;
  pet::drawLook(look, false, [&](int x, int y, bool ink) {
    display.fillRect(x0 + x * scale, y0 + y * scale, scale, scale, ink ? BLACK : WHITE);
  });
}

static void save() { storagePutInt("pet_look", pet::pack(pet::unpack(draft))); }  // skips the write if unchanged

static uint32_t randomNumber() { return devSeed() ? (uint32_t)rand() : esp_random(); }  // test build: repeatable

static void onEnter() { screen = MAIN; }

static Redraw onButton(Event e) {
  if (screen == MAIN) {
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
  if (screen == EDIT) return drawEdit();
  drawHeader("Pet");
  const int16_t y = CONTENT_TOP + (HINTS_TOP - CONTENT_TOP - pet::SIZE * MAIN_SCALE) / 2;  // centred
  drawAvatar(pet::unpack(savedBits()), MAIN_SCALE, y);
  drawHints("", "dress up", "");
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
#endif
  return Redraw::None;
}

static void onExit() {
#if UNIDEX_DEV
  devSetDetail(nullptr);
#endif
}

static void status(char *out, size_t len) { snprintf(out, len, "Say hi!"); }

extern const App petApp = {"Pet", ICON_PET, onEnter, onButton, draw, onExit, onBack, status, tick};
