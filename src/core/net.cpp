#include "net.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <functional>
#include <memory>
#include <esp_wpa2.h>
#include "credentials.h"
#include "storage.h"
#include "wifi_logic.h"
#include "power.h"
#include "battery.h"
#include "devtools.h"

extern const uint8_t caBundle[] asm("_binary_certs_x509_crt_bundle_bin_start");  // see certs/README.md

static const uint32_t WIFI_TIMEOUT_MS = 20000, IO_TIMEOUT_MS = 30000;  // enterprise logins take longer

static volatile bool begun;   // netBegin() started a connection that netOff() hasn't ended
static uint32_t beganAt, attemptAt;
static int tryOrder[wifi::SLOTS], tryCount, tryAt;  // the saved networks in the order to try, and which is being tried
static uint32_t attemptMs;
static bool sawMissing;  // a network we tried wasn't in range

// Starts joining saved network `slot` (the radio is already in station mode).
static void joinSlot(int slot) {
  const String ssid = credGet(credWifiName(slot, "ssid").c_str());
  const String user = credGet(credWifiName(slot, "user").c_str());
  const String pass = credGet(credWifiName(slot, "pass").c_str());
  attemptAt = millis();
  attemptMs = user.length() ? wifi::ATTEMPT_ENTERPRISE_MS : wifi::ATTEMPT_MS;
  WiFi.disconnect(false);  // drop an attempt still going on another network
  esp_wifi_sta_wpa2_ent_clear_ca_cert();
  if (user.length()) {  // eduroam and other WPA2-Enterprise networks: PEAP with username + password
    // The stack keeps a pointer to the PEM, so it must outlive the login; the length includes the NUL for mbedTLS.
    static String pem;
    const String ca = credGet(credWifiName(slot, "ca").c_str());
    if (ca.length()) {
      pem = "-----BEGIN CERTIFICATE-----\n";
      for (size_t i = 0; i < ca.length(); i += 64) pem += ca.substring(i, i + 64) + "\n";
      pem += "-----END CERTIFICATE-----\n";
      esp_wifi_sta_wpa2_ent_set_ca_cert((const uint8_t *)pem.c_str(), pem.length() + 1);
    }
    WiFi.begin(ssid.c_str(), WPA2_AUTH_PEAP, user.c_str(), user.c_str(), pass.c_str());
  } else {
    esp_wifi_sta_wpa2_ent_disable();  // begin() never turns enterprise mode off after an eduroam login
    WiFi.begin(ssid.c_str(), pass.c_str());
  }
}

const char *netBegin() {
  bool has[wifi::SLOTS];
  for (int i = 0; i < wifi::SLOTS; i++) has[i] = credWifiSaved(i);
  tryCount = wifi::order(has, storageGetInt("wifi_last", -1), tryOrder);
  if (!tryCount) return "WiFi not set up";
  if (devNetFail()) return "WiFi failed";  // test build switches (X NETFAIL 1, X FAKE 1)
  if (const char *low = batteryTooLow()) return low;
  if (devFakeCloud() || begun || WiFi.status() == WL_CONNECTED) return nullptr;
  begun = true;
  beganAt = millis();
  tryAt = 0;
  sawMissing = false;
  setCpuFrequencyMhz(240);  // full speed only while the radio is on (as in the Dex scan)
  WiFi.mode(WIFI_STA);
  joinSlot(tryOrder[0]);
  return nullptr;
}

// Moves on to the next saved network when this one isn't in range, refused the login, or took too long.
// False when there is none left.
static bool nextNetwork() {
  if (tryAt + 1 >= tryCount) return false;
  joinSlot(tryOrder[++tryAt]);
  return true;
}

const char *netConnect() {
  if (const char *err = netBegin()) return err;
  if (devFakeCloud()) return nullptr;
  // Time spent connecting since netBegin() counts (it may have started with a recording), with a few
  // seconds' grace now in case it's nearly there. Each saved network gets its turn within a total of 45 s.
  uint32_t deadline = max<uint32_t>(beganAt + WIFI_TIMEOUT_MS, millis() + 5000);
  const uint32_t limit = max<uint32_t>(beganAt + 45000, millis() + 5000);
  while (WiFi.status() != WL_CONNECTED && (int32_t)(deadline - millis()) > 0) {
    powerAlive();
    delay(100);
    const wl_status_t st = WiFi.status();
    // (A status left over from the previous network can still show in the first moments of an attempt.)
    const bool settled = millis() - attemptAt > 1500;
    const bool missing = settled && st == WL_NO_SSID_AVAIL, refused = settled && st == WL_CONNECT_FAILED;
    if (missing) sawMissing = true;
    if ((missing || refused || millis() - attemptAt > attemptMs) && nextNetwork())
      deadline = min<uint32_t>(limit, max<uint32_t>(deadline, millis() + attemptMs));
  }
  powerActivity();
  if (WiFi.status() == WL_CONNECTED) {
    storagePutInt("wifi_last", tryOrder[tryAt]);  // next time, this one first
    return nullptr;
  }
  const bool missing = WiFi.status() == WL_NO_SSID_AVAIL || (sawMissing && tryCount > 1);
  netOff();
  return missing ? "WiFi not found" : "WiFi failed";
}

static volatile bool claimed;
void netClaim(bool on) { claimed = on; }
bool netClaimed() { return claimed; }

void netOff() {
  begun = false;
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  setCpuFrequencyMhz(80);
}

static bool readLine(WiFiClientSecure &c, String &line) {
  line = "";
  const uint32_t t0 = millis();
  while (millis() - t0 < IO_TIMEOUT_MS) {
    while (c.available()) {
      char ch = c.read();
      if (ch == '\n') return true;
      if (ch != '\r') line += ch;
    }
    if (!c.connected()) return line.length() > 0;
    powerAlive();
    delay(2);
  }
  return false;
}

typedef std::function<bool(const uint8_t *, size_t)> Sink;  // where the reply's body goes; false = stop reading

// Reads exactly `n` bytes (or to the end) into the sink. False if the sink stopped it.
static bool readBytes(WiFiClientSecure &c, size_t n, const Sink &sink) {
  uint8_t buf[512];
  uint32_t t0 = millis();
  while (n > 0 && millis() - t0 < IO_TIMEOUT_MS) {
    const int got = c.read(buf, min(n, sizeof buf));
    if (got > 0) {
      if (!sink(buf, got)) return false;
      n -= got;
      t0 = millis();
      powerAlive();  // a big download is a long wait in the main loop
    } else if (!c.connected()) {
      break;
    } else {
      powerAlive();
      delay(2);
    }
  }
  return true;
}

static int request(const char *host, const char *method, const char *path, const String &headers,
                   const NetPart *parts, int nParts, const Sink &sink, const char *header = nullptr,
                   String *value = nullptr) {
  // On the heap: the client holds ~4 KB of mbedTLS state, too much next to the handshake on the 8 KB loop stack.
  std::unique_ptr<WiFiClientSecure> client(new WiFiClientSecure);
  WiFiClientSecure &c = *client;
  c.setCACertBundle(caBundle);
  c.setTimeout(IO_TIMEOUT_MS / 1000);
  if (!c.connect(host, 443)) return -1;

  size_t length = 0;
  for (int i = 0; i < nParts; i++) length += parts[i].len;
  String head = String(method) + " " + path + " HTTP/1.1\r\nHost: " + host + "\r\n" + headers +
                "Content-Length: " + length + "\r\nConnection: close\r\n\r\n";
  c.print(head);
  uint8_t buf[1024];
  for (int i = 0; i < nParts; i++) {
    const NetPart &p = parts[i];
    if (p.data) {
      for (size_t sent = 0; sent < p.len;) {  // large buffers in slices, so a slow link can't stall one write
        const size_t n = min((size_t)4096, p.len - sent);
        if (c.write(p.data + sent, n) != n) return -1;
        sent += n;
      }
    } else {
      for (size_t left = p.len; left > 0;) {
        const int n = p.file->read(buf, min(left, sizeof buf));
        if (n <= 0 || c.write(buf, n) != (size_t)n) return -1;
        left -= n;
      }
    }
    powerActivity();  // uploads take seconds: don't let the idle timer cut in afterwards
  }

  String line;
  if (!readLine(c, line) || !line.startsWith("HTTP/1.")) return -1;
  const int status = line.substring(9, 12).toInt();
  bool chunked = false;
  long contentLength = -1;
  while (readLine(c, line) && line.length()) {
    String lower = line;
    lower.toLowerCase();
    if (lower.startsWith("transfer-encoding:") && lower.indexOf("chunked") > 0) chunked = true;
    if (lower.startsWith("content-length:")) contentLength = lower.substring(15).toInt();
    if (header && lower.startsWith(header) && lower[strlen(header)] == ':') {  // the one header asked for
      *value = line.substring(strlen(header) + 1);
      value->trim();
    }
  }
  if (chunked) {
    for (;;) {  // <hex size>\r\n<data>\r\n ... 0\r\n\r\n
      if (!readLine(c, line)) break;
      const long size = strtol(line.c_str(), nullptr, 16);
      if (size <= 0) break;
      if (!readBytes(c, size, sink)) break;
      readLine(c, line);
    }
  } else {
    readBytes(c, contentLength >= 0 ? (size_t)contentLength : SIZE_MAX, sink);
  }
  c.stop();
  powerActivity();
  return status;
}

int netHttps(const char *host, const char *method, const char *path, const String &headers,
             const NetPart *parts, int nParts, String &body, size_t maxBody) {
  body = "";
  return request(host, method, path, headers, parts, nParts, [&](const uint8_t *p, size_t n) {
    for (size_t i = 0; i < n && body.length() < maxBody; i++) body += (char)p[i];
    return true;
  });
}

int netHttpsGet(const char *host, const char *path, const std::function<bool(const uint8_t *, size_t)> &sink,
                String &location) {
  location = "";
  return request(host, "GET", path, "User-Agent: unidex\r\n", nullptr, 0, sink, "location", &location);
}

int netHttpsHeader(const char *host, const char *path, const char *name, String &value) {
  value = "";
  return request(host, "GET", path, "User-Agent: unidex\r\n", nullptr, 0, [](const uint8_t *, size_t) { return true; },
                 name, &value);
}

