// Name Badge: flips through 1-bit BMPs in /badges, full screen. A = next, B = previous,
// hold B = picker (3x3 thumbnails: A = next, B = open, hold B = back).
#include <algorithm>
#include "../../core/app.h"
#include "../../core/display.h"
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

// Thumbnails, decoded once and kept in RAM (a 46x46 1-bit thumbnail is 276 bytes), so moving
// the cursor only costs the screen refresh. RAM is lost in deep sleep; they're rebuilt as needed.
static const int THUMB_ROW = (THUMB + 7) / 8;
static uint8_t thumbs[MAX_BADGES][THUMB * THUMB_ROW];
static bool thumbReady[MAX_BADGES];

static void list() {
  count = 0;
  memset(thumbReady, 0, sizeof thumbReady);  // the files may have changed
  fs::File dir = storageOpen(DIR);
  if (!dir || !dir.isDirectory()) return;
  for (fs::File f = dir.openNextFile(); f && count < MAX_BADGES; f = dir.openNextFile()) {
    String n = f.name();
    if (n.endsWith(".bmp")) names[count++] = n;
  }
  std::sort(names, names + count);
  if (current >= count) current = 0;
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
  drawHeader("Badges");
  drawRight(pos, 16);
  const int16_t grid = 3 * STEP - 4, gx = (display.width() - grid) / 2;
  const int16_t gy = CONTENT_TOP + (CONTENT_BOTTOM - CONTENT_TOP - grid) / 2;
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
  drawFooter("next", "open");
}

static void drawMessage(const char *line1, const char *line2) {
  const int16_t mid = (CONTENT_TOP + CONTENT_BOTTOM) / 2;
  drawHeader("Badge");
  display.setFont(FONT_SMALL);
  drawCentered(line1, mid - 12);
  drawCentered(line2, mid + 12);
  drawFooter("hold home", "");
}

static void onEnter() {
  picking = false;
  list();
  String last = storageGetString("badge");
  for (int i = 0; i < count; i++)
    if (names[i] == last) current = i;
}

static Redraw onButton(Event e) {
  ensureList();
  if (picking) {
    if (e == Event::AShort) {
      cursor = (cursor + 1) % count;
      return Redraw::Partial;
    }
    if (e == Event::BShort) {
      current = cursor;
      storagePutString("badge", names[current].c_str());
    } else if (e != Event::BLong) {
      return Redraw::None;
    }
    picking = false;  // B opens the picked badge, hold B goes back to the one you had
    return Redraw::Full;
  }
  if (e == Event::BLong && count > 0) {
    picking = true;
    cursor = current;
    return Redraw::Partial;  // quick to open; picking a badge redraws it with a full refresh
  }
  if (count < 2) return Redraw::None;
  if (e == Event::AShort) current = (current + 1) % count;
  else if (e == Event::BShort) current = (current + count - 1) % count;
  else return Redraw::None;
  storagePutString("badge", names[current].c_str());
  return Redraw::Full;  // a whole new image: a partial would ghost the previous badge
}

static void draw() {
  ensureList();
  if (count == 0) drawMessage("no badges", "add BMPs to /badges");
  else if (picking) drawPicker();
  else if (!drawBmp(names[current])) drawMessage("can't read", names[current].c_str());
}

static void onExit() {}

extern const App badgeApp = {"Badge", ICON_BADGE, onEnter, onButton, draw, onExit};
