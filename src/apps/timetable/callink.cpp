#include "callink.h"
#include <Arduino.h>
#include <memory>
#include "ics_logic.h"
#include "../../core/battery.h"
#include "../../core/clock.h"
#include "../../core/credentials.h"
#include "../../core/net.h"
#include "../../core/power.h"
#include "../../core/usbsync.h"

static const size_t MAX_BYTES = 16 * 1024 * 1024;  // a calendar with years of history is a few MB
static const int MAX_REDIRECTS = 4;

// "webcal://host/path" (or https://, http://) to its host and path. False if it isn't a web link.
static bool splitUrl(String url, String &host, String &path) {
  url.trim();
  static const char *SCHEMES[] = {"webcal://", "webcals://", "https://", "http://"};
  bool known = false;
  for (const char *s : SCHEMES)
    if (url.startsWith(s)) {
      url.remove(0, strlen(s));
      known = true;
      break;
    }
  if (!known) return false;
  const int slash = url.indexOf('/');
  host = slash < 0 ? url : url.substring(0, slash);
  path = slash < 0 ? "/" : url.substring(slash);
  return host.length() > 0 && host.indexOf(' ') < 0;
}

static const char *failure(int status) {
  static char reason[40];
  if (status == -1) return "Calendar: no connection";
  if (status == 404 || status == 410) return "Calendar: link not found";
  if (status == 401 || status == 403) return "Calendar: link refused";
  snprintf(reason, sizeof reason, "Calendar: error %d", status);
  return reason;
}

// Downloads the link's calendar and reads it into event lines as it comes (nothing goes on the card). WiFi must be
// up and the clock set.
static const char *fetch(std::vector<std::string> &lines) {
  String host, path;
  if (!splitUrl(credGet("cal_url"), host, path)) return "Calendar: bad link";
  std::unique_ptr<ics::Reader> reader;
  std::string line;
  size_t got = 0;
  int status = 0;
  // Lines are cut where the bytes arrive, so a line is put together until its '\n'.
  auto sink = [&](const uint8_t *p, size_t n) {
    got += n;
    for (size_t i = 0; i < n; i++) {
      if (p[i] == '\n') {
        reader->line(line);
        line.clear();
      } else if (line.size() < ics::MAX_LINE) {
        line += (char)p[i];
      }
    }
    return got <= MAX_BYTES;
  };
  for (int hop = 0; hop <= MAX_REDIRECTS; hop++) {
    reader.reset(new ics::Reader(time(nullptr)));  // a redirect's own body is thrown away with it
    line.clear();
    got = 0;
    String location;
    status = netHttpsGet(host.c_str(), path.c_str(), sink, location);
    if (status < 300 || status >= 400 || !location.length()) break;
    if (location.startsWith("/")) path = location;  // the same server
    else if (!splitUrl(location, host, path)) break;
  }
  if (got > MAX_BYTES) return "Calendar: too big";
  if (status != 200) return failure(status);
  if (line.size()) reader->line(line);  // the last line, if it had no '\n'
  if (!reader->isCalendar()) return "Calendar: not a calendar";
  lines = reader->finish();
  return nullptr;
}

const char *calendarCheck(int &events) {
  events = 0;
  if (!clockValid()) return "Clock not set";
  std::vector<std::string> lines;
  const char *err = fetch(lines);
  events = lines.size();
  return err;
}

const char *calendarSync(char *result, size_t len) {
  snprintf(result, len, "%s", "");
  if (netClaimed()) return "WiFi busy: a note is sending";
  if (const char *low = batteryTooLow()) return low;
  const char *err = netConnect();
  if (!err) err = clockSyncOnline();
  if (err) {
    netOff();
    return err;
  }
  if (!credHas("cal_url")) {
    netOff();
    snprintf(result, len, "Clock set");
    return nullptr;
  }
  std::vector<std::string> lines;
  err = fetch(lines);
  netOff();
  if (err) {
    snprintf(result, len, "Clock set. %s", err);
    return err;
  }
  if (!usbSyncSaveEvents(lines)) {
    snprintf(result, len, "Clock set. Couldn't save the calendar");
    return "Couldn't save";
  }
  snprintf(result, len, lines.size() == 1 ? "Clock set, 1 event" : "Clock set, %u events", (unsigned)lines.size());
  return nullptr;
}
