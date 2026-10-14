#include "scripts.h"
#include <vector>
#include "appstore.h"
#include "luahost.h"
#include "../../core/app.h"
#include "../../core/display.h"
#include "../../core/launcher.h"
#include "../../core/theme.h"
#include "../../core/devtools.h"
#include "../../core/storage.h"

// The list of installed apps, an app running, the offload/delete choice and its "sure?", and "App error".
enum Screen : uint8_t { LIST, RUN, OPTIONS, CONFIRM, FAILED };
static const int ROWS = 5, ROW_H = 26;
static const uint32_t TICK_MS = 1000;  // an app's tick(): at most once a second (e-paper, battery)

RTC_DATA_ATTR static Screen screen;
RTC_DATA_ATTR static uint8_t cursor, option;  // option: 0 offload, 1 delete
RTC_DATA_ATTR static char runId[24 + 1];      // the app open, kept through deep sleep (its Lua state isn't)
RTC_DATA_ATTR static int8_t appCount = -1;    // for the home screen line
static std::vector<AppEntry> apps_;
static bool listed, changed;
static String failMsg, failTitle;
static uint64_t confirmData;
static uint32_t lastTick;

static void ensureList() {
  if (listed) return;
  apps_ = appsList(false);
  listed = true;
  appCount = apps_.size();
  if (cursor >= apps_.size()) cursor = 0;
}

static const AppEntry *selected() { return cursor < apps_.size() ? &apps_[cursor] : nullptr; }

bool scriptsRunning(const char *id) { return screen == RUN && strcmp(runId, id) == 0; }

void scriptsListChanged() {
  listed = false;
  changed = true;
}

static void showFailure(const char *title, const String &why) {
  failTitle = title;
  failMsg = why;
  runId[0] = 0;
  screen = FAILED;
}

// Opens `runId` in the Lua host: on first open, and again after a deep sleep (which loses the Lua state).
static bool ensureRunning() {
  if (screen != RUN) return false;
  if (hostRunning() && hostId() == runId) return true;
  apps::Manifest m;
  String problem;
  if (!appsManifest(runId, m, problem)) {
    showFailure("Can't open", problem);
    return false;
  }
  if (!hostStart(runId, m.perms)) {
    showFailure("App error", hostError());
    return false;
  }
  lastTick = millis();
  return true;
}

// After any call into the app: if it failed, the error screen takes over (a full refresh).
static Redraw checked(Redraw r) {
  if (screen == RUN && !hostRunning()) {
    showFailure("App error", hostError());
    return Redraw::Full;
  }
  return r;
}

#if UNIDEX_DEV
static const char *detail() {  // test tools: X STATE detail=
  static char d[96];
  const AppEntry *e = selected();
  snprintf(d, sizeof d, "%s:%s", screen == RUN ? "run" : screen == LIST ? "list" : screen == OPTIONS ? "options"
                                 : screen == CONFIRM ? "confirm" : "failed",
           screen == RUN ? runId : screen == FAILED ? failMsg.c_str() : e ? e->id.c_str() : "");
  return d;
}

#endif

// --- drawing ---
static void drawRows() {
  drawHeader("Apps", apps_.size() > ROWS ? (String(cursor + 1) + "/" + apps_.size()).c_str() : nullptr);
  const int first = cursor / ROWS * ROWS;
  for (int r = 0; r < ROWS && first + r < (int)apps_.size(); r++) {
    const int16_t top = CONTENT_TOP + 4 + r * ROW_H, baseline = top + 17;
    const bool sel = first + r == cursor;
    const AppEntry &e = apps_[first + r];
    if (sel) display.fillRect(MARGIN - 4, top, display.width() - 2 * (MARGIN - 4), ROW_H - 2, BLACK);
    display.setTextColor(sel ? WHITE : BLACK);
    const char *tag = e.offloaded ? "offloaded" : e.problem.length() ? "can't run" : "";
    display.setFont(FONT_TINY);
    drawRight(tag, baseline);
    const int16_t tagW = *tag ? textWidth(tag) + 6 : 0;
    display.setFont(FONT_SMALL);
    display.setCursor(MARGIN, baseline);
    display.print(fitText(e.name.c_str(), display.width() - 2 * MARGIN - tagW));
    display.setTextColor(BLACK);
  }
}

static void drawList() {
  drawRows();
  if (apps_.empty()) {
    drawEmpty("No apps yet", "Add some from the", "Store on the website");
    drawHints("", "", "");
    return;
  }
  const AppEntry *e = selected();
  drawHints(apps_.size() > 1 ? "next" : "", e && e->offloaded ? "" : "open", "remove");
}

static void drawOptions() {
  const AppEntry *e = selected();
  drawHeader(e ? fitText(e->name.c_str(), 150).c_str() : "App");
  const char *const ROWS_TEXT[] = {"Offload", "Delete"}, *const NOTES[] = {"keeps its data", "and its data"};
  const bool canOffload = e && !e->offloaded;
  for (int r = 0; r < 2; r++) {
    const int16_t top = CONTENT_TOP + 10 + r * 40;
    const bool sel = r == option, off = r == 0 && !canOffload;
    if (sel) display.fillRect(MARGIN - 4, top, display.width() - 2 * (MARGIN - 4), 36, BLACK);
    display.setTextColor(sel ? WHITE : BLACK);
    display.setFont(FONT_SMALL);
    display.setCursor(MARGIN, top + 16);
    display.print(off ? "Offloaded already" : ROWS_TEXT[r]);
    display.setFont(FONT_TINY);
    display.setCursor(MARGIN, top + 30);
    display.print(off ? "only its data is left" : NOTES[r]);
    display.setTextColor(BLACK);
  }
  drawHints(canOffload ? "next" : "", "choose", "");
}

static String kb(uint64_t bytes) {
  if (bytes < 1024) return String((unsigned)bytes) + (bytes == 1 ? " byte" : " bytes");
  return String((unsigned)((bytes + 1023) / 1024)) + " KB";
}

static void drawConfirm() {
  drawRows();
  const AppEntry *e = selected();
  const String name = e ? e->name : String("this app");
  if (option == 0) {
    drawSheet(("Offload " + name + "?").c_str(), "Its data stays. Install", "it again to use it.");
    drawHints("keep", "offload", "");
  } else {
    const String lost = confirmData ? "Its data (" + kb(confirmData) + ")" : String("Its settings");
    drawSheet(("Delete " + name + "?").c_str(), lost.c_str(), "will be lost for good.");
    drawHints("keep", "delete", "");
  }
}

static void drawFailed() {
  display.fillScreen(WHITE);
  drawHeader(failTitle.length() ? failTitle.c_str() : "App error");
  display.setFont(FONT_SMALL);
  String lines[6];
  const int need = wrapText(failMsg.length() ? failMsg.c_str() : "Something went wrong.", display.width() - 2 * MARGIN,
                            lines, 6);
  for (int i = 0; i < need && i < 6; i++) {
    display.setCursor(MARGIN, CONTENT_TOP + 20 + i * 19);
    display.print(lines[i]);
  }
  drawHints("", "ok", "");
}

static void draw() {
  ensureList();
#if UNIDEX_DEV
  devSetDetail(detail);  // (again after a deep sleep, which forgets it)
#endif
  if (screen == RUN && ensureRunning()) {
    hostDraw();
    if (hostRunning()) return;
    showFailure("App error", hostError());
  }
  switch (screen) {
    case OPTIONS: drawOptions(); break;
    case CONFIRM: drawConfirm(); break;
    case FAILED: drawFailed(); break;
    default: drawList(); break;
  }
}

// --- buttons ---
static void openSelected() {
  const AppEntry *e = selected();
  if (!e) return;
  if (e->problem.length()) {
    showFailure("Can't open", e->problem);
    return;
  }
  strlcpy(runId, e->id.c_str(), sizeof runId);
  screen = RUN;
  ensureRunning();  // a failure shows the error screen instead
}

static Redraw onButton(Event e) {
  ensureList();
  switch (screen) {
    case RUN: {
      if (!ensureRunning()) return Redraw::Full;
      const char *fn = e == Event::AShort ? "onA" : e == Event::BShort ? "onB" : e == Event::BLong ? "onBHold" : nullptr;
      return fn ? checked(hostEvent(fn)) : Redraw::None;
    }
    case LIST:
      if (apps_.empty()) return Redraw::None;
      if (e == Event::AShort && apps_.size() > 1) {
        cursor = (cursor + 1) % apps_.size();
        return Redraw::Partial;
      }
      if (e == Event::BShort && !selected()->offloaded) {
        openSelected();
        return Redraw::Full;
      }
      if (e == Event::BLong || (e == Event::BShort && selected()->offloaded)) {
        option = selected()->offloaded ? 1 : 0;
        screen = OPTIONS;
        return Redraw::Partial;
      }
      return Redraw::None;
    case OPTIONS:
      if (e == Event::AShort && selected() && !selected()->offloaded) {
        option ^= 1;
        return Redraw::Partial;
      }
      if (e == Event::BShort && selected()) {
        confirmData = storageTreeBytes((appsDir(selected()->id) + "/data").c_str());
        screen = CONFIRM;
        return Redraw::Partial;
      }
      return Redraw::None;
    case CONFIRM:
      if (e == Event::AShort) {
        screen = LIST;
        return Redraw::Partial;
      }
      if (e == Event::BShort && selected()) {
        const bool ok = appsRemove(selected()->id, option == 0);
        launcherToast(!ok ? "Couldn't remove it" : option == 0 ? "Offloaded" : "Deleted");
        listed = false;
        screen = LIST;
        return Redraw::Full;
      }
      return Redraw::None;
    case FAILED:
      if (e == Event::BShort) {
        screen = LIST;
        listed = false;
        return Redraw::Full;
      }
      return Redraw::None;
  }
  return Redraw::None;
}

static Redraw onBack() {
  switch (screen) {
    case RUN: {
      if (!ensureRunning()) return Redraw::Full;
      const Redraw r = hostBack();
      if (!hostRunning() && hostError().length()) return checked(r);
      if (r != Redraw::Exit) return r;
      hostStop();
      runId[0] = 0;
      screen = LIST;
      listed = false;
      return Redraw::Full;
    }
    case LIST:
      return Redraw::Exit;
    default:
      screen = LIST;
      listed = false;
      return Redraw::Full;
  }
}

static Redraw tick() {
  if (screen == LIST && changed) {
    changed = false;
    return Redraw::Partial;
  }
  if (screen != RUN || millis() - lastTick < TICK_MS) return Redraw::None;
  lastTick = millis();
  if (!ensureRunning()) return Redraw::Full;
  if (!hostHas("tick")) return Redraw::None;
  const Redraw r = checked(hostEvent("tick"));
  return r == Redraw::Partial ? Redraw::Tick : r;  // live content: a clock moving on doesn't add to the full-refresh count
}

static void onEnter() {
  screen = LIST;
  runId[0] = 0;
  listed = false;
}

static void onExit() {
  hostStop();
  runId[0] = 0;
  screen = LIST;
}

static void status(char *out, size_t len) {
  if (appCount < 0) *out = 0;
  else if (appCount == 0) snprintf(out, len, "Add apps from the website");
  else snprintf(out, len, "%d app%s", appCount, appCount == 1 ? "" : "s");
}


static void onEnterDev() {
  onEnter();
#if UNIDEX_DEV
  devSetDetail(detail);
#endif
}

static void onExitDev() {
  onExit();
#if UNIDEX_DEV
  devSetDetail(nullptr);
#endif
}

extern const App scriptsApp = {"Apps", ICON_APPS, onEnterDev, onButton, draw, onExitDev, onBack, status, tick, true};
