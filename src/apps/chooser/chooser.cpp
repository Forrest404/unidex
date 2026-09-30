// Stub until its step is built.
#include "../../core/app.h"
#include "../../core/display.h"
#include "../../core/theme.h"

static void draw() {
  drawHeader("Chooser");
  display.setFont(FONT_SMALL);
  drawCentered("coming soon", (CONTENT_TOP + CONTENT_BOTTOM) / 2);
  drawFooter("hold home", "");
}

static void noop() {}
static bool onButton(Event) { return false; }

extern const App chooserApp = {"Chooser", ICON_CHOOSER, noop, onButton, draw, noop};
