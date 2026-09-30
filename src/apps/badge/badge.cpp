// Name Badge: flips through 1-bit BMPs in /badges, full screen. A = next, B = previous.
#include <algorithm>
#include "../../core/app.h"
#include "../../core/display.h"
#include "../../core/storage.h"
#include "../../core/theme.h"

static const char *DIR = "/badges";
static const int MAX_BADGES = 32;

// RAM is lost in deep sleep, so the list is rebuilt on first use after a wake.
static String names[MAX_BADGES];
static int count = -1;  // -1 = not listed yet
RTC_DATA_ATTR static int current;

static void list() {
  count = 0;
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

// Uncompressed 1-bit BMP, up to 200x200 (smaller is centred). Either palette polarity.
static bool drawBmp(const String &name) {
  fs::File f = storageOpen((String(DIR) + "/" + name).c_str());
  uint8_t h[54];
  if (!f || f.read(h, sizeof h) != sizeof h || h[0] != 'B' || h[1] != 'M') return false;
  uint32_t pixels = le32(h + 10), palette = 14 + le32(h + 14);
  int32_t w = le32(h + 18), ht = le32(h + 22);
  bool topDown = ht < 0;
  if (topDown) ht = -ht;
  if ((h[28] | h[29] << 8) != 1 || le32(h + 30) != 0 || w <= 0 || w > 200 || ht > 200) return false;

  uint8_t entry0[4];  // B, G, R, 0
  f.seek(palette);
  if (f.read(entry0, 4) != 4) return false;
  bool zeroIsBlack = entry0[0] + entry0[1] + entry0[2] < 384;

  const uint32_t stride = ((w + 31) / 32) * 4;  // rows are padded to 4 bytes
  uint8_t row[28];
  int16_t x0 = (200 - w) / 2, y0 = (200 - ht) / 2;
  f.seek(pixels);
  for (int32_t r = 0; r < ht; r++) {
    if (f.read(row, stride) != stride) return false;
    int16_t y = y0 + (topDown ? r : ht - 1 - r);
    for (int32_t c = 0; c < w; c++) {
      bool one = row[c >> 3] & (0x80 >> (c & 7));
      if (one != zeroIsBlack) display.drawPixel(x0 + c, y, GxEPD_BLACK);
    }
  }
  return true;
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
  list();
  String last = storageGetString("badge");
  for (int i = 0; i < count; i++)
    if (names[i] == last) current = i;
}

static Redraw onButton(Event e) {
  ensureList();
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
  else if (!drawBmp(names[current])) drawMessage("can't read", names[current].c_str());
}

static void onExit() {}

extern const App badgeApp = {"Badge", ICON_BADGE, onEnter, onButton, draw, onExit};
