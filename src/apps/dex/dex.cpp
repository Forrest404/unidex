// WiFi Pokedex: scan on demand, log first sightings to /dex.csv with salted, hashed BSSIDs.
// One entry per network name (many access points share one); hidden networks are one entry each.
// A = list of all finds (A pages, B back), B = scan, B long = counts per rarity (hold B there to clear).
#include <WiFi.h>
#include <mbedtls/sha256.h>
#include "../../core/app.h"
#include "../../core/display.h"
#include "../../core/power.h"
#include "../../core/storage.h"
#include "../../core/theme.h"

enum Rarity : uint8_t { COMMON, UNCOMMON, STARTER, RARE };  // ascending: RARE is the best find
static const char *RARITY[] = {"common", "uncommon", "starter", "rare"};

static const char *DEX = "/dex.csv";
static const int MAX_KNOWN = 2000, PAGE = 5;
static const int WEAK_RSSI = -80;

struct Find {
  char ssid[33];
  uint8_t rarity;
  uint16_t tag;  // top of the BSSID hash: tells hidden networks apart on screen
};

// RAM is lost in deep sleep, so the dex is loaded again on first use after a wake.
static uint64_t known[MAX_KNOWN];  // one key per entry: see keyOf()
static int knownCount = -1;  // -1 = not loaded yet
static int perRarity[4];
static Find newest;          // the last row in the file
static uint8_t salt[16];
static bool scanned;         // the last scan's summary below is valid
static int nearby, newCount;
static Find best;

enum View : uint8_t { MAIN, LIST, STATS, CONFIRM };
RTC_DATA_ATTR static uint8_t view, page;

static const char *shown(const Find &f) {
  static char hidden[16];
  if (*f.ssid) return f.ssid;
  snprintf(hidden, sizeof hidden, "(hidden %04x)", f.tag);
  return hidden;
}

static void loadSalt() {
  String hex = storageGetString("dex_salt");
  if (hex.length() != 32) {
    char buf[33];
    for (int i = 0; i < 16; i++) {
      salt[i] = esp_random();
      sprintf(buf + 2 * i, "%02x", salt[i]);
    }
    storagePutString("dex_salt", buf);  // written once, ever
    return;
  }
  for (int i = 0; i < 16; i++) salt[i] = strtoul(hex.substring(2 * i, 2 * i + 2).c_str(), nullptr, 16);
}

// Salted, because a BSSID is only 48 bits with known vendor prefixes: a plain hash could be reversed.
static uint64_t hashBssid(const uint8_t *bssid) {
  uint8_t in[22], out[32];
  memcpy(in, salt, 16);
  memcpy(in + 16, bssid, 6);
  mbedtls_sha256_ret(in, sizeof in, out, 0);
  uint64_t h = 0;
  for (int i = 0; i < 8; i++) h = h << 8 | out[i];
  return h;
}

// Named networks are keyed by their name, so extra access points with the same name don't count
// again; hidden ones have no name, so they're keyed by their BSSID hash.
static uint64_t keyOf(const char *ssid, uint64_t bssidHash) {
  if (!*ssid) return bssidHash;
  uint8_t out[32];
  mbedtls_sha256_ret((const uint8_t *)ssid, strlen(ssid), out, 0);
  uint64_t k = 0;
  for (int i = 0; i < 8; i++) k = k << 8 | out[i];
  return k;
}

static bool isKnown(uint64_t h) {
  for (int i = 0; i < knownCount; i++)
    if (known[i] == h) return true;
  return false;
}

static void remember(uint64_t bssidHash, const char *ssid, uint8_t rarity) {
  if (knownCount < MAX_KNOWN) known[knownCount++] = keyOf(ssid, bssidHash);
  perRarity[rarity]++;
  strlcpy(newest.ssid, ssid, sizeof newest.ssid);
  newest.rarity = rarity;
  newest.tag = bssidHash >> 48;
}

static void load() {
  knownCount = 0;
  memset(perRarity, 0, sizeof perRarity);
  loadSalt();
  fs::File f = storageOpen(DEX);
  while (f && f.available()) {
    String row = f.readStringUntil('\n');  // hash,ssid,rssi,enc,rarity,first_seen
    int c1 = row.indexOf(','), c2 = row.indexOf(',', c1 + 1), c3 = row.indexOf(',', c2 + 1);
    int c4 = row.indexOf(',', c3 + 1), c5 = row.indexOf(',', c4 + 1);
    if (c1 != 16 || c5 < 0) continue;  // header or damaged row
    String rarity = row.substring(c4 + 1, c5);
    uint8_t r = COMMON;
    for (int i = 0; i < 4; i++)
      if (rarity == RARITY[i]) r = i;
    remember(strtoull(row.substring(0, 16).c_str(), nullptr, 16), row.substring(c1 + 1, c2).c_str(), r);
  }
}

static void ensureLoaded() {
  if (knownCount < 0) load();
}

static const char *encName(wifi_auth_mode_t a) {
  switch (a) {
    case WIFI_AUTH_OPEN: return "open";
    case WIFI_AUTH_WEP: return "WEP";
    case WIFI_AUTH_WPA_PSK: return "WPA";
    case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2-E";
    case WIFI_AUTH_WPA3_PSK:
    case WIFI_AUTH_WPA2_WPA3_PSK: return "WPA3";
    default: return "WPA2";
  }
}

// First match wins: eduroam is the starter; hidden or weak is rare; open is common.
static uint8_t rarityOf(const char *ssid, int rssi, wifi_auth_mode_t auth) {
  if (strcmp(ssid, "eduroam") == 0) return STARTER;
  if (!*ssid || rssi < WEAK_RSSI) return RARE;
  if (auth == WIFI_AUTH_OPEN) return COMMON;
  return UNCOMMON;
}

static void drawScanning() {
  drawHeader("Dex");
  display.setFont(FONT_SMALL);
  drawCentered("scanning...", (CONTENT_TOP + CONTENT_BOTTOM) / 2);
}

static void scan() {
  displayShow(drawScanning, false);
  ensureLoaded();

  setCpuFrequencyMhz(240);  // full speed only while the radio is on
  WiFi.mode(WIFI_STA);
  int found = WiFi.scanNetworks(false, true /*include hidden*/, false, 120 /*ms per channel*/);
  nearby = max(found, 0);
  newCount = 0;
  String rows;  // new finds, written to flash in one go below
  for (int i = 0; i < nearby; i++) {
    char ssid[33];
    strlcpy(ssid, WiFi.SSID(i).c_str(), sizeof ssid);
    for (char *p = ssid; *p; p++)  // keep the CSV intact and the ASCII-only font readable
      if (*p == ',') *p = ' ';
      else if (*p < 32 || *p > 126) *p = '?';
    uint64_t h = hashBssid(WiFi.BSSID(i));
    if (isKnown(keyOf(ssid, h))) continue;  // also skips repeats within this scan
    int rssi = WiFi.RSSI(i);
    wifi_auth_mode_t auth = WiFi.encryptionType(i);
    uint8_t r = rarityOf(ssid, rssi, auth);
    char row[96];
    snprintf(row, sizeof row, "%016llx,%s,%d,%s,%s,%ld\n", (unsigned long long)h, ssid, rssi, encName(auth),
             RARITY[r], (long)time(nullptr));
    rows += row;
    remember(h, ssid, r);
    if (newCount++ == 0 || r > best.rarity) best = newest;
  }
  WiFi.scanDelete();
  WiFi.mode(WIFI_OFF);
  setCpuFrequencyMhz(80);

  if (rows.length()) {
    bool fresh = !storageExists(DEX);
    fs::File f = storageOpen(DEX, "a");
    if (fresh) f.print("hash,ssid,rssi,enc,rarity,first_seen\n");
    f.print(rows);
    f.close();
  }
  scanned = true;
#if DEBUG
  Serial.printf("scan: %d nearby, %d new, WiFi mode %d (0 = off)\n", nearby, newCount, (int)WiFi.getMode());
#endif
  powerActivity();  // the scan took a few seconds
}

static void drawTitle(const char *title) {
  drawHeader(title);
  char found[16];
  snprintf(found, sizeof found, "%d found", knownCount);
  drawRight(found, 16);
}

// Large font if it fits, else small.
static void drawName(const char *text, int16_t cy) {
  int16_t x, y;
  uint16_t w, h;
  display.setFont(FONT_LARGE);
  display.getTextBounds(text, 0, 0, &x, &y, &w, &h);
  if (w > display.width() - 2 * MARGIN) display.setFont(FONT_SMALL);
  drawCentered(text, cy);
}

static void drawMain() {
  drawTitle("Dex");
  char line[48];
  display.setFont(FONT_SMALL);
  if (knownCount == 0) {
    drawCentered("no networks yet", 90);
    drawCentered("B to scan", 114);
  } else if (scanned && newCount > 0) {
    // NEW!: a small inverted tag above the best new find.
    const int16_t tagW = 52, tagH = 20, tagX = (display.width() - tagW) / 2, tagY = CONTENT_TOP + 10;
    display.fillRoundRect(tagX, tagY, tagW, tagH, 4, GxEPD_BLACK);
    display.setTextColor(GxEPD_WHITE);
    drawCentered("NEW!", tagY + tagH / 2);
    display.setTextColor(GxEPD_BLACK);
    drawName(shown(best), 90);
    snprintf(line, sizeof line, "%s, %d new of %d", RARITY[best.rarity], newCount, nearby);
    display.setFont(FONT_SMALL);
    drawCentered(line, 128);
  } else if (scanned) {
    drawName("nothing new", 90);
    snprintf(line, sizeof line, "%d nearby", nearby);
    display.setFont(FONT_SMALL);
    drawCentered(line, 128);
  } else {
    char count[12];
    snprintf(count, sizeof count, "%d", knownCount);
    display.setFont(FONT_LARGE);
    drawCentered(count, 78);
    display.setFont(FONT_SMALL);
    drawCentered("networks found", 106);
    snprintf(line, sizeof line, "last: %s", shown(newest));
    drawCentered(line, 140);
  }
  drawFooter("list", "scan");
}

// One page of the whole dex, newest first, read straight from the file (names aren't kept in RAM).
static int readPage(Find *out) {
  const int first = knownCount - 1 - page * PAGE;  // file row index of the page's newest entry
  int row = 0, n = 0;
  fs::File f = storageOpen(DEX);
  while (f && f.available()) {
    String line = f.readStringUntil('\n');
    int c1 = line.indexOf(','), c2 = line.indexOf(',', c1 + 1);
    if (c1 != 16) continue;  // header or damaged row
    if (row > first - PAGE && row <= first) {
      int c4 = line.indexOf(',', line.indexOf(',', c2 + 1) + 1), c5 = line.indexOf(',', c4 + 1);
      Find &f2 = out[first - row];
      strlcpy(f2.ssid, line.substring(c1 + 1, c2).c_str(), sizeof f2.ssid);
      f2.tag = strtoul(line.substring(0, 4).c_str(), nullptr, 16);
      String rarity = line.substring(c4 + 1, c5);
      f2.rarity = COMMON;
      for (int i = 0; i < 4; i++)
        if (rarity == RARITY[i]) f2.rarity = i;
      n++;
    }
    row++;
  }
  return n;
}

static void drawList() {
  Find rows[PAGE];
  const int n = readPage(rows);
  char title[24];
  snprintf(title, sizeof title, "%d-%d of %d", page * PAGE + 1, page * PAGE + n, knownCount);
  drawHeader("All");
  drawRight(title, 16);
  display.setFont(FONT_SMALL);
  for (int i = 0; i < n; i++) {
    const int16_t baseline = CONTENT_TOP + 22 + i * 26;
    drawRight(RARITY[rows[i].rarity], baseline);
    int16_t x, y;
    uint16_t w, h, tagW;
    display.getTextBounds(RARITY[rows[i].rarity], 0, 0, &x, &y, &tagW, &h);
    const int16_t room = display.width() - 2 * MARGIN - tagW - 8;
    String name = shown(rows[i]);
    display.getTextBounds(name.c_str(), 0, 0, &x, &y, &w, &h);
    while (name.length() > 1 && w > room) {  // shorten until it fits beside the rarity
      name.remove(name.length() - 1);
      display.getTextBounds(name.c_str(), 0, 0, &x, &y, &w, &h);
    }
    display.setCursor(MARGIN, baseline);
    display.print(name);
  }
  if (n == 0) drawCentered("nothing yet", (CONTENT_TOP + CONTENT_BOTTOM) / 2);
  drawFooter("more", "back");
}

static void drawStats() {
  drawTitle("Rarity");
  display.setFont(FONT_SMALL);
  const uint8_t order[] = {RARE, STARTER, UNCOMMON, COMMON};
  for (int i = 0; i < 4; i++) {
    const int16_t baseline = CONTENT_TOP + 30 + i * 30;
    display.setCursor(MARGIN, baseline);
    display.print(RARITY[order[i]]);
    char n[8];
    snprintf(n, sizeof n, "%d", perRarity[order[i]]);
    drawRight(n, baseline);
  }
  drawFooter("back", "hold: clear");
}

static void drawConfirm() {
  char line[32];
  snprintf(line, sizeof line, "Clear all %d?", knownCount);
  drawHeader("Clear dex");
  display.setFont(FONT_SMALL);
  drawCentered(line, 88);
  drawCentered("this can't be undone", 112);
  drawFooter("no", "yes");
}

static void clearDex() {
  storageRemove(DEX);
  knownCount = -1;  // reload: empty. The salt stays, so hashes stay comparable.
  scanned = false;
  ensureLoaded();
}

static void onEnter() {
  view = MAIN;
  scanned = false;
  knownCount = -1;
  ensureLoaded();
}

static Redraw onButton(Event e) {
  ensureLoaded();
  if (view == CONFIRM) {
    if (e == Event::BShort) {
      clearDex();
      view = MAIN;
    } else if (e == Event::AShort) {
      view = STATS;
    } else {
      return Redraw::None;
    }
  } else if (view == STATS) {
    view = e == Event::BLong ? CONFIRM : MAIN;
  } else if (view == LIST) {
    if (e == Event::AShort) page = (page + 1) * PAGE < knownCount ? page + 1 : 0;  // wraps to the top
    else view = MAIN;
  } else if (e == Event::AShort) {
    view = LIST;
    page = 0;
  } else if (e == Event::BShort) {
    scan();
    view = MAIN;
  } else if (e == Event::BLong) {
    view = STATS;
  } else {
    return Redraw::None;
  }
  return Redraw::Partial;
}

static void draw() {
  ensureLoaded();
  if (view == LIST) drawList();
  else if (view == STATS) drawStats();
  else if (view == CONFIRM) drawConfirm();
  else drawMain();
}

static void onExit() {}

extern const App dexApp = {"Dex", ICON_DEX, onEnter, onButton, draw, onExit};
