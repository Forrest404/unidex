// Checks the third-party app rules (src/apps/scripts/apps_logic.h) on a computer (no board needed):
//   c++ -std=c++17 -O2 -I src/apps/scripts tools/apptest/apps_test.cpp -o /tmp/apps_test && /tmp/apps_test
#include <cstdio>
#include "apps_logic.h"

using namespace apps;
static int failures;

static void check(bool ok, const char *what) {
  printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
  failures += !ok;
}

static Manifest good() {
  Manifest m;
  m.id = "hello";
  m.name = "Hello";
  m.version = "1.0.0";
  m.author = "someone";
  m.api = 1;
  return m;
}

int main() {
  check(idOk("hello") && idOk("a") && idOk("my-app-2") && idOk("abcdefghijklmnopqrstuvwx"), "good ids");
  check(!idOk("") && !idOk("-x") && !idOk("Hello") && !idOk("a_b") && !idOk("a.b") && !idOk("a/b") &&
            !idOk("abcdefghijklmnopqrstuvwxy") && !idOk("..") && !idOk("a b"),
        "bad ids");

  check(nameOk("count") && nameOk("save.txt") && nameOk("a_b-c.json"), "good file names");
  check(!nameOk("") && !nameOk(".hidden") && !nameOk("a..b") && !nameOk("A.txt") && !nameOk("a/b") &&
            !nameOk("a b") && !nameOk("abcdefghijklmnopqrstuvwxyz0123456"),
        "bad file names");

  check(installPathOk("main.lua") && installPathOk("app.json") && installPathOk("lib/util.lua") &&
            installPathOk("img/big/a.bmp") && installPathOk("notes.txt"),
        "good install paths");
  check(!installPathOk("") && !installPathOk("/main.lua") && !installPathOk("../x.lua") &&
            !installPathOk("a/../b.lua") && !installPathOk("a//b.lua") && !installPathOk("lib/") &&
            !installPathOk("data/count") && !installPathOk("data/x.txt") && !installPathOk("a/b/c/d.lua") &&
            !installPathOk("main.exe") && !installPathOk("main") && !installPathOk("Main.lua") &&
            !installPathOk(".x.lua"),
        "bad install paths");
  check(installPathOk("database.lua") && installPathOk("datax/a.lua"), "only the data/ folder is off limits");

  check(versionValue("1.0.0") == 100000000 && versionValue("0.0.1") == 1 && versionValue("12.3.45") > 0,
        "versions parse");
  check(versionValue("1.0") < 0 && versionValue("1.0.0.0") < 0 && versionValue("a.b.c") < 0 &&
            versionValue("1.0.x") < 0 && versionValue("") < 0 && versionValue("10000.0.0") < 0 &&
            versionValue("1..0") < 0,
        "bad versions are refused");
  check(versionCompare("1.2.0", "1.10.0") < 0 && versionCompare("2.0.0", "1.99.99") > 0 &&
            versionCompare("1.0.0", "1.0.0") == 0 && versionCompare("junk", "0.0.0") < 0,
        "versions compare by number, not by text");

  Manifest m = good();
  check(check(m, "hello") == nullptr, "a good manifest passes");
  m = good();
  check(check(m, "other") != nullptr, "the id must match its folder");
  m = good();
  m.id = "Bad";
  check(check(m, "Bad") != nullptr, "a bad id fails");
  m = good();
  m.name = "";
  check(check(m, "hello") != nullptr, "a name is needed");
  m = good();
  m.name = "This name is far too long";
  check(check(m, "hello") != nullptr, "a long name fails");
  m = good();
  m.version = "1";
  check(check(m, "hello") != nullptr, "a bad version fails");
  m = good();
  m.api = 0;
  check(check(m, "hello") != nullptr, "an api level is needed");
  m = good();
  m.api = 2;
  check(check(m, "hello") == nullptr, "an app for api 2 (unidex.wifi) is fine");
  m.api = 3;
  check(check(m, "hello") == nullptr, "an app for api 3 (unidex.frame) is fine");
  m = good();
  m.api = API_LEVEL + 1;
  check(strcmp(check(m, "hello"), "needs newer firmware") == 0, "a newer api level is refused");
  m = good();
  m.type = "plugin";
  check(check(m, "hello") != nullptr, "a plugin must say what it extends");
  m.extends = "badge";
  check(check(m, "hello") == nullptr, "a badge plugin passes");
  m.extends = "settings";
  check(check(m, "hello") != nullptr, "an app that can't be extended fails");
  m = good();
  m.extends = "badge";
  check(check(m, "hello") != nullptr, "only plugins extend an app");
  m = good();
  m.type = "game";
  check(check(m, "hello") != nullptr, "unknown types fail");
  m = good();
  m.permissions = {"wifi"};
  check(check(m, "hello") == nullptr && m.perms == PERM_WIFI, "the wifi permission is known");
  m = good();
  m.permissions = {"wifi", "camera"};
  check(check(m, "hello") != nullptr, "an unknown permission fails");
  m = good();
  m.icon = std::vector<std::string>(40, std::string(40, '.'));
  check(check(m, "hello") == nullptr, "a 40x40 icon passes");
  m = good();
  m.icon = std::vector<std::string>(39, std::string(40, '.'));
  check(check(m, "hello") != nullptr, "an icon with the wrong rows fails");
  m = good();
  m.icon = std::vector<std::string>(40, std::string(41, '.'));
  check(check(m, "hello") != nullptr, "an icon with the wrong width fails");

  printf(failures ? "%d FAILED\n" : "all passed\n", failures);
  return failures != 0;
}
