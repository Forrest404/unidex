// Badge: flips through 1-bit BMPs in /badges, full screen. A = next, B = picker (3x3 thumbnails:
// A = next, B = open, hold A = back), hold B = previous. The hints show as a band for a moment on opening.
#include "../../core/app.h"
#include "../../core/display.h"
#include "../../core/launcher.h"
#include "../../core/storage.h"
#include "../../core/theme.h"
#include "badge_file.h"

static const int MAX_BADGES = 32, PER_PAGE = 9;
static const int16_t THUMB = 40, STEP = THUMB + 4;  // thumbnail size and spacing: 3 rows fit above the hints

// RAM is lost in deep sleep, so the list is rebuilt on first use after a wake.
static String names[MAX_BADGES];
static int count = -1;  // -1 = not listed yet
RTC_DATA_ATTR static int current;
RTC_DATA_ATTR static bool picking;
RTC_DATA_ATTR static int cursor;  // selected thumbnail in the picker
static const uint32_t BAND_MS = 2500;
static uint32_t bandAt;     // when the hints band was shown over the badge
static bool band;           // it's on screen now
RTC_DATA_ATTR static int8_t shownCount = -1;  // badges on the card when last listed (for the home screen)

// Thumbnails, decoded once and kept in RAM (a 46x46 1-bit thumbnail is 276 bytes), so moving
// the cursor only costs the screen refresh. RAM is lost in deep sleep; they're rebuilt as needed.
static const int THUMB_ROW = (THUMB + 7) / 8;
static uint8_t thumbs[MAX_BADGES][THUMB * THUMB_ROW];
static bool thumbReady[MAX_BADGES];

static void choose(int i) {
  current = i;
  storagePutString("badge", names[i].c_str());
}

static void showBand() {
  band = true;
  bandAt = millis();
}

static void list() {
  memset(thumbReady, 0, sizeof thumbReady);  // the files may have changed
  count = badgeList(names, MAX_BADGES);
  if (current >= count) current = 0;
  shownCount = count;
}

static void ensureList() {
  if (count < 0) list();
}

static bool drawBmp(const String &name) {
  Bmp b;
  if (!badgeLoad(name.c_str(), b)) return false;
  badgeDraw(b);
  return true;
}


static void makeThumb(int i) {
  Bmp b;
  memset(thumbs[i], 0, sizeof thumbs[i]);
  thumbReady[i] = true;  // an unreadable badge stays blank
  if (!badgeLoad(names[i].c_str(), b)) return;
  for (int y = 0; y < THUMB; y++)
    for (int x = 0; x < THUMB; x++)  // nearest neighbour from the 200x200 frame
      if (b.black(x * 200 / THUMB, y * 200 / THUMB)) thumbs[i][y * THUMB_ROW + x / 8] |= 0x80 >> (x % 8);
}

// 3x3 thumbnails, 9 per page; the page follows the cursor.
static void drawPicker() {
  drawHeader("Badges");
  const int16_t grid = 3 * STEP - 4, gx = (display.width() - grid) / 2;
  const int16_t gy = CONTENT_TOP + (HINTS_TOP - CONTENT_TOP - grid) / 2;
  const int first = cursor / PER_PAGE * PER_PAGE;
  for (int i = first; i < count && i < first + PER_PAGE; i++) {
    const int16_t x = gx + (i - first) % 3 * STEP, y = gy + (i - first) / 3 * STEP;
    if (!thumbReady[i]) makeThumb(i);
    display.drawBitmap(x, y, thumbs[i], THUMB, THUMB, BLACK);
    if (i == cursor) {  // white gap, then a 2 px frame: stands out even on a dark badge
      display.drawRect(x - 1, y - 1, THUMB + 2, THUMB + 2, WHITE);
      display.drawRect(x - 2, y - 2, THUMB + 4, THUMB + 4, BLACK);
      display.drawRect(x - 3, y - 3, THUMB + 6, THUMB + 6, BLACK);
    } else {
      display.drawRect(x - 1, y - 1, THUMB + 2, THUMB + 2, BLACK);
    }
  }
  drawHints(count > 1 ? "next" : "", "open", "");
}

static void onEnter() {
  picking = false;
  list();
  String last = storageGetString("badge");
  for (int i = 0; i < count; i++)
    if (names[i] == last) current = i;
  showBand();
}

static Redraw onButton(Event e) {
  ensureList();
  if (count == 0) return Redraw::None;
  if (picking) {
    if (e == Event::AShort && count > 1) {
      cursor = (cursor + 1) % count;
      return Redraw::Partial;
    }
    if (e != Event::BShort) return Redraw::None;
    choose(cursor);
    picking = false;
    band = false;
    return Redraw::Full;  // a whole new image
  }
  if (e == Event::BShort) {
    picking = true;
    cursor = current;
    return Redraw::Partial;  // quick to open; picking a badge redraws it with a full refresh
  }
  if (count < 2) {  // nothing to flip to: say so, and show what the buttons do
    launcherToast("Only one badge");
    showBand();
    return Redraw::Partial;
  }
  band = false;
  choose(e == Event::AShort ? (current + 1) % count : (current + count - 1) % count);
  return Redraw::Full;  // a whole new image: a partial would ghost the previous badge
}

static Redraw onBack() {
  if (!picking) return Redraw::Exit;
  picking = false;  // back to the badge you had
  return Redraw::Full;
}

// The hints over the bottom of the badge, on a white band, for a moment after opening.
static void drawBand() {
  display.fillRect(0, HINTS_TOP - 3, display.width(), display.height() - HINTS_TOP + 3, WHITE);
  drawHints(count > 1 ? "next" : "", "pick", count > 1 ? "prev" : "");
}

static void draw() {
  ensureList();
  if (count == 0) {
    drawHeader("Badge");
    drawEmpty("No badges yet", "Make them on the", "website, in Tools");
    drawHints("", "", "");
  } else if (picking) {
    drawPicker();
  } else if (!drawBmp(names[current])) {
    drawHeader("Badge");
    drawEmpty("Can't read this one", names[current].c_str(), "Not a 1-bit BMP?");
    drawHints(count > 1 ? "next" : "", "pick", count > 1 ? "prev" : "");
  } else if (band) {
    drawBand();
  }
}

// Clears the hints band once its moment is over.
static Redraw tick() {
  if (!band || picking || millis() - bandAt < BAND_MS) return Redraw::None;
  band = false;
  return Redraw::Tick;
}

static void onExit() { band = false; }

static void status(char *out, size_t len) {
  static bool looked;  // after a restart, list the badges once so the line is there before the first visit
  if (shownCount < 0 && !looked) {
    looked = true;
    if (storageCardMount()) {
      list();
      const String last = storageGetString("badge");
      for (int i = 0; i < count; i++)
        if (names[i] == last) current = i;
    }
  }
  if (shownCount > 0) snprintf(out, len, "Badge %d of %d", current + 1, shownCount);
  else if (shownCount == 0) snprintf(out, len, "No badges yet");
  else snprintf(out, len, "Show a name badge");
}

extern const App badgeApp = {"Badge", ICON_BADGE, onEnter, onButton, draw, onExit, onBack, status, tick, true};
