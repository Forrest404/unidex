#include "badge_file.h"
#include <algorithm>
#include "../../core/display.h"
#include "../../core/storage.h"

static uint32_t le32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }

bool Bmp::black(int32_t x, int32_t y) const {
  x -= x0;
  y -= y0;
  if (x < 0 || x >= w || y < 0 || y >= h) return false;
  const uint8_t *row = data + pixels + (topDown ? y : h - 1 - y) * stride;
  return (bool)(row[x >> 3] & (0x80 >> (x & 7))) != zeroIsBlack;
}

int badgeList(String *names, int max) {
  int count = 0;
  fs::File dir = storageOpen(BADGE_DIR);
  if (!dir || !dir.isDirectory()) return 0;
  for (fs::File f = dir.openNextFile(); f && count < max; f = dir.openNextFile()) {
    String n = f.name();
    if (n.endsWith(".bmp") && !n.startsWith(".")) names[count++] = n;  // skip macOS "._" files on the card
  }
  std::sort(names, names + count);
  return count;
}

bool badgeNameOk(const char *n) {
  const size_t len = strlen(n);
  if (len < 5 || len >= 32 || strcmp(n + len - 4, ".bmp") != 0) return false;
  for (size_t i = 0; i < len - 4; i++)
    if (!(islower(n[i]) || isdigit(n[i]) || n[i] == '-')) return false;
  return true;
}

bool bmpCheck(const uint8_t *h, size_t size, Bmp &b) {
  if (!h || size < 62 || size > BADGE_MAX_BYTES) return false;
  if (h[0] != 'B' || h[1] != 'M' || (h[28] | h[29] << 8) != 1 || le32(h + 30) != 0) return false;
  b.data = h;
  b.size = size;
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

// Read into RAM in one go (a 200x200 one is 5.6 KB): one card read, not one per row.
bool badgeLoad(const char *name, Bmp &b) {
  fs::File f = storageOpen((String(BADGE_DIR) + "/" + name).c_str());
  const size_t size = f ? f.size() : 0;
  if (size < 62 || size > BADGE_MAX_BYTES || !(b.owned = (uint8_t *)malloc(size)) || f.read(b.owned, size) != size)
    return false;
  return bmpCheck(b.owned, size, b);
}

void badgeDraw(const Bmp &b) {
  for (int16_t y = 0; y < 200; y++)
    for (int16_t x = 0; x < 200; x++)
      if (b.black(x, y)) display.drawPixel(x, y, BLACK);
}

void badgeDrawFit(const Bmp &b, int16_t x0, int16_t y0, int16_t size) {
  for (int16_t y = 0; y < size; y++)
    for (int16_t x = 0; x < size; x++)
      if (b.black(x * 200 / size, y * 200 / size)) display.drawPixel(x0 + x, y0 + y, BLACK);
  display.drawRect(x0 - 2, y0 - 2, size + 4, size + 4, BLACK);  // the badge's edge, so a white one still reads
}

String badgeFreeName(const char *name) {
  const String base = String(name).substring(0, strlen(name) - 4);
  String n = name;
  for (int i = 2; storageExists((String(BADGE_DIR) + "/" + n).c_str()) && i < 100; i++) {
    n = base.substring(0, 31 - 4 - 3) + "-" + i + ".bmp";  // stays under 32 characters
  }
  return n;
}

bool badgeSave(const char *name, const uint8_t *data, size_t size) {
  static const char *TMP = "/badges/radio.tmp";
  fs::File f = storageOpen(TMP, "w");  // (makes /badges on a new card)
  if (!f) return false;
  const bool ok = f.write(data, size) == size;
  f.close();
  if (!ok) {
    storageRemove(TMP);
    return false;
  }
  return storageRename(TMP, (String(BADGE_DIR) + "/" + name).c_str());
}
