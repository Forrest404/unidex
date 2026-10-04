#include "net.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <memory>
#include <esp_wpa2.h>
#include "credentials.h"
#include "power.h"
#include "devtools.h"

extern const uint8_t caBundle[] asm("_binary_certs_x509_crt_bundle_bin_start");  // see certs/README.md

static const uint32_t WIFI_TIMEOUT_MS = 20000, IO_TIMEOUT_MS = 30000;  // enterprise logins take longer

const char *netConnect() {
  const String ssid = credGet("wifi_ssid");
  if (!ssid.length()) return "WiFi not set up";
  if (devNetFail()) return "WiFi failed";  // test build switch (X NETFAIL 1)
  if (WiFi.status() == WL_CONNECTED) return nullptr;
  setCpuFrequencyMhz(240);  // full speed only while the radio is on (as in the Dex scan)
  WiFi.mode(WIFI_STA);
  const String user = credGet("wifi_user");
  esp_wifi_sta_wpa2_ent_clear_ca_cert();
  if (user.length()) {  // eduroam and other WPA2-Enterprise networks: PEAP with username + password
    // The stack keeps a pointer to the PEM, so it must outlive the login; the length includes the NUL for mbedTLS.
    static String pem;
    const String ca = credGet("wifi_ca");
    if (ca.length()) {
      pem = "-----BEGIN CERTIFICATE-----\n";
      for (size_t i = 0; i < ca.length(); i += 64) pem += ca.substring(i, i + 64) + "\n";
      pem += "-----END CERTIFICATE-----\n";
      esp_wifi_sta_wpa2_ent_set_ca_cert((const uint8_t *)pem.c_str(), pem.length() + 1);
    }
    WiFi.begin(ssid.c_str(), WPA2_AUTH_PEAP, user.c_str(), user.c_str(), credGet("wifi_pass").c_str());
  } else {
    esp_wifi_sta_wpa2_ent_disable();  // begin() never turns enterprise mode off after an eduroam login
    WiFi.begin(ssid.c_str(), credGet("wifi_pass").c_str());
  }
  const uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < WIFI_TIMEOUT_MS) delay(100);
  powerActivity();
  if (WiFi.status() == WL_CONNECTED) return nullptr;
  const bool missing = WiFi.status() == WL_NO_SSID_AVAIL;
  netOff();
  return missing ? "WiFi not found" : "WiFi failed";
}

void netOff() {
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
    delay(2);
  }
  return false;
}

// Reads exactly `n` bytes (or to the end), appending up to the cap.
static void readBytes(WiFiClientSecure &c, size_t n, String &body, size_t maxBody) {
  uint8_t buf[256];
  uint32_t t0 = millis();
  while (n > 0 && millis() - t0 < IO_TIMEOUT_MS) {
    const int got = c.read(buf, min(n, sizeof buf));
    if (got > 0) {
      for (int i = 0; i < got && body.length() < maxBody; i++) body += (char)buf[i];
      n -= got;
      t0 = millis();
    } else if (!c.connected()) {
      break;
    } else {
      delay(2);
    }
  }
}

int netHttps(const char *host, const char *method, const char *path, const String &headers,
             const NetPart *parts, int nParts, String &body, size_t maxBody) {
  body = "";
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
  }
  if (chunked) {
    for (;;) {  // <hex size>\r\n<data>\r\n ... 0\r\n\r\n
      if (!readLine(c, line)) break;
      const long size = strtol(line.c_str(), nullptr, 16);
      if (size <= 0) break;
      readBytes(c, size, body, maxBody);
      readLine(c, line);
    }
  } else {
    readBytes(c, contentLength >= 0 ? (size_t)contentLength : SIZE_MAX, body, maxBody);
  }
  c.stop();
  powerActivity();
  return status;
}
