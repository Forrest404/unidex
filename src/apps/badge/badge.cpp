// Stub until its step is built.
#include "../../core/app.h"
#include "../../core/display.h"
#include "../../core/theme.h"

static void draw() {
  drawHeader("Badge");
  display.setFont(FONT_SMALL);
  drawCentered("coming soon", (CONTENT_TOP + CONTENT_BOTTOM) / 2);
  drawFooter("hold home", "");
}

static void noop() {}
static bool onButton(Event) { return false; }

extern const App badgeApp = {"Badge", ICON_BADGE, noop, onButton, draw, noop};
