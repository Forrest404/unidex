#include "launcher.h"
#include <Arduino.h>
#include "battery.h"
#include "clock.h"
#include "display.h"
#include "storage.h"
#include "usbsync.h"
#include "theme.h"
#include "../apps/apps.h"

static const int HOME = -1, SETTINGS = -2, NO_CARD = -3;
RTC_DATA_ATTR static int current = HOME;  // open app, HOME, SETTINGS or NO_CARD
RTC_DATA_ATTR static int noCardFor;       // the app that couldn't open
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
static int shownMinute = -1;  // the minute the home clock shows, -1 = none

static void drawStatus() {
  char text[16] = "";
  shownMinute = -1;
  if (clockValid()) {
    time_t t = time(nullptr);
    struct tm now;
    localtime_r(&t, &now);
    snprintf(text, sizeof text, "%02d:%02d", now.tm_hour, now.tm_min);
    shownMinute = now.tm_min;
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

static void drawRestarting() {
  display.setFont(FONT_LARGE);
  drawCentered("Restarting", 88);
  display.setFont(FONT_SMALL);
  drawCentered("let go of the buttons", 124);
}

// A fresh boot: RAM and RTC state start over; files, NVS and the clock chip are untouched.
static void restart() {
  displayShow(drawRestarting, false);
  // Wait for release: BOOT (GPIO0) is the download-mode strapping pin, so don't restart with it held.
  while (inputAnyDown()) {
    inputPoll();
    delay(10);
  }
  ESP.restart();
}

void launcherBegin(bool woke) {
  if (woke) return;  // the screen still shows where you were; the wake press redraws (with the time)
  current = HOME;  // a fresh boot (power-on, flash or restart) always starts at home
  displayShow(drawSplash, true);
  delay(1200);
  displayShow(drawHome, false);
}

static bool needsCard(int i) {
  const App *a = app(i);
  return a == &timetableApp || a == &badgeApp || a == &dexApp;
}

static void drawNoCard() {
  const int16_t mid = (CONTENT_TOP + CONTENT_BOTTOM) / 2;
  drawHeader(app(noCardFor)->name);
  display.setFont(FONT_SMALL);
  drawCentered("No SD card", mid - 12);
  drawCentered("insert a FAT32 card", mid + 12);
  drawFooter("hold home", "");
}

void launcherHandle(Event e) {
  if (e == Event::Reset) restart();
  if (current == NO_CARD) {  // only A long (home) does anything; opening the app again retries
    if (e == Event::ALong) {
      current = HOME;
      displayShow(drawHome, true);
    }
    return;
  }
  if (current == HOME) {
    if (e == Event::AShort) {
      selected = (selected + 1) % APP_COUNT;
      displayShow(drawHome, false);
    } else if (e == Event::BShort || e == Event::ALong) {
      current = e == Event::ALong ? SETTINGS : selected;
      if (needsCard(current) && !storageCardMount()) {  // missing, unreadable or not FAT32
        noCardFor = current;
        current = NO_CARD;
        displayShow(drawNoCard, true);
        return;
      }
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

void launcherPoll() {
  // Live clock while awake: redraw when the minute on screen goes out of date. Asleep, the clock
  // chip keeps counting silently and the time catches up on the next press (no wake-ups, saving battery).
  if (current == HOME && clockValid()) {
    time_t t = time(nullptr);
    struct tm now;
    localtime_r(&t, &now);
    if (now.tm_min != shownMinute) displayTick(drawHome);
  }
  char name[32];
  if (!usbSyncTakeNewBadge(name, sizeof name)) return;
  storagePutString("badge", name);  // the Badge app opens on the saved badge
  if (current >= 0 || current == SETTINGS) app(current)->onExit();
  for (int i = 0; i < APP_COUNT; i++)
    if (APPS[i] == &badgeApp) current = i;
  app(current)->onEnter();
  displayShow(app(current)->draw, true);
}
