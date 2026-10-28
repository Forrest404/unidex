#include "luahost.h"
#include <climits>  // first: C++ gets LLONG_MAX from here, which luaconf.h needs for 64-bit integers
#include <lua.hpp>
#include <qrcode.h>
#include <esp_random.h>
#include <WiFi.h>
#include "apps_logic.h"
#include "appstore.h"
#include "../badge/badge_file.h"
#include "../../core/audio.h"
#include "../../core/battery.h"
#include "../../core/climate.h"
#include "../../core/clock.h"
#include "../../core/display.h"
#include "../../core/launcher.h"
#include "../../core/net.h"
#include "../../core/power.h"
#include "../../core/storage.h"
#include "../../core/theme.h"
#if UNIDEX_EMU
#include <stdlib.h>
#else
#include <multi_heap.h>
#endif

static const size_t ARENA = 512 * 1024;
static const uint32_t CALL_MS = 2000;

static lua_State *L;
static String id, error;
static uint8_t perms;
static bool usedWifi;
static uint32_t callStart;

// --- memory: one block of PSRAM, taken once and kept (freeing and taking big blocks splits the PSRAM up, see the
// Notes recording buffer), with its own small heap inside, made fresh for every app ---
#if UNIDEX_EMU
static size_t used;
static void *alloc(void *, void *ptr, size_t osize, size_t nsize) {
  if (!ptr) osize = 0;
  if (nsize == 0) {
    free(ptr);
    used -= osize;
    return nullptr;
  }
  if (used - osize + nsize > ARENA) return nullptr;
  void *p = realloc(ptr, nsize);
  if (p) used = used - osize + nsize;
  return p;
}
static bool arenaReset() {
  used = 0;
  return true;
}
size_t hostMemoryUsed() { return used; }
#else
static void *arenaMem;
static multi_heap_handle_t heap;
static void *alloc(void *, void *ptr, size_t, size_t nsize) {
  if (nsize == 0) {
    if (ptr) multi_heap_free(heap, ptr);
    return nullptr;
  }
  return multi_heap_realloc(heap, ptr, nsize);
}
static bool arenaReset() {
  if (!arenaMem) arenaMem = ps_malloc(ARENA);
  if (!arenaMem) return false;
  heap = multi_heap_register(arenaMem, ARENA);
  return heap != nullptr;
}
size_t hostMemoryUsed() { return heap && L ? ARENA - multi_heap_free_size(heap) : 0; }
#endif

// --- limits ---
static void hook(lua_State *s, lua_Debug *) {
  if (millis() - callStart <= CALL_MS) return;
  luaL_where(s, 0);  // the line running now, the loop (luaL_error would name the line that called its function)
  lua_pushfstring(s, "took too long (over %d s)", (int)(CALL_MS / 1000));
  lua_concat(s, 2);
  lua_error(s);
}

// --- small helpers ---
static int16_t coord(lua_State *s, int i) {
  const lua_Number v = luaL_checknumber(s, i);
  return v < -1000 ? -1000 : v > 1000 ? 1000 : (int16_t)floor(v);
}
static int16_t optCoord(lua_State *s, int i, int16_t def) { return lua_isnoneornil(s, i) ? def : coord(s, i); }

static const GFXfont *fontArg(lua_State *s, int i) {
  static const char *const NAMES[] = {"small", "tiny", "medium", "large", "bold", nullptr};
  const GFXfont *const FONTS[] = {FONT_SMALL, FONT_TINY, FONT_MEDIUM, FONT_LARGE, FONT_BOLD};
  return FONTS[luaL_checkoption(s, i, "small", NAMES)];
}

static uint16_t colourArg(lua_State *s, int i) {
  static const char *const NAMES[] = {"black", "white", nullptr};
  return luaL_checkoption(s, i, "black", NAMES) ? WHITE : BLACK;
}

static String text(lua_State *s, int i, size_t max = 200) {
  size_t n;
  const char *t = luaL_optlstring(s, i, "", &n);
  String out;
  out.concat(t, n < max ? n : max);
  return out;
}

static String appFile(const char *rel) { return appsDir(id) + "/" + rel; }

// Reads a whole file of the app (up to 64 KB); false if it's missing or too big.
static bool readFile(const String &path, std::vector<char> &out) {
  fs::File f = storageOpen(path.c_str());
  if (!f || f.isDirectory() || f.size() > apps::MAX_FILE) return false;
  out.resize(f.size());
  const size_t got = out.empty() ? 0 : f.read((uint8_t *)out.data(), out.size());
  return got == out.size();
}

// --- unidex.* drawing ---
static int l_header(lua_State *s) {
  const String t = text(s, 1, 40), r = text(s, 2, 20);
  drawHeader(t.c_str(), r.length() ? r.c_str() : nullptr);
  return 0;
}

static int l_hints(lua_State *s) {
  const String a = text(s, 1, 20), b = text(s, 2, 20), h = text(s, 3, 30);
  drawHints(a.c_str(), b.c_str(), h.c_str());
  return 0;
}

static int l_text(lua_State *s) {
  const int16_t x = coord(s, 1), y = coord(s, 2);
  const String t = text(s, 3);
  display.setFont(fontArg(s, 4));
  display.setTextColor(colourArg(s, 5));
  display.setCursor(x, y);
  display.print(t);
  display.setTextColor(BLACK);
  return 0;
}

static int l_centered(lua_State *s) {
  const int16_t y = coord(s, 1);
  const String t = text(s, 2);
  display.setFont(fontArg(s, 3));
  display.setTextColor(colourArg(s, 4));
  drawCenteredLine(t.c_str(), y);
  display.setTextColor(BLACK);
  return 0;
}

static int l_wrap(lua_State *s) {
  const int16_t x = coord(s, 1), y = coord(s, 2), w = coord(s, 3);
  const String t = text(s, 4, 2000);
  const int maxLines = constrain((int)luaL_optinteger(s, 5, 6), 1, 12);
  const GFXfont *f = fontArg(s, 6);
  display.setFont(f);
  String lines[12];
  const int need = wrapText(t.c_str(), w, lines, maxLines), shown = need < maxLines ? need : maxLines;
  for (int i = 0; i < shown; i++) {
    display.setCursor(x, y + i * f->yAdvance);
    display.print(lines[i]);
  }
  lua_pushinteger(s, shown);
  return 1;
}

static int l_width(lua_State *s) {
  const String t = text(s, 1);
  display.setFont(fontArg(s, 2));
  lua_pushinteger(s, textWidth(t.c_str()));
  return 1;
}

static int l_rect(lua_State *s) {
  display.drawRect(coord(s, 1), coord(s, 2), coord(s, 3), coord(s, 4), colourArg(s, 5));
  return 0;
}

static int l_fill(lua_State *s) {
  display.fillRect(coord(s, 1), coord(s, 2), coord(s, 3), coord(s, 4), colourArg(s, 5));
  return 0;
}

static int l_line(lua_State *s) {
  display.drawLine(coord(s, 1), coord(s, 2), coord(s, 3), coord(s, 4), colourArg(s, 5));
  return 0;
}

static int l_qr(lua_State *s) {
  const String t = text(s, 1, 120);
  const int16_t x = coord(s, 2), y = coord(s, 3), size = optCoord(s, 4, 80);
  QRCode qr;
  uint8_t modules[qrcode_getBufferSize(6)];
  for (uint8_t v = 1; v <= 6; v++) {
    if (qrcode_initText(&qr, modules, v, ECC_LOW, t.c_str()) != 0) continue;
    const int scale = max(1, size / qr.size);
    for (uint8_t r = 0; r < qr.size; r++)
      for (uint8_t c = 0; c < qr.size; c++)
        if (qrcode_getModule(&qr, c, r)) display.fillRect(x + c * scale, y + r * scale, scale, scale, BLACK);
    return 0;
  }
  return luaL_error(s, "qr: text too long");
}

static int l_icon(lua_State *s) {
  luaL_checktype(s, 1, LUA_TTABLE);
  const int16_t x = coord(s, 2), y = coord(s, 3);
  const int scale = constrain((int)luaL_optinteger(s, 4, 1), 1, 2);
  String rows[ICON_SIZE];
  const char *ptrs[ICON_SIZE];
  for (int i = 0; i < ICON_SIZE; i++) {
    lua_rawgeti(s, 1, i + 1);
    size_t n = 0;
    const char *r = lua_tolstring(s, -1, &n);
    if (!r || n != (size_t)ICON_SIZE) return luaL_error(s, "icon: needs 40 rows of 40 characters");
    rows[i] = r;
    ptrs[i] = rows[i].c_str();
    lua_pop(s, 1);
  }
  drawIcon(ptrs, x, y, BLACK, scale);
  return 0;
}

static int l_bitmap(lua_State *s) {
  const char *file = luaL_checkstring(s, 1);
  const int16_t x = coord(s, 2), y = coord(s, 3);
  if (!apps::installPathOk(file) || !apps::endsWith(file, ".bmp")) return luaL_error(s, "bitmap: bad file name");
  std::vector<char> data;
  Bmp b;
  if (!readFile(appFile(file), data) || data.size() > BADGE_MAX_BYTES ||
      !bmpCheck((const uint8_t *)data.data(), data.size(), b))
    return luaL_error(s, "bitmap: can't read %s (a 1-bit BMP up to 200x200)", file);
  for (int32_t r = 0; r < b.h; r++)
    for (int32_t c = 0; c < b.w; c++)
      if (b.black(b.x0 + c, b.y0 + r)) display.drawPixel(x + c, y + r, BLACK);
  return 0;
}

// --- unidex.data.*: the app's own data/ folder ---
static const char *dataName(lua_State *s, int i) {
  const char *n = luaL_checkstring(s, i);
  if (!apps::nameOk(n)) luaL_error(s, "data: bad file name '%s' (a-z 0-9 . _ -)", n);
  return n;
}

static uint64_t dataBytes() { return storageTreeBytes((appsDir(id) + "/data").c_str()); }

static uint64_t fileBytes(const String &path) {
  if (!storageExists(path.c_str())) return 0;
  fs::File f = storageOpen(path.c_str());
  return f ? f.size() : 0;
}

static int l_read(lua_State *s) {
  std::vector<char> buf;
  const String path = appsDataPath(id, dataName(s, 1));
  if (!storageExists(path.c_str()) || !readFile(path, buf)) {
    lua_pushnil(s);
    return 1;
  }
  lua_pushlstring(s, buf.data(), buf.size());
  return 1;
}

static int l_write(lua_State *s) {
  const char *name = dataName(s, 1);
  size_t n;
  const char *t = luaL_checklstring(s, 2, &n);
  const String path = appsDataPath(id, name), tmp = appsDataPath(id, ".write.tmp");
  if (n > apps::MAX_FILE) return luaL_error(s, "data: a file can hold 64 KB");
  if (dataBytes() - fileBytes(path) + n > apps::MAX_DATA) return luaL_error(s, "data: the app's 512 KB are full");
  fs::File f = storageOpen(tmp.c_str(), "w");
  const bool ok = f && f.write((const uint8_t *)t, n) == n;
  if (f) f.close();
  lua_pushboolean(s, ok && storageReplace(tmp.c_str(), path.c_str(), n));
  return 1;
}

static int l_append(lua_State *s) {
  const char *name = dataName(s, 1);
  size_t n;
  const char *t = luaL_checklstring(s, 2, &n);
  const String path = appsDataPath(id, name);
  if (fileBytes(path) + n > apps::MAX_FILE) return luaL_error(s, "data: a file can hold 64 KB");
  if (dataBytes() + n > apps::MAX_DATA) return luaL_error(s, "data: the app's 512 KB are full");
  fs::File f = storageOpen(path.c_str(), "a");
  lua_pushboolean(s, f && f.write((const uint8_t *)t, n) == n);
  return 1;
}

static int l_list(lua_State *s) {
  lua_newtable(s);
  const String dir = appsDir(id) + "/data";
  if (!storageExists(dir.c_str())) return 1;
  fs::File d = storageOpen(dir.c_str());
  int i = 0;
  for (fs::File f = d ? d.openNextFile() : fs::File(); f && i < 500; f = d.openNextFile()) {
    const char *n = f.name();
    if (f.isDirectory() || !apps::nameOk(n)) continue;  // (skips the write's .tmp)
    lua_pushstring(s, n);
    lua_rawseti(s, -2, ++i);
  }
  return 1;
}

static int l_remove(lua_State *s) {
  lua_pushboolean(s, storageRemove(appsDataPath(id, dataName(s, 1)).c_str()));
  return 1;
}

// --- unidex.* device ---
static int l_time(lua_State *s) {
  const struct tm t = clockLocal();
  lua_createtable(s, 0, 8);
  const struct { const char *k; int v; } F[] = {{"year", t.tm_year + 1900}, {"month", t.tm_mon + 1},
                                                {"day", t.tm_mday},          {"hour", t.tm_hour},
                                                {"min", t.tm_min},           {"sec", t.tm_sec},
                                                {"wday", t.tm_wday + 1}};
  for (auto &f : F) {
    lua_pushinteger(s, f.v);
    lua_setfield(s, -2, f.k);
  }
  lua_pushboolean(s, clockValid());
  lua_setfield(s, -2, "valid");
  return 1;
}

// API 6: the SHTC3's reading, or nil and the reason.
static int l_climate(lua_State *s) {
  ClimateReading r;
  if (!climateRead(r)) {
    lua_pushnil(s);
    lua_pushstring(s, "sensor read failed");
    return 2;
  }
  lua_createtable(s, 0, 2);
  lua_pushnumber(s, r.temperatureC);
  lua_setfield(s, -2, "temperature_c");
  lua_pushnumber(s, r.humidityRH);
  lua_setfield(s, -2, "humidity_rh");
  return 1;
}

static int l_battery(lua_State *s) {
  lua_pushinteger(s, batteryPercent());
  lua_pushboolean(s, batteryCharging());
  return 2;
}

static int l_toast(lua_State *s) {
  launcherToast(text(s, 1, 40).c_str());
  return 0;
}

static int l_wake_in(lua_State *s) {
  const lua_Integer ms = luaL_checkinteger(s, 1);
  powerWakeWithin((uint32_t)constrain(ms, (lua_Integer)1000, (lua_Integer)86400000));
  return 0;
}

static int l_random(lua_State *s) {
  const lua_Integer n = luaL_checkinteger(s, 1);
  luaL_argcheck(s, n >= 1, 1, "must be at least 1");
  lua_pushinteger(s, 1 + (lua_Integer)(esp_random() % (uint32_t)min<lua_Integer>(n, 0x7fffffff)));
  return 1;
}

static int l_print(lua_State *s) {
#if UNIDEX_DEV
  Serial.printf("LUA %s:", id.c_str());
  for (int i = 1; i <= lua_gettop(s); i++) Serial.printf(" %s", luaL_tolstring(s, i, nullptr)), lua_pop(s, 1);
  Serial.println();
#endif
  return 0;
}

// --- unidex.http (permission "wifi") ---
static int l_get(lua_State *s) {
  if (!(perms & apps::PERM_WIFI)) return luaL_error(s, "http: the app needs the \"wifi\" permission");
  size_t n;
  const char *url = luaL_checklstring(s, 1, &n);
  if (n > 300 || strncmp(url, "https://", 8)) return luaL_error(s, "http: an https:// address, up to 300 characters");
  for (size_t i = 0; i < n; i++)
    if ((uint8_t)url[i] <= ' ' || url[i] == 127) return luaL_error(s, "http: no spaces in the address");
  const char *hostStart = url + 8, *slash = strchr(hostStart, '/');
  const String host = slash ? String(hostStart).substring(0, slash - hostStart) : String(hostStart);
  const String path = slash ? String(slash) : String("/");
  for (char c : host)
    if (!isalnum((unsigned char)c) && c != '.' && c != '-') return luaL_error(s, "http: bad host name");
  const char *why = batteryTooLow();
  if (!why && netClaimed()) why = "WiFi busy (a note is sending)";
  if (!why) why = netConnect();
  usedWifi = true;
  if (why) {
    callStart = millis();
    lua_pushnil(s);
    lua_pushstring(s, why);
    return 2;
  }
  String body;
  const int status = netHttps(host.c_str(), "GET", path.c_str(), "User-Agent: unidex-app\r\n", nullptr, 0, body, 32768);
  callStart = millis();  // the wait doesn't count against the app's 2 s
  if (status < 0) {
    lua_pushnil(s);
    lua_pushstring(s, "no connection");
    return 2;
  }
  lua_pushinteger(s, status);
  lua_pushlstring(s, body.c_str(), body.length());
  return 2;
}

// --- unidex.wifi (permission "wifi"): the networks nearby, by name; never their addresses ---
static const char *securityName(wifi_auth_mode_t a) {
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

static int l_scan(lua_State *s) {
  if (!(perms & apps::PERM_WIFI)) return luaL_error(s, "wifi: the app needs the \"wifi\" permission");
  const char *why = batteryTooLow();
  if (!why && netClaimed()) why = "WiFi busy (a note is sending)";
  if (why) {
    lua_pushnil(s);
    lua_pushstring(s, why);
    return 2;
  }
  // 160 MHz while the radio is on: a scan takes the same time as at 240 (about 2.2 s) but 3.3 s at 80. Connected
  // already (an http.get before)? Then it stays connected.
  const bool wasOn = WiFi.getMode() != WIFI_OFF;
  if (!wasOn) {
    setCpuFrequencyMhz(160);
    WiFi.mode(WIFI_STA);
  }
  const int found = WiFi.scanNetworks(false, false /*no hidden networks*/, false, 120 /*ms per channel*/);
  lua_createtable(s, found > 0 ? min(found, 64) : 0, 0);
  int n = 0;
  for (int i = 0; i < found && n < 64; i++) {
    const String ssid = WiFi.SSID(i);
    if (!ssid.length()) continue;
    lua_createtable(s, 0, 3);
    lua_pushlstring(s, ssid.c_str(), ssid.length());
    lua_setfield(s, -2, "ssid");
    lua_pushinteger(s, WiFi.RSSI(i));
    lua_setfield(s, -2, "rssi");
    lua_pushstring(s, securityName(WiFi.encryptionType(i)));
    lua_setfield(s, -2, "security");
    lua_rawseti(s, -2, ++n);
  }
  WiFi.scanDelete();
  if (!wasOn) netOff();
  callStart = millis();  // the scan doesn't count against the app's 2 s
  if (found < 0) {
    lua_pop(s, 1);
    lua_pushnil(s);
    lua_pushstring(s, "scan failed");
    return 2;
  }
  return 1;
}

// --- unidex.chime (API 5): the device's chime, unless Settings > Alert sound is off; at most one a second ---
static int l_chime(lua_State *s) {
  static uint32_t lastAt;
  static bool chimed;
  bool played = false;
  if (!audioAlertsMuted() && (!chimed || millis() - lastAt >= 1000)) {
    const uint32_t t0 = millis();
    played = audioChime();  // (silent at volume 0)
    callStart += millis() - t0;  // its ~0.5 s doesn't count against the app's 2 s
    lastAt = millis();
    chimed = true;
  }
  lua_pushboolean(s, played);
  return 1;
}

// --- unidex.frame (API 3): draw() shown now, mid-call, as an animation frame (a quick partial refresh) ---
static int frameCount;      // frames in the current call (60 at most, so an endless animation still ends)
static String frameError;   // an error in draw() during a frame, raised in the caller once the frame is done

static void frameDraw() {
  display.fillScreen(WHITE);
  lua_getglobal(L, "draw");
  if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
    const char *m = lua_tostring(L, -1);
    frameError = m ? m : "error";
    lua_pop(L, 1);
  }
}

static int l_frame(lua_State *s) {
  const int ms = constrain((int)luaL_optinteger(s, 1, 0), 0, 1000);  // a pause after it, for easing
  if (++frameCount > 60) return luaL_error(s, "frame: at most 60 frames in one go");
  frameError = "";
  const uint32_t t0 = millis();
  displayFrame(frameDraw);
  if (ms) delay(ms);
  powerAlive();
  callStart += millis() - t0;  // the screen and the pause don't count against the app's 2 s
  if (frameError.length()) {  // draw()'s own error, with its own line ("main.lua:3: ...")
    lua_pushstring(s, frameError.c_str());
    return lua_error(s);
  }
  return 0;
}

// --- require: only .lua files inside the app's folder, loaded once ---
static int l_require(lua_State *s) {
  const char *name = luaL_checkstring(s, 1);
  const String rel = String(name) + ".lua";
  if (!apps::installPathOk(rel.c_str())) return luaL_error(s, "require: bad module name '%s'", name);
  lua_getfield(s, LUA_REGISTRYINDEX, "unidex.loaded");
  lua_getfield(s, -1, name);
  if (!lua_isnil(s, -1)) return 1;
  lua_pop(s, 1);
  std::vector<char> code;
  if (!readFile(appFile(rel.c_str()), code)) return luaL_error(s, "require: no %s in the app", rel.c_str());
  const String chunk = "@" + rel;
  if (luaL_loadbufferx(s, code.data(), code.size(), chunk.c_str(), "t") != LUA_OK) return lua_error(s);
  lua_call(s, 0, 1);
  if (lua_isnil(s, -1)) {
    lua_pop(s, 1);
    lua_pushboolean(s, 1);
  }
  lua_pushvalue(s, -1);
  lua_setfield(s, -3, name);
  return 1;
}

// --- setting up the sandbox (run protected, so running out of memory here is an error, not a crash) ---
static int setup(lua_State *s) {
  const luaL_Reg LIBS[] = {{LUA_GNAME, luaopen_base},       {LUA_STRLIBNAME, luaopen_string},
                           {LUA_TABLIBNAME, luaopen_table}, {LUA_MATHLIBNAME, luaopen_math},
                           {LUA_UTF8LIBNAME, luaopen_utf8}, {LUA_COLIBNAME, luaopen_coroutine}};
  for (const luaL_Reg &lib : LIBS) {
    luaL_requiref(s, lib.name, lib.func, 1);
    lua_pop(s, 1);
  }
  for (const char *gone : {"dofile", "loadfile", "load"}) {  // no files, and no compiled (binary) chunks
    lua_pushnil(s);
    lua_setglobal(s, gone);
  }
  lua_pushcfunction(s, l_print);
  lua_setglobal(s, "print");
  lua_newtable(s);
  lua_setfield(s, LUA_REGISTRYINDEX, "unidex.loaded");
  lua_pushcfunction(s, l_require);
  lua_setglobal(s, "require");

  const luaL_Reg API[] = {{"header", l_header}, {"hints", l_hints},     {"text", l_text},       {"centered", l_centered},
                          {"wrap", l_wrap},     {"width", l_width},     {"rect", l_rect},       {"fill", l_fill},
                          {"line", l_line},     {"qr", l_qr},           {"icon", l_icon},       {"bitmap", l_bitmap},
                          {"time", l_time},     {"battery", l_battery}, {"toast", l_toast},     {"wake_in", l_wake_in},
                          {"random", l_random}, {"frame", l_frame},     {"chime", l_chime},     {"climate", l_climate},
                          {nullptr, nullptr}};
  luaL_newlib(s, API);
  lua_pushinteger(s, apps::API_LEVEL);
  lua_setfield(s, -2, "api");
  const luaL_Reg DATA[] = {{"read", l_read},     {"write", l_write},   {"append", l_append},
                           {"list", l_list},     {"remove", l_remove}, {nullptr, nullptr}};
  luaL_newlib(s, DATA);
  lua_setfield(s, -2, "data");
  const luaL_Reg HTTP[] = {{"get", l_get}, {nullptr, nullptr}};
  luaL_newlib(s, HTTP);
  lua_setfield(s, -2, "http");
  const luaL_Reg WIFI[] = {{"scan", l_scan}, {nullptr, nullptr}};
  luaL_newlib(s, WIFI);
  lua_setfield(s, -2, "wifi");
  lua_setglobal(s, "unidex");

  std::vector<char> code;
  if (!readFile(appFile("main.lua"), code)) return luaL_error(s, "can't read main.lua (missing, or over 64 KB)");
  if (luaL_loadbufferx(s, code.data(), code.size(), "@main.lua", "t") != LUA_OK) return lua_error(s);
  lua_call(s, 0, 0);
  return 0;
}

// --- calling into the app ---
static void fail(const char *why) {
  error = why;
  if (L) lua_close(L);
  L = nullptr;
  if (usedWifi) netOff();
  usedWifi = false;
}

// Calls global `fn` (if defined) with no arguments; true with its result on the stack (one value).
// Runs the function under its `nargs` arguments on the stack, leaving one result. An error closes the app.
static bool run(int nargs) {
  callStart = millis();
  frameCount = 0;
  if (lua_pcall(L, nargs, 1, 0) != LUA_OK) {
    const char *m = lua_tostring(L, -1);
    String why = m ? m : "error";
    if (why.indexOf("not enough memory") >= 0) why = "used more than its 512 KB of memory";
    fail(why.c_str());
    return false;
  }
  return true;
}

static bool call(const char *fn, bool &defined) {
  defined = false;
  if (!L) return false;
  lua_getglobal(L, fn);
  if (!lua_isfunction(L, -1)) {
    lua_pop(L, 1);
    return false;
  }
  defined = true;
  return run(0);
}

// A game's own function with arguments (games.cpp): they're pushed first, then this puts fn under them. One result
// is left on the stack. A game without the function fails, saying which.
static bool callGame(const char *fn, int nargs) {
  if (!L) return false;
  lua_getglobal(L, fn);
  if (!lua_isfunction(L, -1)) {
    lua_pop(L, nargs + 1);
    char why[48];
    snprintf(why, sizeof why, "main.lua has no %s() function", fn);
    fail(why);
    return false;
  }
  lua_insert(L, -(nargs + 1));
  return run(nargs);
}

bool hostGameStart(uint32_t seed) {
  if (!L) return false;
  lua_pushinteger(L, seed);
  if (!callGame("start", 1)) return false;
  lua_pop(L, 1);
  return true;
}

int hostGameStep(bool tapped, bool held) {
  if (!L) return -1;
  lua_pushboolean(L, tapped);
  lua_pushboolean(L, held);
  if (!callGame("step", 2)) return -1;
  const char *r = lua_isstring(L, -1) ? lua_tostring(L, -1) : "";
  const int out = !strcmp(r, "over") ? 0 : !strcmp(r, "still") ? 2 : 1;  // nothing (or "moved"): redraw
  lua_pop(L, 1);
  return out;
}

bool hostGameScore(int &score) {
  if (!callGame("score", 0)) return false;
  int isNumber = 0;
  const lua_Integer n = lua_tointegerx(L, -1, &isNumber);
  lua_pop(L, 1);
  if (!isNumber) {
    fail("score() should give a whole number");
    return false;
  }
  score = (int)constrain(n, (lua_Integer)-999999, (lua_Integer)999999);
  return true;
}

static Redraw redrawFrom(lua_State *s) {
  const char *r = lua_isstring(s, -1) ? lua_tostring(s, -1) : "";
  const Redraw out = !strcmp(r, "full") ? Redraw::Full : !strcmp(r, "partial") ? Redraw::Partial
                   : !strcmp(r, "exit")  ? Redraw::Exit : Redraw::None;
  lua_pop(s, 1);
  return out;
}

bool hostStart(const String &appId, uint8_t permissions) {
  hostStop();
  id = appId;
  perms = permissions;
  error = "";
  if (!arenaReset()) {
    error = "not enough memory to start";
    return false;
  }
  L = lua_newstate(alloc, nullptr);
  if (!L) {
    error = "not enough memory to start";
    return false;
  }
  lua_sethook(L, hook, LUA_MASKCOUNT, 1000);
  callStart = millis();
  lua_pushcfunction(L, setup);
  if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
    const char *m = lua_tostring(L, -1);
    fail(m ? m : "error");
    return false;
  }
  bool defined;
  if (call("onEnter", defined)) lua_pop(L, 1);
  return L != nullptr;
}

void hostStop() {
  bool defined;
  if (L && call("onExit", defined)) lua_pop(L, 1);
  if (L) lua_close(L);
  L = nullptr;
  if (usedWifi) netOff();
  usedWifi = false;
}

bool hostRunning() { return L != nullptr; }
const String &hostId() { return id; }
const String &hostError() { return error; }

bool hostHas(const char *fn) {
  if (!L) return false;
  lua_getglobal(L, fn);
  const bool yes = lua_isfunction(L, -1);
  lua_pop(L, 1);
  return yes;
}

void hostDraw(const char *fn) {
  display.fillScreen(WHITE);
  bool defined;
  if (call(fn, defined)) {
    lua_pop(L, 1);
  } else if (L && !defined) {
    char why[48];
    snprintf(why, sizeof why, "main.lua has no %s() function", fn);
    fail(why);
  }
}

Redraw hostEvent(const char *fn) {
  bool defined;
  if (!call(fn, defined)) return L ? Redraw::None : Redraw::Full;  // an error: the error screen replaces it
  const Redraw r = redrawFrom(L);
  return r == Redraw::Exit ? Redraw::None : r;
}

Redraw hostBack() {
  bool defined;
  if (!call("onBack", defined)) return L || !defined ? Redraw::Exit : Redraw::Full;
  const Redraw r = redrawFrom(L);
  return r == Redraw::None ? Redraw::Exit : r;
}

String hostStatus() {
  bool defined;
  if (!call("status", defined)) return "";
  const String s = lua_isstring(L, -1) ? lua_tostring(L, -1) : "";
  lua_pop(L, 1);
  return s.substring(0, 40);
}
