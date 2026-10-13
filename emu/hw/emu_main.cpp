// The browser build's runtime: time, Serial as byte queues, the buttons, the in-memory NVS, the page's screen, and
// the few exports the page calls. The firmware's own setup() and loop() (src/main.cpp) run unchanged.
#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <SD_MMC.h>
#include <WiFi.h>
#include <deque>
#include <dirent.h>
#include <emscripten.h>
#include <map>
#include <nvs_flash.h>
#include <vector>
#include "../../src/core/devtools.h"

void setup();
void loop();

// ---- time
uint32_t millis() { return (uint32_t)emscripten_get_now(); }
uint32_t micros() { return (uint32_t)(emscripten_get_now() * 1000.0); }
void delay(uint32_t ms) { emscripten_sleep(ms ? ms : 1); }  // ASYNCIFY: hands the thread back to the page

// ---- Serial: bytes in from the page, bytes out to it
static std::deque<uint8_t> serialIn;
static std::string serialOut;
SerialPort Serial;
int SerialPort::available() { return (int)serialIn.size(); }
int SerialPort::read() {
  if (serialIn.empty()) return -1;
  int c = serialIn.front();
  serialIn.pop_front();
  return c;
}
size_t SerialPort::write(uint8_t c) { serialOut += (char)c; return 1; }
size_t SerialPort::write(const uint8_t *b, size_t n) { serialOut.append((const char *)b, n); return n; }
EspClass ESP;
void EspClass::restart() { EM_ASM({ location.reload(); }); }

// ---- the file system lives in /sd (storage.cpp mounts it as the card)
SDMMCFS SD_MMC;
LittleFSFS LittleFS;
WiFiClass WiFi;
void *emuOpenDir(const char *path) { return opendir(path); }
bool emuReadDir(void *dir, char *name, size_t len) {
  while (dirent *e = readdir((DIR *)dir)) {
    if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
    strlcpy(name, e->d_name, len);
    return true;
  }
  return false;
}
void emuCloseDir(void *dir) { closedir((DIR *)dir); }

// ---- NVS
static std::map<std::string, std::vector<uint8_t>> nvs;
bool Preferences::clear() { nvs.clear(); return true; }
bool Preferences::remove(const char *k) { return nvs.erase(k) > 0; }
bool Preferences::isKey(const char *k) { return nvs.count(k) > 0; }
int32_t Preferences::getInt(const char *k, int32_t d) {
  auto it = nvs.find(k);
  if (it == nvs.end() || it->second.size() != 4) return d;
  int32_t v;
  memcpy(&v, it->second.data(), 4);
  return v;
}
size_t Preferences::putInt(const char *k, int32_t v) {
  nvs[k].assign((uint8_t *)&v, (uint8_t *)&v + 4);
  return 4;
}
String Preferences::getString(const char *k, const String &d) {
  auto it = nvs.find(k);
  return it == nvs.end() ? d : String(std::string(it->second.begin(), it->second.end()));
}
size_t Preferences::putString(const char *k, const char *v) {
  nvs[k].assign((const uint8_t *)v, (const uint8_t *)v + strlen(v));
  return strlen(v);
}
size_t Preferences::getBytes(const char *k, void *out, size_t len) {
  auto it = nvs.find(k);
  if (it == nvs.end()) return 0;
  const size_t n = std::min(len, it->second.size());
  memcpy(out, it->second.data(), n);
  return n;
}
size_t Preferences::putBytes(const char *k, const void *v, size_t len) {
  nvs[k].assign((const uint8_t *)v, (const uint8_t *)v + len);
  return len;
}
size_t Preferences::getBytesLength(const char *k) {
  auto it = nvs.find(k);
  return it == nvs.end() ? 0 : it->second.size();
}
esp_err_t nvs_flash_erase() { nvs.clear(); return 0; }
esp_err_t nvs_flash_init() { return 0; }

// ---- the buttons: the page sets them; input.cpp reads them through digitalRead like the real pins
static bool pinDown[2];  // A (GPIO0), B (GPIO18)
int digitalRead(int pin) { return (pin == 0 ? pinDown[0] : pin == 18 ? pinDown[1] : false) ? 0 : 1; }

// ---- the screen: the page draws what display.cpp hands over
extern "C" void emuPresent(const uint8_t *buf, char kind) {
  EM_ASM({ if (Module.onScreen) Module.onScreen(HEAPU8.slice($0, $0 + 5000), String.fromCharCode($1)); }, buf, kind);
}

extern "C" {
EMSCRIPTEN_KEEPALIVE void emu_button(int which, int down) { pinDown[which ? 1 : 0] = down; }
EMSCRIPTEN_KEEPALIVE void emu_serial_in(const char *s) {
  for (; *s; s++) serialIn.push_back((uint8_t)*s);
}
// Everything the firmware printed since the last call (NUL-terminated, valid until the next call).
EMSCRIPTEN_KEEPALIVE const char *emu_serial_out() {
  static std::string out;
  out.swap(serialOut);
  serialOut.clear();
  return out.c_str();
}
}

static void frame() { loop(); }

int main() {
  mkdir("/sd", 0777);
  setup();
#if UNIDEX_DEV
  devUsb("X DEMO 1");  // sample classes and notes, nothing personal
#endif
  emscripten_set_main_loop(frame, 0, 1);
  return 0;
}

// X CLOCK UNSET (the test commands) sets the system time; the browser's clock can't be changed.
extern "C" int settimeofday(const struct timeval *, const struct timezone *) { return 0; }
