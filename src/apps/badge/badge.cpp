// Badge: flips through 1-bit BMPs in /badges, full screen. A = next, B = picker (3x3 thumbnails:
// A = next, B = open, hold A = back), hold B = previous. The hints show as a band for a moment on opening.
#include <algorithm>
#include "../../core/app.h"
#include "../../core/display.h"
#include "../../core/launcher.h"
#include "../../core/storage.h"
#include "../../core/theme.h"

static const char *DIR = "/badges";
static const int MAX_BADGES = 32, PER_PAGE = 9;
static const int16_t THUMB = 46, STEP = THUMB + 4;  // thumbnail size and spacing (4 px gaps)

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
  count = 0;
  memset(thumbReady, 0, sizeof thumbReady);  // the files may have changed
  fs::File dir = storageOpen(DIR);
  if (!dir || !dir.isDirectory()) return;
  for (fs::File f = dir.openNextFile(); f && count < MAX_BADGES; f = dir.openNextFile()) {
    String n = f.name();
    if (n.endsWith(".bmp") && !n.startsWith(".")) names[count++] = n;  // skip macOS "._" files on the card
  }
  std::sort(names, names + count);
  if (current >= count) current = 0;
  shownCount = count;
}

static void ensureList() {
  if (count < 0) list();
}

static uint32_t le32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }

// A 1-bit BMP read into RAM in one go (a 200x200 one is 5.6 KB): one flash read, not one per row.
struct Bmp {
  uint8_t *data = nullptr;
  int32_t w = 0, h = 0, x0 = 0, y0 = 0;  // size, and offset that centres it in the 200x200 frame
  uint32_t pixels = 0, stride = 0;
  bool topDown = false, zeroIsBlack = true;
  ~Bmp() { free(data); }

  // Black at (x, y) of the 200x200 frame?
  bool black(int32_t x, int32_t y) const {
    x -= x0;
    y -= y0;
    if (x < 0 || x >= w || y < 0 || y >= h) return false;
    const uint8_t *row = data + pixels + (topDown ? y : h - 1 - y) * stride;
    return (bool)(row[x >> 3] & (0x80 >> (x & 7))) != zeroIsBlack;
  }
};

// Uncompressed 1-bit BMP, up to 200x200 (smaller is centred). Either palette polarity.
static bool loadBmp(const String &name, Bmp &b) {
  fs::File f = storageOpen((String(DIR) + "/" + name).c_str());
  const size_t size = f ? f.size() : 0;
  if (size < 62 || size > 16384 || !(b.data = (uint8_t *)malloc(size)) || f.read(b.data, size) != size) return false;
  const uint8_t *h = b.data;
  if (h[0] != 'B' || h[1] != 'M' || (h[28] | h[29] << 8) != 1 || le32(h + 30) != 0) return false;
  b.pixels = le32(h + 10);
  b.w = le32(h + 18);
  b.h = (int32_t)le32(h + 22);
  b.topDown = b.h < 0;
  if (b.topDown) b.h = -b.h;
  if (b.w <= 0 || b.w > 200 || b.h <= 0 || b.h > 200) return false;
  b.stride = ((b.w + 31) / 32) * 4;  // rows are padded to 4 bytes
  const uint32_t palette = 14 + le32(h + 14);
  if (palette + 4 > size || b.pixels + b.stride * b.h > size) return false;
  b.zeroIsBlack = h[palette] + h[palette + 1] + h[palette + 2] < 384;  // entry 0 is B, G, R
  b.x0 = (200 - b.w) / 2;
  b.y0 = (200 - b.h) / 2;
  return true;
}

static bool drawBmp(const String &name) {
  Bmp b;
  if (!loadBmp(name, b)) return false;
  for (int16_t y = 0; y < 200; y++)
    for (int16_t x = 0; x < 200; x++)
      if (b.black(x, y)) display.drawPixel(x, y, GxEPD_BLACK);
  return true;
}


static void makeThumb(int i) {
  Bmp b;
  memset(thumbs[i], 0, sizeof thumbs[i]);
  thumbReady[i] = true;  // an unreadable badge stays blank
  if (!loadBmp(names[i], b)) return;
  for (int y = 0; y < THUMB; y++)
    for (int x = 0; x < THUMB; x++)  // nearest neighbour from the 200x200 frame
      if (b.black(x * 200 / THUMB, y * 200 / THUMB)) thumbs[i][y * THUMB_ROW + x / 8] |= 0x80 >> (x % 8);
}

// 3x3 thumbnails, 9 per page; the page follows the cursor.
static void drawPicker() {
  char pos[12];
  snprintf(pos, sizeof pos, "%d/%d", cursor + 1, count);
  drawHeader("Badges", pos);
  const int16_t grid = 3 * STEP - 4, gx = (display.width() - grid) / 2;
  const int16_t gy = CONTENT_TOP + (HINTS_TOP - CONTENT_TOP - grid) / 2;
  const int first = cursor / PER_PAGE * PER_PAGE;
  for (int i = first; i < count && i < first + PER_PAGE; i++) {
    const int16_t x = gx + (i - first) % 3 * STEP, y = gy + (i - first) / 3 * STEP;
    if (!thumbReady[i]) makeThumb(i);
    display.drawBitmap(x, y, thumbs[i], THUMB, THUMB, GxEPD_BLACK);
    if (i == cursor) {  // white gap, then a 2 px frame: stands out even on a dark badge
      display.drawRect(x - 1, y - 1, THUMB + 2, THUMB + 2, GxEPD_WHITE);
      display.drawRect(x - 2, y - 2, THUMB + 4, THUMB + 4, GxEPD_BLACK);
      display.drawRect(x - 3, y - 3, THUMB + 6, THUMB + 6, GxEPD_BLACK);
    } else {
      display.drawRect(x - 1, y - 1, THUMB + 2, THUMB + 2, GxEPD_BLACK);
    }
  }
  drawHints(count > 1 ? "next" : "", "open", "back", "");
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
  display.fillRect(0, HINTS_TOP - 3, display.width(), display.height() - HINTS_TOP + 3, GxEPD_WHITE);
  drawHints(count > 1 ? "next" : "", "pick", "home", count > 1 ? "prev" : "");
}

static void draw() {
  ensureList();
  if (count == 0) {
    drawHeader("Badge");
    drawEmpty("No badges yet", "Make them on the", "website, in Tools");
    drawHints("", "", "home", "");
  } else if (picking) {
    drawPicker();
  } else if (!drawBmp(names[current])) {
    drawHeader("Badge");
    drawEmpty("Can't read this one", names[current].c_str(), "Not a 1-bit BMP?");
    drawHints(count > 1 ? "next" : "", "pick", "home", count > 1 ? "prev" : "");
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
