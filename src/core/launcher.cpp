#include "launcher.h"
#include <Arduino.h>
#include "battery.h"
#include "clock.h"
#include "display.h"
#include "storage.h"
#include "usbsync.h"
#include "theme.h"
#include "../apps/apps.h"

static const int HOME = -1, NO_CARD = -2;
RTC_DATA_ATTR static int current = HOME;  // open app, HOME or NO_CARD
RTC_DATA_ATTR static int noCardFor;       // the app that couldn't open
RTC_DATA_ATTR static int selected;        // highlighted app on the home screen

static const App *open() { return APPS[current]; }

// --- toast: one short message over the screen, cleared after a few seconds ---

static const uint32_t TOAST_MS = 2500;
static char toastText[40];
static uint32_t toastAt;

void launcherToast(const char *text) {
  strlcpy(toastText, text, sizeof toastText);
  toastAt = millis();
}

static void drawToastIfAny() {
  if (*toastText) drawToast(toastText);
}

// --- home ---

static const int16_t ICON_Y = CONTENT_TOP + 2, NAME_BASE = 127, LINE_BASE = 146, DOTS_Y = 157;

// Small lightning bolt, 7 px wide and 12 tall, with its top-left corner at (x, y).
static void drawBolt(int16_t x, int16_t y) {
  display.fillTriangle(x + 4, y, x, y + 7, x + 4, y + 7, BLACK);
  display.fillTriangle(x + 3, y + 5, x + 7, y + 5, x + 3, y + 12, BLACK);
}

// Top right of the home header: "14:32  87%", with a bolt before it on USB power.
// Either text part is left out when it isn't known.
static int shownMinute = -1;  // the minute the home clock shows, -1 = none
static char shownLine[40];    // the selected app's status line as last drawn

// The home header: the time on the left, the battery (and a bolt while charging) on the right.
static void drawStatus() {
  char text[16] = "";
  shownMinute = -1;
  if (clockValid()) {
    const struct tm now = clockLocal();
    snprintf(text, sizeof text, "%02d:%02d", now.tm_hour, now.tm_min);
    shownMinute = now.tm_min;
  }
  drawHeader(text);
  *text = 0;
  const int pct = batteryPercent();
  if (pct >= 0) snprintf(text, sizeof text, "%d%%", pct);
  uint16_t w = 0;
  if (*text) {
    display.setFont(FONT_SMALL);
    w = textWidth(text);
    drawRight(text, 16);
  }
  if (batteryCharging()) drawBolt(display.width() - MARGIN - w - (w ? 12 : 7), 5);
}

static void appStatus(const App *a, char *out, size_t len) {
  *out = 0;
  if (a->status) a->status(out, len);
}

// Home is a carousel: one app at a time, its icon at double size, its name, a live line, a dot per app.
static void drawHome() {
  drawStatus();
  const App *a = APPS[selected];
  drawIcon(a->icon, (display.width() - 2 * ICON_SIZE) / 2, ICON_Y, BLACK, 2);
  display.setFont(FONT_MEDIUM);
  drawCenteredLine(a->name, NAME_BASE);
  appStatus(a, shownLine, sizeof shownLine);
  if (*shownLine) {
    display.setFont(FONT_SMALL);
    drawCenteredLine(fitText(shownLine, display.width() - 2 * MARGIN).c_str(), LINE_BASE);
  }
  drawPageDots(APP_COUNT, selected, DOTS_Y);
  drawHints("next", "open", "");
  drawToastIfAny();
}

static void drawSplash() {
  // Mark: 2x2 squares, one filled.
  const int16_t S = 14, G = 4, mx = (200 - 2 * S - G) / 2, my = 56;
  display.drawRect(mx, my, S, S, BLACK);
  display.drawRect(mx + S + G, my, S, S, BLACK);
  display.drawRect(mx, my + S + G, S, S, BLACK);
  display.fillRect(mx + S + G, my + S + G, S, S, BLACK);
  display.setFont(FONT_LARGE);
  drawCentered("unidex", 128);
}

// --- restart ---

static const char *restartWhy = "";

static void drawRestarting() {
  display.setFont(FONT_LARGE);
  drawCentered("Restarting", 88);
  display.setFont(FONT_SMALL);
  if (*restartWhy) drawCentered(restartWhy, 118);
  drawCentered("let go of the buttons", *restartWhy ? 140 : 124);
}

void systemRestart(const char *why) {
  restartWhy = why;
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

// --- opening apps ---

static void drawNoCard() {
  drawHeader(APPS[noCardFor]->name);
  drawEmpty("No SD card", "Put in a FAT32 card,", "then press B");
  drawHints("", "retry", "");
  drawToastIfAny();
}

static void drawApp() {
  open()->draw();
  drawToastIfAny();
}

static void showApp(Redraw r) {
  if (r == Redraw::Full || r == Redraw::Partial) displayShow(drawApp, r == Redraw::Full);
  else if (r == Redraw::Tick) displayTick(drawApp);
}

static void goHome() {
  current = HOME;
  displayShow(drawHome, true);
}

static void openApp(int i) {
  if (APPS[i]->needsCard && !storageCardMount()) {  // missing, unreadable or not FAT32
    noCardFor = i;
    current = NO_CARD;
    displayShow(drawNoCard, true);
    return;
  }
  current = i;
  open()->onEnter();
  displayShow(drawApp, true);
}

void launcherHandle(Event e) {
  if (e == Event::Reset) systemRestart("");
  *toastText = 0;  // any press clears a message
  if (current == NO_CARD) {
    if (e == Event::ALong) goHome();
    if (e == Event::BShort) {
      if (storageCardMount()) {
        openApp(noCardFor);
        return;
      }
      launcherToast("Still no card");
      displayShow(drawNoCard, false);
    }
    return;
  }
  if (current == HOME) {
    if (e == Event::AShort || e == Event::ALong) {
      selected = (selected + (e == Event::AShort ? 1 : APP_COUNT - 1)) % APP_COUNT;
      displayShow(drawHome, false);
    } else if (e == Event::BShort) {
      openApp(selected);
    }
    return;
  }
  Redraw r;
  if (e == Event::ALong) r = open()->onBack ? open()->onBack() : Redraw::Exit;
  else r = open()->onButton(e);
  if (r == Redraw::Exit) {
    open()->onExit();
    goHome();
  } else {
    showApp(r);
  }
}

const char *launcherScreenName() {
  return current == HOME ? "Home" : current == NO_CARD ? "No card" : open()->name;
}

const char *launcherSelectedName() { return APPS[selected]->name; }

void launcherPoll() {
  const bool toastOver = *toastText && millis() - toastAt >= TOAST_MS;
  if (toastOver) *toastText = 0;
  if (current == HOME) {
    // Live clock and status line while awake: redraw when what's on screen goes out of date. Asleep, the
    // clock chip keeps counting silently and the time catches up on the next press (no wake-ups).
    char line[sizeof shownLine];
    appStatus(APPS[selected], line, sizeof line);
    const bool minuteChanged = clockValid() && clockLocal().tm_min != shownMinute;
    if (toastOver || minuteChanged || strcmp(line, shownLine) != 0) displayTick(drawHome);
  } else if (current == NO_CARD) {
    if (toastOver) displayTick(drawNoCard);
  } else {
    const Redraw r = open()->tick ? open()->tick() : Redraw::None;
    if (r != Redraw::None) showApp(r);
    else if (toastOver) displayTick(drawApp);
  }
  char name[32];
  if (!usbSyncTakeNewBadge(name, sizeof name)) return;
  storagePutString("badge", name);  // the Badge app opens on the saved badge
  if (current >= 0) open()->onExit();
  for (int i = 0; i < APP_COUNT; i++)
    if (APPS[i] == &badgeApp) current = i;
  open()->onEnter();
  displayShow(drawApp, true);
}
