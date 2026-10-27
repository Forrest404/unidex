// Checks the device's calendar reader (src/apps/timetable/ics_logic.h) on a computer: each calendar in fixtures/,
// read at a fixed time, must give exactly the lines the Tools page gives (fixtures/<name>@<time without colons>.txt, made with
// reference.mjs).
//   c++ -std=c++11 -O2 -Wall -I src/apps/timetable tools/caltest/ics_test.cpp -o /tmp/ics_test && /tmp/ics_test
//   /tmp/ics_test <file.ics> <time, e.g. 2026-10-23T09:00:00Z>   (shows the lines for one file)
#include <cstdio>
#include <fstream>
#include "ics_logic.h"

static ics::Secs parseIso(const std::string &s) {  // "2026-10-23T09:00:00Z"
  return ics::daysFromCivil(atoi(s.substr(0, 4).c_str()), atoi(s.substr(5, 2).c_str()), atoi(s.substr(8, 2).c_str())) *
           86400 +
         atoi(s.substr(11, 2).c_str()) * 3600 + atoi(s.substr(14, 2).c_str()) * 60 + atoi(s.substr(17, 2).c_str());
}

static std::vector<std::string> read(const std::string &path, const std::string &now) {
  ics::Reader r(parseIso(now));
  std::ifstream f(path);
  for (std::string l; std::getline(f, l);) r.line(l);
  return r.finish();
}

int main(int argc, char **argv) {
  if (argc > 2) {
    for (const std::string &l : read(argv[1], argv[2])) printf("%s\n", l.c_str());
    return 0;
  }
  const char *DIR = "tools/caltest/fixtures/";
  const char *CASES[][2] = {
    {"basic", "2026-10-23T09:00:00Z"},  // the clocks go back on the 25th
    {"basic", "2026-10-25T00:30:00Z"},  // in the hour that happens twice
    {"recur", "2026-10-23T09:00:00Z"},
    {"recur", "2027-03-26T12:00:00Z"},  // the clocks go forward on the 28th
    {"zones", "2026-10-23T09:00:00Z"},
    {"zones", "2027-03-26T12:00:00Z"},  // New York already on summer time, London not yet
  };
  int failures = 0;
  for (const auto &c : CASES) {
    const std::vector<std::string> got = read(std::string(DIR) + c[0] + ".ics", c[1]);
    std::vector<std::string> want;
    std::string stamp = c[1];  // no colons in the file name
    stamp.erase(std::remove(stamp.begin(), stamp.end(), ':'), stamp.end());
    std::ifstream f(std::string(DIR) + c[0] + "@" + stamp + ".txt");
    if (!f) {
      printf("FAIL  %s at %s: no expected lines (make them with reference.mjs)\n", c[0], c[1]);
      failures++;
      continue;
    }
    for (std::string l; std::getline(f, l);) want.push_back(l);
    const bool ok = got == want;
    printf("%s  %s at %s: %zu lines\n", ok ? "ok  " : "FAIL", c[0], c[1], got.size());
    for (size_t i = 0; !ok && i < std::max(got.size(), want.size()); i++) {
      const std::string g = i < got.size() ? got[i] : "(none)", w = i < want.size() ? want[i] : "(none)";
      if (g != w) printf("      got  %s\n      want %s\n", g.c_str(), w.c_str());
    }
    failures += !ok;
  }
  return failures ? 1 : 0;
}
