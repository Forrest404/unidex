#include "phone.h"
#include <DNSServer.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_mac.h>
#include <esp_wifi.h>
#include "../../core/devtools.h"
#include "../../core/net.h"
#include "../../core/power.h"

#if UNIDEX_DEV
static const uint32_t IDLE_MS = 60000;  // test build: a minute, so the auto-stop can be tested
#else
static const uint32_t IDLE_MS = 5 * 60000;
#endif

static WebServer server(80);
static DNSServer dns;  // every name the phone looks up points here, so its internet check finds this page
static bool on;
static uint32_t lastUse, served, startedAt, firstAt;  // firstAt: the first request from a phone
static char ssid[16], password[12], joinCode[64];

static void used() {
  lastUse = millis();
  if (!firstAt) firstAt = lastUse;
}

#if UNIDEX_DEV
// Test build: when a phone got through each step of joining, in ms after the start (X STATE detail).
static uint32_t joinedAt, addressAt;
static char times[64];
static const char *phoneTimes() {
  snprintf(times, sizeof times, "joined=%lu address=%lu first=%lu", joinedAt ? joinedAt - startedAt : 0,
           addressAt ? addressAt - startedAt : 0, firstAt ? firstAt - startedAt : 0);
  return times;
}
#endif

static void handleRoot() {
  used();
  served++;
  server.send(200, "text/html; charset=utf-8",
              "<!doctype html><meta name=viewport content='width=device-width,initial-scale=1'>"
              "<title>unidex</title><body style='font:20px system-ui;margin:2em'><h1>unidex</h1>"
              "<p>Connected. Your notes will appear here.</p>");
}

// Anything that isn't this device's own page: the phone's internet check (iPhone and Mac
// captive.apple.com/hotspot-detect.html, Android /generate_204, Windows /connecttest.txt) or a site
// typed by hand. Sending it here, instead of the answer it expects, makes the phone open the page by itself.
static void handleOther() {
  used();
  served++;
  if (server.hostHeader() == WiFi.softAPIP().toString()) return handleRoot();
  server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/");
  server.send(302, "text/plain", "");
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
  static bool routed;
  if (!routed) {
    server.on("/", handleRoot);
    server.onNotFound(handleOther);
    routed = true;
  }
  server.begin();
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
  server.stop();
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
  server.handleClient();
  if (millis() - lastUse > IDLE_MS) phoneStop();
}

void phoneActivity() { used(); }
const char *phoneSsid() { return ssid; }
const char *phonePassword() { return password; }
const char *phoneJoinCode() { return joinCode; }
uint32_t phoneServed() { return served; }
uint32_t phoneConnectedMs() { return firstAt ? millis() - firstAt : 0; }
