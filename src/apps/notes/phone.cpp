#include "phone.h"
#include <WebServer.h>
#include <WiFi.h>
#include <esp_mac.h>
#include "../../core/net.h"
#include "../../core/power.h"

#if UNIDEX_DEV
static const uint32_t IDLE_MS = 60000;  // test build: a minute, so the auto-stop can be tested
#else
static const uint32_t IDLE_MS = 5 * 60000;
#endif

static WebServer server(80);
static bool on;
static uint32_t lastUse, served;
static char ssid[16], password[12], joinCode[64];

static void used() { lastUse = millis(); }

static void handleRoot() {
  used();
  served++;
  server.send(200, "text/html; charset=utf-8",
              "<!doctype html><meta name=viewport content='width=device-width,initial-scale=1'>"
              "<title>unidex</title><body style='font:20px system-ui;margin:2em'><h1>unidex</h1>"
              "<p>Connected. Your notes will appear here.</p>");
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
  WiFi.mode(WIFI_AP);
  if (!WiFi.softAP(ssid, password)) {
    WiFi.mode(WIFI_OFF);
    return false;
  }
  static bool routed;
  if (!routed) {
    server.on("/", handleRoot);
    server.onNotFound(handleRoot);
    routed = true;
  }
  server.begin();
  powerHold();  // no light or deep sleep while the phone may be reading
  on = true, served = 0;
  used();
  return true;
}

void phoneStop() {
  if (!on) return;
  server.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  powerRelease();
  on = false;
}

bool phoneOn() { return on; }

void phonePoll() {
  if (!on) return;
  server.handleClient();
  if (millis() - lastUse > IDLE_MS) phoneStop();
}

void phoneActivity() { used(); }
const char *phoneSsid() { return ssid; }
const char *phonePassword() { return password; }
const char *phoneJoinCode() { return joinCode; }
uint32_t phoneServed() { return served; }
