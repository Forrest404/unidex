#include "phone.h"
#include <DNSServer.h>
#include <StreamString.h>
#include <WiFi.h>
#include <algorithm>
#include <esp_mac.h>
#include <esp_wifi.h>
#include <qrcode.h>
#include <unidex_version.h>
#include "job.h"
#include "phone_page.h"
#include "phone_setup.h"
#include "phone_text.h"
#include "store.h"
#include "../../core/audio.h"
#include "../../core/battery.h"
#include "../../core/clock.h"
#include "../../core/credentials.h"
#include "../../core/storage.h"
#include "../../core/devtools.h"
#include "../../core/display.h"
#include "../../core/net.h"
#include "../../core/power.h"
#include "../../core/theme.h"

#if UNIDEX_DEV
static const uint32_t IDLE_MS = 60000;  // test build: a minute, so the auto-stop can be tested
#else
static const uint32_t IDLE_MS = 5 * 60000;
#endif

static WiFiServer http(80, 8);
static DNSServer dns;  // every name the phone looks up points here, so its internet check finds this page
static bool on;
static uint32_t lastUse, served, startedAt, firstAt;  // firstAt: the first request from a phone
static uint32_t shown;     // requests answered when the screen was last drawn ("Phone connected" once there are some)
static uint32_t stopAt;    // Done was pressed: stop at this time (after its answer has gone out)
static bool changed;       // setup was saved since the start
static bool done;          // Done was pressed (it stops, or has stopped, because of that)
static bool redrawFull;    // the screen needs a full refresh (invert changed)
static PhoneTab tab;
static String nearby = "[]";  // the WiFi names around, strongest first, as a JSON array
static char ssid[16], password[12], joinCode[64];

#if UNIDEX_DEV
// Test build: the hotspot, so a computer can join it, and when a phone got through each step of joining, in ms
// after the start (X STATE detail).
static uint32_t joinedAt, addressAt;
static char reqLog[96];  // the last requests and how long each took
static char times[200];
static const char *phoneTimes() {
  snprintf(times, sizeof times, "ssid=%s pw=%s joined=%lu address=%lu first=%lu%s", ssid, password,
           joinedAt ? joinedAt - startedAt : 0, addressAt ? addressAt - startedAt : 0,
           firstAt ? firstAt - startedAt : 0, reqLog);
  return times;
}
#endif

// --- a small web server ---
// Several connections at once: phones open a spare connection ahead of time and leave it empty, and a server
// that waits on one connection at a time (the core's WebServer) would sit on that one for seconds while the
// real request queues behind it. Each request is answered and closed straight away.

struct Request {
  String method, path, query, host, range;  // range: the Range header's value ("bytes=0-1"), if any
  String origin, body;                      // origin: the Origin header (the page that sent it), if any
  String arg(const char *name) const {  // a query value, %XX-decoded
    const String all = "&" + query, key = String("&") + name + "=";
    const int at = all.indexOf(key);
    if (at < 0) return "";
    String raw = all.substring(at + key.length()), out;
    if (raw.indexOf('&') >= 0) raw = raw.substring(0, raw.indexOf('&'));
    for (size_t i = 0; i < raw.length(); i++) {
      if (raw[i] == '%' && i + 2 < raw.length()) out += (char)strtol(raw.substring(i + 1, i + 3).c_str(), nullptr, 16), i += 2;
      else out += raw[i] == '+' ? ' ' : raw[i];
    }
    return out;
  }
};

struct Connection {
  WiFiClient client;
  String head;  // the request so far, up to the blank line after the headers
  uint32_t since;
  fs::File file;     // a recording being sent, a piece per loop turn (other requests carry on meanwhile)
  size_t remaining;  // bytes of it still to send
};
static const int CONNECTIONS = 6;
static const uint32_t REQUEST_WAIT_MS = 4000;  // an empty connection is dropped after this
static const size_t HEAD_MAX = 4096, BODY_MAX = 8192;  // a form with an eduroam certificate is ~4 KB
static Connection connections[CONNECTIONS];

// Writes all of it, or gives up on a phone that stopped taking it (it left the hotspot, say for its usual WiFi): the
// WiFi library waits ~10 s on a write that can't go, so a page sent in several writes would otherwise hold the loop
// up past the watchdog. After the first that fails, the rest return at once.
static bool send(WiFiClient &c, const void *data, size_t len) {
  if (!len) return true;
  if (c.connected() && c.write((const uint8_t *)data, len) == len) return true;
  c.stop();
  return false;
}

static String header(int code, const char *type, size_t len, const String &headers = "") {
  const char *status = code == 200   ? "OK"
                       : code == 204 ? "No Content"
                       : code == 206 ? "Partial Content"
                       : code == 302 ? "Found"
                       : code == 400 ? "Bad Request"
                       : code == 416 ? "Range Not Satisfiable"
                                     : "Not Found";
  return "HTTP/1.1 " + String(code) + " " + status + "\r\nContent-Type: " + type + "\r\nContent-Length: " + String(len) +
         "\r\nCache-Control: no-store\r\nConnection: close\r\n" + headers + "\r\n";
}

static void reply(WiFiClient &c, int code, const char *type, const char *body, size_t len,
                  const String &headers = "") {
  const String head = header(code, type, len, headers);
  if (send(c, head.c_str(), head.length())) send(c, body, len);
}

static void reply(WiFiClient &c, int code, const char *type, const String &body, const String &headers = "") {
  reply(c, code, type, body.c_str(), body.length(), headers);
}

// --- pages ---

using namespace phonetext;

static String str(const std::string &s) { return String(s.c_str()); }

static String listJson;  // every note, built once when the hotspot starts (notes can't change while it's on)

#if UNIDEX_DEV
static void logRequest(const char *what, uint32_t t0) {
  char entry[40];
  snprintf(entry, sizeof entry, " %s:%lums", what, (unsigned long)(millis() - t0));
  const size_t keep = sizeof reqLog - strlen(entry) - 1;
  if (strlen(reqLog) > keep) memmove(reqLog, reqLog + strlen(reqLog) - keep, keep + 1);
  strcat(reqLog, entry);
}
#else
static void logRequest(const char *, uint32_t) {}
#endif

static void buildList() {
  listJson = "[";
  if (storeReady())
    for (const NoteInfo &n : storeList()) {
      std::string title = "Not transcribed yet", line, search, topics;
      if (n.text) {
        const Note note = parse(storeReadNote(n.id).c_str());
        if (!note.title.empty()) title = note.title;
        line = firstLine(note.summary.empty() ? note.body : note.summary, 160);
        search = firstLine(note.body, 400);
        topics = note.topics;
      }
      if (listJson.length() > 1) listJson += ",";
      listJson += str("{\"id\":\"" + json(n.id.c_str()) + "\",\"t\":\"" + json(title) + "\",\"d\":\"" +
                      json(whenOf(n.id.c_str())) + "\",\"s\":\"" + json(line) + "\",\"x\":\"" + json(search) +
                      "\",\"k\":\"" + json(topics) + "\"" + (n.text ? "" : ",\"p\":1") + "}");
    }
  listJson += "]";
}

// One note's parts for the note view; c is what Copy and Share send.
static String noteJson(const String &id) {
  const std::string md = storeReadNote(id).c_str(), when = whenOf(id.c_str());
  std::string out = "{\"id\":\"" + json(id.c_str()) + "\",\"d\":\"" + json(when) + "\"";
  if (md.empty()) return str(out + ",\"t\":\"Not transcribed yet\",\"p\":1,\"c\":\"" + json("Voice note, " + when) + "\"}");
  const Note n = parse(md);
  return str(out + ",\"t\":\"" + json(n.title.empty() ? "Note" : n.title) + "\",\"s\":\"" + json(n.summary) +
             "\",\"b\":\"" + json(n.body) + "\",\"q\":\"" + json(n.transcript) + "\",\"k\":\"" + json(n.topics) +
             "\",\"c\":\"" + json(plainText(n, when)) + "\"}");
}

// "Save as file": the note as a .txt download named after its title.
static void sendText(WiFiClient &c, const String &id) {
  const std::string md = storeValidId(id) ? storeReadNote(id).c_str() : "";
  if (md.empty()) return reply(c, 404, "text/plain", String("Not found"));
  const Note n = parse(md);
  String name;
  for (char ch : n.title) name += isalnum((unsigned char)ch) || ch == ' ' || ch == '-' ? ch : ' ';
  name.trim();
  reply(c, 200, "text/plain; charset=utf-8", str(plainText(n, whenOf(id.c_str()))),
        "Content-Disposition: attachment; filename=\"" + (name.length() ? name : id) + ".txt\"\r\n");
}

// The page with the note list already inside it (in place of "/*NOTES*/null"), so the list draws with the page,
// and the tab to open on (in place of "/*TAB*/'notes'").
static void sendPage(WiFiClient &c) {
  static const char NOTES_MARK[] = "/*NOTES*/null", TAB_MARK[] = "/*TAB*/'notes'";
  const char *notesAt = strstr(PHONE_PAGE, NOTES_MARK), *tabAt = strstr(PHONE_PAGE, TAB_MARK);
  const char *afterNotes = notesAt + sizeof NOTES_MARK - 1, *afterTab = tabAt + sizeof TAB_MARK - 1;
  const char *tabName = tab == PhoneTab::Setup ? "'setup'" : tab == PhoneTab::Settings ? "'settings'" : "'notes'";
  const char *end = PHONE_PAGE + sizeof PHONE_PAGE - 1;
  const String head = header(200, "text/html; charset=utf-8", (notesAt - PHONE_PAGE) + listJson.length() +
                                                                 (tabAt - afterNotes) + strlen(tabName) + (end - afterTab));
  send(c, head.c_str(), head.length()) && send(c, PHONE_PAGE, notesAt - PHONE_PAGE) &&
      send(c, listJson.c_str(), listJson.length()) && send(c, afterNotes, tabAt - afterNotes) &&
      send(c, tabName, strlen(tabName)) && send(c, afterTab, end - afterTab);
}

// --- setup and settings ---

using namespace phonesetup;

static String answer(const char *error) {  // what a save sends back: {"ok":1} or {"error":"..."}
  return error ? "{\"error\":\"" + str(json(error)) + "\"}" : String("{\"ok\":1}");
}

// Everything the Settings and Setup tabs show. Keys only as their last 4 characters, WiFi passwords not at all
// (credStatus's rules).
static String setupJson() {
  StreamString lines;
  credStatus(lines);
  return "{\"cred\":" + str(statusJson(lines.c_str())) + ",\"sleep\":" + String(powerSleepSeconds()) +
         ",\"volume\":" + String(audioVolume()) + ",\"volumeMax\":" + String(AUDIO_VOLUME_MAX) +
         ",\"invert\":" + String(display.inverted ? 1 : 0) + ",\"mute\":" + String(audioAlertsMuted() ? 1 : 0) +
         ",\"time\":" + String(clockValid() ? (long long)time(nullptr) : 0LL) +
         ",\"battery\":" + String(batteryPercent()) + ",\"charging\":" + String(batteryCharging() ? 1 : 0) +
         ",\"name\":\"" + String(ssid) + "\",\"version\":\"" + str(json(UNIDEX_VERSION)) +
         "\",\"nearby\":" + nearby + ",\"busy\":" + String(jobBusy() ? 1 : 0) + "}";
}

// WiFi, keys, GitHub: name=value pairs with the names of the Setup page's N SET. Every value is checked before
// any is saved, so a form is saved whole or not at all (unless the device's memory fills up mid-way).
static const char *saveCredentials(const String &body) {
  const auto pairs = formPairs(body.c_str());
  if (pairs.empty()) return "Nothing to save.";
  if (jobBusy()) return "A note is sending. Try again in a moment.";  // it still needs its keys
  for (const auto &p : pairs) {
    if (!credKnown(p.first.c_str())) return "The device doesn't know that setting. Update it first.";
    if (const char *why = checkValue(p.first, p.second)) return why;
  }
  for (const auto &p : pairs)
    if (!credSet(p.first.c_str(), String(p.second.c_str())))
      return "Couldn't save that: it's too long or has characters the device can't keep.";
  changed = true;
  return nullptr;
}

// Sleep after, volume, the reminder chime (mute) and invert, each only if sent.
static const char *saveSettings(const String &body) {
  int sleepS = -1, volume = -1, invert = -1, mute = -1;
  for (const auto &p : formPairs(body.c_str())) {
    const int v = p.second.empty() ? -1 : atoi(p.second.c_str());
    if (p.first == "sleep") sleepS = v == 10 || v == 20 || v == 30 || v == 60 ? v : -2;
    else if (p.first == "volume") volume = v >= 0 && v <= AUDIO_VOLUME_MAX ? v : -2;
    else if (p.first == "invert") invert = p.second == "0" || p.second == "1" ? v : -2;
    else if (p.first == "mute") mute = p.second == "0" || p.second == "1" ? v : -2;
    else return "The device doesn't know that setting. Update it first.";
  }
  if (sleepS == -2 || volume == -2 || invert == -2 || mute == -2) return "That value isn't one the device takes.";
  if (mute >= 0) audioSetAlertsMuted(mute == 1);
  if (sleepS > 0) powerSetSleepSeconds(sleepS);
  if (volume >= 0 && volume != audioVolume()) {
    audioSetVolume(volume);
    audioChime();  // how loud it is now (silent at 0), as in Settings > Volume
  }
  if (invert >= 0 && (invert == 1) != display.inverted) {
    displaySetInverted(invert == 1);
    redrawFull = true;
  }
  return nullptr;
}

// The phone's clock, in seconds since 1970 (UTC). Anything before 2024 is a phone that doesn't know the time.
static const char *saveTime(const String &body) {
  for (const auto &p : formPairs(body.c_str()))
    if (p.first == "t") {
      const long long t = atoll(p.second.c_str());
      if (t < 1704067200LL || t > 4102444800LL) return "That time doesn't look right.";
      clockSet((time_t)t);
      return nullptr;
    }
  return "No time sent.";
}

// The WiFi names around, strongest first, once before the hotspot starts (it can't look while phones are on it).
static void scanNearby() {
  WiFi.mode(WIFI_STA);
  const int n = WiFi.scanNetworks();  // ~2 s
  std::vector<int> order;
  for (int i = 0; i < n; i++) order.push_back(i);
  std::sort(order.begin(), order.end(), [](int a, int b) { return WiFi.RSSI(a) > WiFi.RSSI(b); });
  std::vector<String> names;
  for (int i : order) {
    const String name = WiFi.SSID(i);
    if (name.length() && std::find(names.begin(), names.end(), name) == names.end() && names.size() < 15)
      names.push_back(name);
  }
  WiFi.scanDelete();
  nearby = "[";
  for (const String &name : names) nearby += (nearby.length() > 1 ? ",\"" : "\"") + str(json(name.c_str())) + "\"";
  nearby += "]";
}

static void post(WiFiClient &c, const Request &r) {
  const char *error = "Not found.";
  if (r.path == "/set") error = saveCredentials(r.body);
  else if (r.path == "/settings") error = saveSettings(r.body);
  else if (r.path == "/time") error = saveTime(r.body);
  else if (r.path == "/clear") {  // Remove everything: WiFi, keys, GitHub, the calendar link
    if (jobBusy()) error = "A note is sending. Try again in a moment.";
    else credClearAll(), changed = false, error = nullptr;
  } else if (r.path == "/done") {
    stopAt = millis() + 500;  // the answer goes out first
    done = true;
    error = nullptr;
  }
  reply(c, error ? 400 : 200, "application/json", answer(error));
}

// "Export": every note as one .txt, newest first.
static void sendExport(WiFiClient &c) {
  String all;
  if (storeReady())
    for (const NoteInfo &n : storeList()) {
      if (!n.text) continue;
      all += str(plainText(parse(storeReadNote(n.id).c_str()), whenOf(n.id.c_str()))) + "\n\n---\n\n";
    }
  reply(c, 200, "text/plain; charset=utf-8", all, "Content-Disposition: attachment; filename=\"unidex notes.txt\"\r\n");
}

// A recording, from the card. Phones ask for pieces ("Range: bytes=start-end") to play and seek, and need the
// answer 206 with Content-Range for that; without a Range the whole file comes. ?dl=1 makes it a download.
// Only the headers go out here: the bytes follow from serveHttp, a piece at a time.
static void startAudio(Connection &conn, const Request &r) {
  const String id = r.arg("id");
  conn.file = storeValidId(id) ? storageOpen(storeWavPath(id).c_str()) : fs::File();
  if (!conn.file || conn.file.isDirectory()) return reply(conn.client, 404, "text/plain", String("Not found"));
  const size_t total = conn.file.size();
  size_t first = 0, last = total ? total - 1 : 0;
  const bool ranged = r.range.startsWith("bytes=");
  if (ranged && !byteRange(r.range.substring(6).c_str(), total, first, last)) {
    conn.file.close();
    return reply(conn.client, 416, "text/plain", "", 0, "Content-Range: bytes */" + String(total) + "\r\n");
  }
  String headers = "Accept-Ranges: bytes\r\n";
  if (ranged) headers += "Content-Range: bytes " + String(first) + "-" + String(last) + "/" + String(total) + "\r\n";
  if (r.arg("dl") == "1") headers += "Content-Disposition: attachment; filename=\"unidex " + id + ".wav\"\r\n";
  const String head = header(ranged ? 206 : 200, "audio/wav", last - first + 1, headers);
  if (!send(conn.client, head.c_str(), head.length())) return conn.file.close();
  conn.file.seek(first);
  conn.remaining = last - first + 1;
}

static void handle(Connection &conn, const Request &r) {
  WiFiClient &c = conn.client;
  const uint32_t t0 = millis();
  const String self = WiFi.softAPIP().toString();
  if (r.path == "/favicon.ico" || r.path.startsWith("/apple-touch-icon")) return reply(c, 204, "text/plain", "", 0);
  lastUse = t0;
  if (!firstAt) firstAt = t0;
  served++;
  if (r.host != self) {
    // The phone's internet check (iPhone and Mac captive.apple.com/hotspot-detect.html, Android /generate_204,
    // Windows /connecttest.txt) or a site typed by hand: sending it here, instead of the answer it expects,
    // makes the phone open the page by itself.
    reply(c, 302, "text/plain", "", 0, "Location: http://" + self + "/\r\n");
    return logRequest("probe", t0);
  }
  if (r.method == "POST") {
    // Saves come only from the page itself: another page (one open from before joining) can't send them here.
    if (r.origin.length() && r.origin != "http://" + self) reply(c, 400, "application/json", answer("Not from this page."));
    else post(c, r);
    logRequest("save", t0);
  } else if (r.path == "/setup.json") {
    reply(c, 200, "application/json", setupJson());
    logRequest("setup", t0);
  } else if (r.path == "/notes.json") {
    reply(c, 200, "application/json", listJson);
    logRequest("list", t0);
  } else if (r.path == "/note.json") {
    const String id = r.arg("id");
    if (storeValidId(id)) reply(c, 200, "application/json", noteJson(id));
    else reply(c, 404, "application/json", String("{}"));
    logRequest("note", t0);
  } else if (r.path == "/note.txt") {
    sendText(c, r.arg("id"));
    logRequest("txt", t0);
  } else if (r.path == "/export.txt") {
    sendExport(c);
    logRequest("export", t0);
  } else if (r.path == "/audio") {
    startAudio(conn, r);
    logRequest("audio", t0);
  } else {
    sendPage(c);
    logRequest("page", t0);
  }
}

// A header's value ("" if it isn't there). `lower` is the headers in lower case, `name` like "\nhost:".
static String headerValue(const String &head, const String &lower, const char *name) {
  const int at = lower.indexOf(name);
  if (at < 0) return "";
  const int from = at + strlen(name);
  String v = head.substring(from, head.indexOf('\r', from));
  v.trim();
  return v;
}

// "GET /path?query HTTP/1.1", the headers that matter, and the body after the blank line at `end`.
static Request parseRequest(const String &head, int end) {
  Request r;
  const int sp1 = head.indexOf(' '), sp2 = head.indexOf(' ', sp1 + 1);
  r.method = sp1 < 0 ? "GET" : head.substring(0, sp1);
  String target = sp1 < 0 || sp2 < 0 ? "/" : head.substring(sp1 + 1, sp2);
  const int q = target.indexOf('?');
  r.path = q < 0 ? target : target.substring(0, q);
  r.query = q < 0 ? "" : target.substring(q + 1);
  String lower = head.substring(0, end + 2);
  lower.toLowerCase();
  r.range = headerValue(head, lower, "\nrange:");
  r.range.replace(" ", "");
  r.host = headerValue(head, lower, "\nhost:");
  const int colon = r.host.indexOf(':');
  if (colon >= 0) r.host = r.host.substring(0, colon);
  r.origin = headerValue(head, lower, "\norigin:");
  r.body = head.substring(end + 4);
  return r;
}

// How long the body after the headers is (Content-Length), or -1 if it's more than a save could need.
static long bodyLength(const String &head, int end) {
  String lower = head.substring(0, end + 2);
  lower.toLowerCase();
  const String v = headerValue(head, lower, "\ncontent-length:");
  const long n = v.length() ? v.toInt() : 0;
  return n < 0 || n > (long)BODY_MAX ? -1 : n;
}

static void serveHttp() {
  for (WiFiClient incoming = http.accept(); incoming; incoming = http.accept()) {
    int slot = 0;
    for (int i = 0; i < CONNECTIONS; i++) {
      if (!connections[i].client) {
        slot = i;
        break;
      }
      if (connections[i].since < connections[slot].since) slot = i;  // all busy: the one waiting longest goes
    }
    connections[slot].client.stop();
    connections[slot].file.close();
    incoming.setNoDelay(true);  // small replies go out at once
    connections[slot] = {incoming, "", millis(), fs::File(), 0};
  }
  for (Connection &conn : connections) {
    if (!conn.client) continue;
    if (conn.remaining) {  // sending a recording: the next piece
      static uint8_t piece[4096];
      const size_t n = conn.file.read(piece, min(conn.remaining, sizeof piece));
      const size_t sent = n && conn.client.connected() ? conn.client.write(piece, n) : 0;
      conn.remaining = sent == n && n ? conn.remaining - n : 0;
      lastUse = millis();  // still in use: no idle stop mid-playback
      if (conn.remaining) continue;
      conn.file.close();
      conn.client.stop();
      conn.client = WiFiClient();
      conn.head = "";
      continue;
    }
    uint8_t buf[256];
    while (conn.client.available() && conn.head.length() < HEAD_MAX + BODY_MAX) {
      const int n = conn.client.read(buf, sizeof buf);
      if (n <= 0) break;
      conn.head.concat((const char *)buf, n);
    }
    const int end = conn.head.indexOf("\r\n\r\n");
    const bool waiting = conn.client.connected() && millis() - conn.since < REQUEST_WAIT_MS;
    if (end >= 0) {
      const long body = bodyLength(conn.head, end);
      if (body < 0) {
        reply(conn.client, 400, "application/json", answer("That's too much to save at once."));
      } else if (conn.head.length() >= (size_t)end + 4 + body) {
        handle(conn, parseRequest(conn.head.substring(0, end + 4 + body), end));
        if (conn.remaining) continue;  // a recording: it goes out over the next loop turns
      } else if (waiting) {
        continue;  // the rest of a save's form is on its way
      }
    } else if (waiting && conn.head.length() < HEAD_MAX) {
      continue;  // still waiting for the request
    }
    conn.client.stop();
    conn.client = WiFiClient();
    conn.head = "";
  }
}

const char *phoneStart(PhoneTab startTab) {
  tab = startTab;
  if (on) return nullptr;
  if (netClaimed()) return "Sending a note: try soon";  // over the normal WiFi
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
  snprintf(ssid, sizeof ssid, "unidex-%02X%02X", mac[4], mac[5]);
  // No look-alike characters (l/1, O/0), so it can be typed from the screen if the camera can't scan.
  static const char ALPHABET[] = "abcdefghjkmnpqrstuvwxyz23456789";
  for (size_t i = 0; i < sizeof password - 2; i++) password[i] = ALPHABET[esp_random() % (sizeof ALPHABET - 1)];
  password[sizeof password - 2] = 0;
  snprintf(joinCode, sizeof joinCode, "WIFI:T:WPA;S:%s;P:%s;;", ssid, password);
  buildList();  // reads the card once, before the radio starts
  netOff();
  setCpuFrequencyMhz(240);  // full speed while the radio is on (as for normal WiFi): phones join faster
  if (tab == PhoneTab::Notes) nearby = "[]";
  else scanNearby();  // for the WiFi name box on Setup
  WiFi.mode(WIFI_AP);
  if (!WiFi.softAP(ssid, password)) {
    WiFi.mode(WIFI_OFF);
    setCpuFrequencyMhz(80);
    return "WiFi didn't start: try again";
  }
  WiFi.setSleep(false);                             // answer at once, no power-save naps
  esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_BW_HT20);  // a 20 MHz channel: phones join it more reliably
  http.begin();
  dns.start(53, "*", WiFi.softAPIP());
  powerHold();     // no light or deep sleep while the phone may be reading
  netClaim(true);  // the radio is the hotspot's: the background sends (Notes, Reminders) wait until it's off
  on = true, served = 0, firstAt = 0, shown = 0, stopAt = 0, changed = false, done = false, redrawFull = false;
  startedAt = lastUse = millis();
#if UNIDEX_DEV
  joinedAt = addressAt = 0;
  reqLog[0] = 0;
  static bool watching;
  if (!watching) {
    WiFi.onEvent([](arduino_event_id_t, arduino_event_info_t) { if (!joinedAt) joinedAt = millis(); },
                 ARDUINO_EVENT_WIFI_AP_STACONNECTED);
    WiFi.onEvent([](arduino_event_id_t, arduino_event_info_t) { if (!addressAt) addressAt = millis(); },
                 ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED);
    watching = true;
  }
  devSetDetail(phoneTimes);
#endif
  return nullptr;
}

void phoneStop() {
  if (!on) return;
  dns.stop();
  for (Connection &conn : connections)
    conn.client.stop(), conn.client = WiFiClient(), conn.head = "", conn.file.close(), conn.remaining = 0;
  http.end();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  netClaim(false);
  setCpuFrequencyMhz(80);
  powerRelease();
#if UNIDEX_DEV
  devSetDetail(nullptr);
#endif
  on = false;
}

bool phoneOn() { return on; }

Redraw phoneTick() {
  if (!on) return Redraw::None;
  dns.processNextRequest();
  serveHttp();
  if (millis() - lastUse > IDLE_MS || (stopAt && (int32_t)(millis() - stopAt) >= 0)) {
    phoneStop();
    return Redraw::Partial;
  }
  if (redrawFull) {
    redrawFull = false;
    return Redraw::Full;
  }
  // "Phone connected", once its first few requests are answered: a screen refresh holds everything up for
  // ~0.5 s, and the phone is waiting on those answers to open the page.
  return !shown && firstAt && millis() - firstAt > 3000 ? Redraw::Partial : Redraw::None;
}

// The QR code the camera joins with, then the name and password to type.
void phoneDraw(const char *title, const char *computer) {
  shown = served;
  drawHeader(title);
  QRCode qr;
  uint8_t modules[qrcode_getBufferSize(3)];
  if (qrcode_initText(&qr, modules, 3, ECC_LOW, joinCode) == 0) {  // version 3: 29x29, 4 px each (3 with `computer`)
    const int scale = computer ? 3 : 4, x0 = (display.width() - qr.size * scale) / 2, y0 = CONTENT_TOP + (computer ? 3 : 4);
    for (uint8_t y = 0; y < qr.size; y++)
      for (uint8_t x = 0; x < qr.size; x++)
        if (qrcode_getModule(&qr, x, y)) display.fillRect(x0 + x * scale, y0 + y * scale, scale, scale, BLACK);
  }
  char line[48];
  if (computer) {  // everything above the hints
    display.setFont(FONT_SMALL);
    drawCenteredLine(shown ? "Phone connected" : "Scan with your phone", 128);
    display.setFont(FONT_TINY);
    snprintf(line, sizeof line, "%s   %s", ssid, password);
    drawCenteredLine(line, 142);
    snprintf(line, sizeof line, "Computer: %s", computer);
    drawCenteredLine(line, 157);
    return;
  }
  display.setFont(FONT_SMALL);
  drawCenteredLine(shown ? "Phone connected" : "Scan with your camera", 162);
  display.setFont(FONT_TINY);
  snprintf(line, sizeof line, "%s   %s", ssid, password);
  drawCenteredLine(line, 179);
  drawCenteredLine("Hold A to stop", 194);  // descenders clear of the 2 px margin
}

void phoneActivity() { lastUse = millis(); }
bool phoneChanged() { return changed; }
bool phoneDone() { return done; }
const char *phoneSsid() { return ssid; }
const char *phonePassword() { return password; }
const char *phoneJoinCode() { return joinCode; }
