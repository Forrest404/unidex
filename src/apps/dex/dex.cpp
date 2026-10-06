// Dex, a WiFi network collection game: scan on demand, log first sightings to /dex.csv with salted, hashed BSSIDs.
// One entry per network name (many access points share one). Hidden networks are left out.
// A = list of all finds, B = scan, hold B = counts per rarity (hold B there to clear), hold A = back.
#include <WiFi.h>
#include <mbedtls/sha256.h>
#include "../../core/app.h"
#include "../../core/devtools.h"
#include "../../core/display.h"
#include "../../core/launcher.h"
#include "../../core/net.h"
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
RTC_DATA_ATTR static int dexCount = -1;  // finds, kept through sleep for the home screen (-1 = not counted yet)

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

// First 8 bytes of SHA-256.
static uint64_t sha64(const uint8_t *data, size_t len) {
  uint8_t out[32];
  mbedtls_sha256_ret(data, len, out, 0);
  uint64_t h = 0;
  for (int i = 0; i < 8; i++) h = h << 8 | out[i];
  return h;
}

// Salted, because a BSSID is only 48 bits with known vendor prefixes: a plain hash could be reversed.
static uint64_t hashBssid(const uint8_t *bssid) {
  uint8_t in[22];
  memcpy(in, salt, 16);
  memcpy(in + 16, bssid, 6);
  return sha64(in, sizeof in);
}

// Networks are keyed by their name, so extra access points with the same name don't count again.
static uint64_t keyOf(const char *ssid) {
  return sha64((const uint8_t *)ssid, strlen(ssid));
}

static bool isKnown(uint64_t h) {
  for (int i = 0; i < knownCount; i++)
    if (known[i] == h) return true;
  return false;
}

static void remember(const char *ssid, uint8_t rarity) {
  if (knownCount < MAX_KNOWN) known[knownCount++] = keyOf(ssid);
  perRarity[rarity]++;
  strlcpy(newest.ssid, ssid, sizeof newest.ssid);
  newest.rarity = rarity;
}

// One dex.csv row (hash,ssid,rssi,enc,rarity,first_seen). False for the header, a damaged row, or a
// hidden network logged by an older version (those are left out now).
static bool parseRow(const String &row, Find &out) {
  int c1 = row.indexOf(','), c2 = row.indexOf(',', c1 + 1), c3 = row.indexOf(',', c2 + 1);
  int c4 = row.indexOf(',', c3 + 1), c5 = row.indexOf(',', c4 + 1);
  if (c1 != 16 || c5 < 0 || c2 == c1 + 1) return false;
  strlcpy(out.ssid, row.substring(c1 + 1, c2).c_str(), sizeof out.ssid);
  String rarity = row.substring(c4 + 1, c5);
  out.rarity = COMMON;
  for (int i = 0; i < 4; i++)
    if (rarity == RARITY[i]) out.rarity = i;
  return true;
}

static void load() {
  knownCount = 0;
  memset(perRarity, 0, sizeof perRarity);
  loadSalt();
  if (storageExists(DEX)) {  // opening a missing file would log an error onto the USB line
    fs::File f = storageOpen(DEX);
    Find entry;
    while (f && f.available())
      if (parseRow(f.readStringUntil('\n'), entry)) remember(entry.ssid, entry.rarity);
  }
  dexCount = knownCount;
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

// First match wins: eduroam is the starter; a weak signal is rare; open is common.
static uint8_t rarityOf(const char *ssid, int rssi, wifi_auth_mode_t auth) {
  if (strcmp(ssid, "eduroam") == 0) return STARTER;
  if (rssi < WEAK_RSSI) return RARE;
  if (auth == WIFI_AUTH_OPEN) return COMMON;
  return UNCOMMON;
}

static void drawScanning() {
  drawHeader("Dex");
  drawEmpty("Looking for networks", "a few seconds...");
  drawProgress(HINTS_TOP - 24, -1);
  drawHints("", "", "");
}

static void scan() {
  if (netClaimed()) {  // a note is sending in the background: don't pull the WiFi from under it
    launcherToast("A note is sending");
    return;
  }
  displayShow(drawScanning, false);
  ensureLoaded();

  setCpuFrequencyMhz(240);  // full speed only while the radio is on
  WiFi.mode(WIFI_STA);
  int found = WiFi.scanNetworks(false, false /*no hidden networks*/, false, 120 /*ms per channel*/);
  nearby = newCount = 0;
  uint64_t seen[64];  // names nearby (many access points can share one), so "nearby" counts networks
  String rows;  // new finds, written to flash in one go below
  for (int i = 0; i < found; i++) {
    char ssid[33];
    strlcpy(ssid, WiFi.SSID(i).c_str(), sizeof ssid);
    if (!*ssid) continue;  // hidden networks are left out
    for (char *p = ssid; *p; p++)  // keep the CSV intact and the ASCII-only font readable
      if (*p == ',') *p = ' ';
      else if (*p < 32 || *p > 126) *p = '?';
    const uint64_t key = keyOf(ssid);
    bool repeat = false;
    for (int s = 0; s < nearby && s < 64; s++) repeat |= seen[s] == key;
    if (repeat) continue;
    if (nearby < 64) seen[nearby] = key;
    nearby++;
    uint64_t h = hashBssid(WiFi.BSSID(i));
    if (isKnown(key)) continue;
    int rssi = WiFi.RSSI(i);
    wifi_auth_mode_t auth = WiFi.encryptionType(i);
    uint8_t r = rarityOf(ssid, rssi, auth);
    char row[96];
    snprintf(row, sizeof row, "%016llx,%s,%d,%s,%s,%ld\n", (unsigned long long)h, ssid, rssi, encName(auth),
             RARITY[r], (long)time(nullptr));
    rows += row;
    remember(ssid, r);
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
  dexCount = knownCount;
#if DEBUG
  Serial.printf("scan: %d nearby, %d new, WiFi mode %d (0 = off)\n", nearby, newCount, (int)WiFi.getMode());
#endif
  powerActivity();  // the scan took a few seconds
}

// Large font if it fits, else small (cut with "..." if even that is too wide).
static void drawName(const char *text, int16_t cy) {
  display.setFont(FONT_LARGE);
  if (textWidth(text) > display.width() - 2 * MARGIN) display.setFont(FONT_SMALL);
  drawCentered(fitText(text, display.width() - 2 * MARGIN).c_str(), cy);
}

static void drawMain() {
  drawHeader("Dex");
  char line[48];
  display.setFont(FONT_SMALL);
  if (knownCount == 0) {
    drawEmpty("No networks yet", "Press B to look for", "WiFi networks nearby");
  } else if (scanned && newCount > 0) {
    // NEW!: a small inverted tag above the best new find.
    const int16_t tagW = 52, tagH = 20, tagX = (display.width() - tagW) / 2, tagY = CONTENT_TOP + 10;
    display.fillRoundRect(tagX, tagY, tagW, tagH, 4, BLACK);
    display.setTextColor(WHITE);
    drawCentered("NEW!", tagY + tagH / 2);
    display.setTextColor(BLACK);
    drawName(best.ssid, 84);
    snprintf(line, sizeof line, "%s, %d new of %d", RARITY[best.rarity], newCount, nearby);
    line[0] = toupper(line[0]);  // "Rare, 3 new of 12"
    display.setFont(FONT_SMALL);
    drawCentered(fitText(line, display.width() - 2 * MARGIN).c_str(), 116);
  } else if (scanned) {
    drawEmpty("Nothing new here", nearby == 1 ? "1 network nearby," : (String(nearby) + " networks nearby,").c_str(),
              "all found before");
  } else {
    char count[12];
    snprintf(count, sizeof count, "%d", knownCount);
    display.setFont(FONT_LARGE);
    drawCentered(count, 84);
    display.setFont(FONT_SMALL);
    drawCentered(knownCount == 1 ? "network found" : "networks found", 114);
  }
  drawHints("list", "scan", "rarity");
}

// One page of the whole dex, newest first, read straight from the file (names aren't kept in RAM).
static int readPage(Find *out) {
  const int first = knownCount - 1 - page * PAGE;  // file row index of the page's newest entry
  int row = 0, n = 0;
  if (!storageExists(DEX)) return 0;
  fs::File f = storageOpen(DEX);
  Find entry;
  while (f && f.available()) {
    if (!parseRow(f.readStringUntil('\n'), entry)) continue;  // counted exactly as load() does
    if (row > first - PAGE && row <= first) {
      out[first - row] = entry;
      n++;
    }
    row++;
  }
  return n;
}

static void drawList() {
  Find rows[PAGE];
  const int n = readPage(rows);
  drawHeader("All finds");
  display.setFont(FONT_SMALL);
  for (int i = 0; i < n; i++) {
    const int16_t baseline = CONTENT_TOP + 22 + i * 26;
    display.setFont(FONT_TINY);  // the rarity small, so the name gets the room
    drawRight(RARITY[rows[i].rarity], baseline);
    const int16_t tagW = textWidth(RARITY[rows[i].rarity]);
    display.setFont(FONT_SMALL);
    display.setCursor(MARGIN, baseline);
    display.print(fitText(rows[i].ssid, display.width() - 2 * MARGIN - tagW - 6));
  }
  if (n == 0) drawEmpty("Nothing yet", "Scan on the Dex screen", "to find networks");
  drawHints(knownCount > PAGE ? "more" : "", "", "");
}

static void drawStatsRows() {
  drawHeader("Rarity");
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
}

static void drawStats() {
  drawStatsRows();
  drawHints("", "", knownCount ? "clear" : "");
}

static void drawConfirm() {
  char line[32];
  snprintf(line, sizeof line, knownCount == 1 ? "Your 1 find goes." : "All %d finds go.", knownCount);
  drawStatsRows();
  drawSheet("Clear the Dex?", line, "Can't be undone.");
  drawHints("keep", "clear", "");
}

static void clearDex() {
  if (devDryRun()) {
    launcherToast("Dry run: not cleared");
    return;
  }
  launcherToast(storageRemove(DEX) ? "Dex cleared" : "Couldn't clear it");
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
    if (e != Event::BLong || !knownCount) return Redraw::None;
    view = CONFIRM;
  } else if (view == LIST) {
    if (e != Event::AShort || knownCount <= PAGE) return Redraw::None;
    page = (page + 1) * PAGE < knownCount ? page + 1 : 0;  // wraps to the top
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

static Redraw onBack() {
  if (view == MAIN) return Redraw::Exit;
  view = view == CONFIRM ? STATS : MAIN;
  return Redraw::Partial;
}

static void onExit() {}

static void status(char *out, size_t len) {
  static bool looked;  // after a restart, count the finds once so the line is there before the first visit
  if (dexCount < 0 && !looked) {
    looked = true;
    if (storageCardMount()) ensureLoaded();
  }
  if (dexCount < 0) snprintf(out, len, "Collect WiFi networks");
  else snprintf(out, len, dexCount == 1 ? "1 network found" : "%d networks found", dexCount);
}

extern const App dexApp = {"Dex", ICON_DEX, onEnter, onButton, draw, onExit, onBack, status, nullptr, true};
