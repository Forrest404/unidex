#include "phone.h"
#include <DNSServer.h>
#include <WiFi.h>
#include <esp_mac.h>
#include <esp_wifi.h>
#include "phone_page.h"
#include "phone_text.h"
#include "store.h"
#include "../../core/devtools.h"
#include "../../core/net.h"
#include "../../core/power.h"

#if UNIDEX_DEV
static const uint32_t IDLE_MS = 60000;  // test build: a minute, so the auto-stop can be tested
#else
static const uint32_t IDLE_MS = 5 * 60000;
#endif

static WiFiServer http(80, 8);
static DNSServer dns;  // every name the phone looks up points here, so its internet check finds this page
static bool on;
static uint32_t lastUse, served, startedAt, firstAt;  // firstAt: the first request from a phone
static char ssid[16], password[12], joinCode[64];

#if UNIDEX_DEV
// Test build: when a phone got through each step of joining, in ms after the start (X STATE detail).
static uint32_t joinedAt, addressAt;
static char reqLog[96];  // the last requests and how long each took
static char times[160];
static const char *phoneTimes() {
  snprintf(times, sizeof times, "joined=%lu address=%lu first=%lu%s", joinedAt ? joinedAt - startedAt : 0,
           addressAt ? addressAt - startedAt : 0, firstAt ? firstAt - startedAt : 0, reqLog);
  return times;
}
#endif

// --- a small web server ---
// Several connections at once: phones open a spare connection ahead of time and leave it empty, and a server
// that waits on one connection at a time (the core's WebServer) would sit on that one for seconds while the
// real request queues behind it. Each request is answered and closed straight away.

struct Request {
  String path, query, host;
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
};
static const int CONNECTIONS = 6;
static const uint32_t REQUEST_WAIT_MS = 4000;  // an empty connection is dropped after this
static Connection connections[CONNECTIONS];

static String header(int code, const char *type, size_t len, const String &headers = "") {
  const char *status = code == 200 ? "OK" : code == 204 ? "No Content" : code == 302 ? "Found" : "Not Found";
  return "HTTP/1.1 " + String(code) + " " + status + "\r\nContent-Type: " + type + "\r\nContent-Length: " + String(len) +
         "\r\nCache-Control: no-store\r\nConnection: close\r\n" + headers + "\r\n";
}

static void reply(WiFiClient &c, int code, const char *type, const char *body, size_t len,
                  const String &headers = "") {
  const String head = header(code, type, len, headers);
  c.write((const uint8_t *)head.c_str(), head.length());
  if (len) c.write((const uint8_t *)body, len);
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

// The page with the note list already inside it (in place of "/*NOTES*/null"), so the list draws with the page.
static void sendPage(WiFiClient &c) {
  static const char MARK[] = "/*NOTES*/null";
  const char *at = strstr(PHONE_PAGE, MARK);
  const size_t before = at - PHONE_PAGE, after = sizeof PHONE_PAGE - 1 - before - (sizeof MARK - 1);
  const String head = header(200, "text/html; charset=utf-8", before + listJson.length() + after);
  c.write((const uint8_t *)head.c_str(), head.length());
  c.write((const uint8_t *)PHONE_PAGE, before);
  c.write((const uint8_t *)listJson.c_str(), listJson.length());
  c.write((const uint8_t *)at + sizeof MARK - 1, after);
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

static void handle(WiFiClient &c, const Request &r) {
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
  if (r.path == "/notes.json") {
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
  } else {
    sendPage(c);
    logRequest("page", t0);
  }
}

static Request parseRequest(const String &head) {  // "GET /path?query HTTP/1.1" and the Host header
  Request r;
  const int sp1 = head.indexOf(' '), sp2 = head.indexOf(' ', sp1 + 1);
  String target = sp1 < 0 || sp2 < 0 ? "/" : head.substring(sp1 + 1, sp2);
  const int q = target.indexOf('?');
  r.path = q < 0 ? target : target.substring(0, q);
  r.query = q < 0 ? "" : target.substring(q + 1);
  String lower = head;
  lower.toLowerCase();
  const int h = lower.indexOf("\nhost:");
  if (h >= 0) {
    r.host = head.substring(h + 6, head.indexOf('\r', h + 6));
    r.host.trim();
    const int colon = r.host.indexOf(':');
    if (colon >= 0) r.host = r.host.substring(0, colon);
  }
  return r;
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
    incoming.setNoDelay(true);  // small replies go out at once
    connections[slot] = {incoming, "", millis()};
  }
  for (Connection &conn : connections) {
    if (!conn.client) continue;
    uint8_t buf[256];
    while (conn.client.available() && conn.head.length() < 4096) {
      const int n = conn.client.read(buf, sizeof buf);
      if (n <= 0) break;
      conn.head.concat((const char *)buf, n);
    }
    if (conn.head.indexOf("\r\n\r\n") >= 0) {
      handle(conn.client, parseRequest(conn.head));
    } else if (conn.client.connected() && millis() - conn.since < REQUEST_WAIT_MS && conn.head.length() < 4096) {
      continue;  // still waiting for the request
    }
    conn.client.stop();
    conn.client = WiFiClient();
    conn.head = "";
  }
}

bool phoneStart() {
  if (on) return true;
  if (netClaimed()) return false;  // a note is sending over the normal WiFi
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
  WiFi.mode(WIFI_AP);
  if (!WiFi.softAP(ssid, password)) {
    WiFi.mode(WIFI_OFF);
    setCpuFrequencyMhz(80);
    return false;
  }
  WiFi.setSleep(false);                             // answer at once, no power-save naps
  esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_BW_HT20);  // a 20 MHz channel: phones join it more reliably
  http.begin();
  dns.start(53, "*", WiFi.softAPIP());
  powerHold();  // no light or deep sleep while the phone may be reading
  on = true, served = 0, firstAt = 0;
  startedAt = lastUse = millis();
#if UNIDEX_DEV
  joinedAt = addressAt = 0;
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
  return true;
}

void phoneStop() {
  if (!on) return;
  dns.stop();
  for (Connection &conn : connections) conn.client.stop(), conn.client = WiFiClient(), conn.head = "";
  http.end();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  setCpuFrequencyMhz(80);
  powerRelease();
#if UNIDEX_DEV
  devSetDetail(nullptr);
#endif
  on = false;
}

bool phoneOn() { return on; }

void phonePoll() {
  if (!on) return;
  dns.processNextRequest();
  serveHttp();
  if (millis() - lastUse > IDLE_MS) phoneStop();
}

void phoneActivity() { lastUse = millis(); }
const char *phoneSsid() { return ssid; }
const char *phonePassword() { return password; }
const char *phoneJoinCode() { return joinCode; }
uint32_t phoneServed() { return served; }
uint32_t phoneConnectedMs() { return firstAt ? millis() - firstAt : 0; }
