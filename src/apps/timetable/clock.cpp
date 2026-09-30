#include "clock.h"
#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <esp_sntp.h>
#include <sys/time.h>
#if __has_include("../../secrets.h")
#include "../../secrets.h"
#else
#include "../../secrets.example.h"
#endif

static const char *TZ_LONDON = "GMT0BST,M3.5.0/1,M10.5.0";
static const uint8_t PCF_ADDR = 0x51, PCF_SECONDS = 0x04;  // registers 0x04-0x0A: s m h day wday month year
static const int PIN_SDA = 47, PIN_SCL = 48;
static const uint32_t WIFI_TIMEOUT_MS = 10000, NTP_TIMEOUT_MS = 8000;

static uint8_t fromBcd(uint8_t v) { return (v >> 4) * 10 + (v & 0x0F); }
static uint8_t toBcd(uint8_t v) { return (v / 10) << 4 | v % 10; }

static void setTz(const char *tz) {
  setenv("TZ", tz, 1);
  tzset();
}

void clockBegin() {
  setTz(TZ_LONDON);
  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.beginTransmission(PCF_ADDR);
  Wire.write(PCF_SECONDS);
  if (Wire.endTransmission(false) != 0 || Wire.requestFrom((uint16_t)PCF_ADDR, (size_t)7) != 7) return;
  uint8_t r[7];
  for (uint8_t &b : r) b = Wire.read();
  if (r[0] & 0x80) return;  // oscillator stopped since it was last set: the time can't be trusted

  struct tm t = {};
  t.tm_sec = fromBcd(r[0] & 0x7F);
  t.tm_min = fromBcd(r[1] & 0x7F);
  t.tm_hour = fromBcd(r[2] & 0x3F);
  t.tm_mday = fromBcd(r[3] & 0x3F);
  t.tm_mon = fromBcd(r[5] & 0x1F) - 1;
  t.tm_year = fromBcd(r[6]) + 100;
  // The chip holds UTC. There's no timegm() here, so convert with TZ briefly set to UTC.
  setTz("UTC0");
  struct timeval tv = {mktime(&t), 0};
  setTz(TZ_LONDON);
  settimeofday(&tv, nullptr);
}

bool clockValid() {
  return time(nullptr) > 1704067200;  // 2024-01-01
}

static void writeChip() {
  time_t now = time(nullptr);
  struct tm t;
  gmtime_r(&now, &t);
  Wire.beginTransmission(PCF_ADDR);
  Wire.write(PCF_SECONDS);
  Wire.write(toBcd(t.tm_sec));  // writing seconds also clears the oscillator-stopped flag
  Wire.write(toBcd(t.tm_min));
  Wire.write(toBcd(t.tm_hour));
  Wire.write(toBcd(t.tm_mday));
  Wire.write(t.tm_wday);
  Wire.write(toBcd(t.tm_mon + 1));
  Wire.write(toBcd(t.tm_year - 100));
  Wire.endTransmission();
}

const char *clockSync() {
  if (!*WIFI_SSID) return "no WiFi set";
  const char *err = nullptr;
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < WIFI_TIMEOUT_MS) delay(100);
  if (WiFi.status() != WL_CONNECTED) {
    err = "WiFi failed";
  } else {
    // Wait for a real NTP answer: getLocalTime() alone returns at once if the clock was already set.
    sntp_set_sync_status(SNTP_SYNC_STATUS_RESET);
    configTzTime(TZ_LONDON, "pool.ntp.org", "time.google.com");
    t0 = millis();
    while (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED && millis() - t0 < NTP_TIMEOUT_MS) delay(100);
    if (sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) writeChip();
    else err = "no time server";
    sntp_stop();
  }
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
#if DEBUG
  Serial.printf("sync: %s, WiFi mode %d (0 = off)\n", err ? err : "ok", (int)WiFi.getMode());
#endif
  return err;
}
