#include "launcher.h"
#include <Arduino.h>
#include "battery.h"
#include "clock.h"
#include "display.h"
#include "theme.h"
#include "../apps/apps.h"

static const int HOME = -1, SETTINGS = -2;
RTC_DATA_ATTR static int current = HOME;  // open app, HOME or SETTINGS
RTC_DATA_ATTR static int selected;        // highlighted icon on the home screen

static const App *app(int i) { return i == SETTINGS ? &settingsApp : APPS[i]; }

static const int16_t CELL_W = 96, CELL_H = 74, GRID_X = 4, GRID_Y = CONTENT_TOP + 2, GAP = 3;

// Small lightning bolt, 7 px wide and 12 tall, with its top-left corner at (x, y).
static void drawBolt(int16_t x, int16_t y) {
  display.fillTriangle(x + 4, y, x, y + 7, x + 4, y + 7, GxEPD_BLACK);
  display.fillTriangle(x + 3, y + 5, x + 7, y + 5, x + 3, y + 12, GxEPD_BLACK);
}

// Top right of the home header: "14:32  87%", with a bolt before it on USB power.
// Either text part is left out when it isn't known.
static void drawStatus() {
  char text[16] = "";
  if (clockValid()) {
    time_t t = time(nullptr);
    struct tm now;
    localtime_r(&t, &now);
    snprintf(text, sizeof text, "%02d:%02d", now.tm_hour, now.tm_min);
  }
  const int pct = batteryPercent();
  if (pct >= 0) snprintf(text + strlen(text), sizeof text - strlen(text), "%s%d%%", *text ? "  " : "", pct);
  int16_t x, y;
  uint16_t w = 0, h;
  if (*text) {
    display.setFont(FONT_SMALL);
    display.getTextBounds(text, 0, 0, &x, &y, &w, &h);
    drawRight(text, 16);
  }
  if (batteryCharging()) drawBolt(display.width() - MARGIN - w - (w ? 12 : 7), 5);
}

static void drawHome() {
  drawHeader("unidex");
  drawStatus();
  display.setFont(FONT_SMALL);
  for (int i = 0; i < APP_COUNT; i++) {
    int16_t x = GRID_X + (i % 2) * CELL_W, y = GRID_Y + (i / 2) * CELL_H;
    uint16_t ink = GxEPD_BLACK;
    if (i == selected) {
      display.fillRoundRect(x + GAP, y + GAP, CELL_W - 2 * GAP, CELL_H - 2 * GAP, 6, GxEPD_BLACK);
      ink = GxEPD_WHITE;
    }
    drawIcon(APPS[i]->icon, x + (CELL_W - ICON_SIZE) / 2, y + 8, ink);
    int16_t tx, ty;
    uint16_t tw, th;
    display.getTextBounds(APPS[i]->name, 0, 0, &tx, &ty, &tw, &th);
    display.setTextColor(ink);
    display.setCursor(x + (CELL_W - tw) / 2 - tx, y + 65);
    display.print(APPS[i]->name);
  }
  display.setTextColor(GxEPD_BLACK);
  drawFooter("next", "open");
}

static void drawSplash() {
  // Mark: 2x2 squares, one filled, echoing the home grid.
  const int16_t S = 14, G = 4, mx = (200 - 2 * S - G) / 2, my = 56;
  display.drawRect(mx, my, S, S, GxEPD_BLACK);
  display.drawRect(mx + S + G, my, S, S, GxEPD_BLACK);
  display.drawRect(mx, my + S + G, S, S, GxEPD_BLACK);
  display.fillRect(mx + S + G, my + S + G, S, S, GxEPD_BLACK);
  display.setFont(FONT_LARGE);
  drawCentered("unidex", 128);
}

void launcherBegin(bool woke) {
  if (woke) return;  // the screen still shows where you were; the wake press itself redraws
  displayShow(drawSplash, true);
  delay(1200);
  displayShow(drawHome, false);
}

void launcherHandle(Event e) {
  if (current == HOME) {
    if (e == Event::AShort) {
      selected = (selected + 1) % APP_COUNT;
      displayShow(drawHome, false);
    } else if (e == Event::BShort || e == Event::ALong) {
      current = e == Event::ALong ? SETTINGS : selected;
      app(current)->onEnter();
      displayShow(app(current)->draw, true);
    } else {
      displayShow(drawHome, false);  // B long: refresh the clock and battery in the header
    }
    return;
  }
  if (e == Event::ALong) {
    app(current)->onExit();
    current = HOME;
    displayShow(drawHome, true);
  } else {
    Redraw r = app(current)->onButton(e);
    if (r != Redraw::None) displayShow(app(current)->draw, r == Redraw::Full);
  }
}
