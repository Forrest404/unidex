// Checks what the device asks of a store entry before it downloads an app (src/apps/scripts/appfetch_logic.h):
//   c++ -std=c++17 -O2 -I src/apps/scripts tools/apptest/appfetch_test.cpp -o /tmp/appfetch_test && /tmp/appfetch_test
#include <cstdio>
#include "appfetch_logic.h"

using namespace appfetch;
static int failures;

static void expect(bool ok, const char *what) {
  printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
  failures += !ok;
}

// Big Clock as site/store/index.json lists it.
static Entry bigClock() {
  Entry e;
  e.id = "clock";
  e.version = "1.0.0";
  e.api = 1;
  e.files = {{"main.lua", "cf2121648c75b082e78ffe38a190bfbf86cd61a1e7d0d3c14bc65779f5a78e60", 1258},
             {"app.json", "7fb9056aa83e6c22e67651ff86a390ab45040d8232be24f0d56c6a8e63a36741", 274}};
  return e;
}

static bool refused(void (*change)(Entry &)) {
  Entry e = bigClock();
  change(e);
  return check(e) != nullptr;
}

int main() {
  expect(!check(bigClock()), "a store entry passes");
  expect(filePath(bigClock(), bigClock().files[0]) == "/store/apps/clock/1.0.0/main.lua", "where a file is on the store");
  Entry old = bigClock();
  old.api = 0;
  expect(!check(old), "a catalogue from before entries had an api level still installs (app.json is checked later)");

  Entry newer = bigClock();
  newer.api = apps::API_LEVEL + 1;
  expect(check(newer) && std::string(check(newer)) == "Needs newer firmware", "an app for newer firmware is refused");
  expect(refused([](Entry &e) { e.id = "../x"; }) && refused([](Entry &e) { e.id = "Clock"; }), "a bad id");
  expect(refused([](Entry &e) { e.version = "1.0"; }) && refused([](Entry &e) { e.version = "1.0.0/../x"; }),
         "a bad version (it's part of the address)");
  expect(refused([](Entry &e) { e.files[0].path = "../main.lua"; }) &&
             refused([](Entry &e) { e.files.push_back({"data/x.txt", std::string(64, 'a'), 1}); }) &&
             refused([](Entry &e) { e.files.push_back({"run.sh", std::string(64, 'a'), 1}); }),
         "a path an install may not write");
  expect(refused([](Entry &e) { e.files[0].sha256 = "CF21"; }) &&
             refused([](Entry &e) { e.files[0].sha256[0] = 'C'; }),
         "a digest that isn't 64 lower-case hex digits");
  expect(refused([](Entry &e) { e.files[0].size = apps::MAX_FILE + 1; }), "a file over 64 KB");
  expect(refused([](Entry &e) {
           for (int i = 0; i < 9; i++)
             e.files.push_back({"f" + std::to_string(i) + ".txt", std::string(64, 'a'), apps::MAX_FILE});
         }),
         "an app over 512 KB");
  expect(refused([](Entry &e) { e.files.erase(e.files.begin()); }) &&
             refused([](Entry &e) { e.files.pop_back(); }) && refused([](Entry &e) { e.files.clear(); }),
         "no main.lua or app.json");
  expect(refused([](Entry &e) {
           for (int i = 0; i < 31; i++) e.files.push_back({"f" + std::to_string(i) + ".txt", std::string(64, 'a'), 1});
         }),
         "more than 32 files");

  const uint8_t bytes[] = {0x00, 0x0f, 0xa5, 0xff};
  expect(hex(bytes, 4) == "000fa5ff", "a digest as hex");

  printf(failures ? "\n%d FAILED\n" : "\nall passed\n", failures);
  return failures ? 1 : 0;
}
