// Pet: a small creature you dress up (body, eyes, mouth, hat, extra), saved in NVS as one number ("pet_look").
// Hold A = back.
#include "pet_logic.h"
#include "../../core/app.h"
#include "../../core/display.h"
#include "../../core/storage.h"
#include "../../core/theme.h"

static const int SCALE = 4, AVATAR = pet::SIZE * SCALE;  // 128 px on screen
static const int16_t AVATAR_Y = CONTENT_TOP + (HINTS_TOP - CONTENT_TOP - AVATAR) / 2;  // centred between the lines

static pet::Look savedLook() { return pet::unpack(storageGetInt("pet_look", pet::pack(pet::DEFAULT_LOOK))); }

static void drawAvatar(const pet::Look &look, int16_t x0, int16_t y0) {
  pet::drawLook(look, false, [&](int x, int y, bool ink) {
    display.fillRect(x0 + x * SCALE, y0 + y * SCALE, SCALE, SCALE, ink ? BLACK : WHITE);
  });
}

static void onEnter() {}

static Redraw onButton(Event) { return Redraw::None; }

static Redraw onBack() { return Redraw::Exit; }

static void draw() {
  drawHeader("Pet");
  drawAvatar(savedLook(), (display.width() - AVATAR) / 2, AVATAR_Y);
  drawHints("", "", "");
}

static void onExit() {}

static void status(char *out, size_t len) { snprintf(out, len, "Say hi!"); }

extern const App petApp = {"Pet", ICON_PET, onEnter, onButton, draw, onExit, onBack, status};
