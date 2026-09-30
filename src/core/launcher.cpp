#include "launcher.h"
#include <Arduino.h>
#include "display.h"
#include "theme.h"
#include "../apps/apps.h"

static const int HOME = -1;
RTC_DATA_ATTR static int current = HOME;  // open app, or HOME
RTC_DATA_ATTR static int selected;        // highlighted icon on the home screen

static const int16_t CELL_W = 96, CELL_H = 74, GRID_X = 4, GRID_Y = CONTENT_TOP + 2, GAP = 3;

static void drawHome() {
  drawHeader("unidex");
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
  if (woke) return;
  displayShow(drawSplash, true);
  delay(1200);
  displayShow(drawHome, false);
}

void launcherHandle(Event e) {
  if (current == HOME) {
    if (e == Event::AShort) {
      selected = (selected + 1) % APP_COUNT;
      displayShow(drawHome, false);
    } else if (e == Event::BShort) {
      current = selected;
      APPS[current]->onEnter();
      displayShow(APPS[current]->draw, true);
    }
    return;
  }
  if (e == Event::ALong) {
    APPS[current]->onExit();
    current = HOME;
    displayShow(drawHome, true);
  } else {
    Redraw r = APPS[current]->onButton(e);
    if (r != Redraw::None) displayShow(APPS[current]->draw, r == Redraw::Full);
  }
}
