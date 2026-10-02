#include "clock.h"
#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <esp_sntp.h>
#include <sys/time.h>
#include "net.h"

static const char *TZ_LONDON = "GMT0BST,M3.5.0/1,M10.5.0";
static const uint8_t PCF_ADDR = 0x51, PCF_CONTROL1 = 0x00, PCF_SECONDS = 0x04;  // 0x04-0x0A: s m h day wday month year
static const uint8_t CONTROL1_STOP = 0x20, CONTROL1_12H = 0x02;  // 0x00 = running, 24-hour mode
static const int PIN_SDA = 47, PIN_SCL = 48;
static const uint32_t NTP_TIMEOUT_MS = 8000;

static uint8_t fromBcd(uint8_t v) { return (v >> 4) * 10 + (v & 0x0F); }
static uint8_t toBcd(uint8_t v) { return (v / 10) << 4 | v % 10; }

static void setTz(const char *tz) {
  setenv("TZ", tz, 1);
  tzset();
}

static void start() {
  static bool started;
  if (started) return;
  started = true;
  setTz(TZ_LONDON);
  Wire.begin(PIN_SDA, PIN_SCL);
}

static void writeControl1(uint8_t value) {
  Wire.beginTransmission(PCF_ADDR);
  Wire.write(PCF_CONTROL1);
  Wire.write(value);
  Wire.endTransmission();
}

static void writeChip(const struct tm &t);

void clockBegin() {
  start();
  Wire.beginTransmission(PCF_ADDR);
  Wire.write(PCF_CONTROL1);
  if (Wire.endTransmission(false) != 0 || Wire.requestFrom((uint16_t)PCF_ADDR, (size_t)11) != 11) return;
  uint8_t control1 = Wire.read();
  for (int i = 1; i < 4; i++) Wire.read();  // control 2, offset, RAM byte
  uint8_t r[7];
  for (uint8_t &b : r) b = Wire.read();
  // Found halted or in 12-hour mode (the factory firmware left it so): its time is frozen or misread.
  // Start it in 24-hour mode. After a deep-sleep wake our own clock is still right, so put that back
  // on the chip; after power-on there's no good time, so leave it unset until the next sync.
  if (control1 & (CONTROL1_STOP | CONTROL1_12H)) {
    time_t now = time(nullptr);
    struct tm t = {};
    if (clockValid()) gmtime_r(&now, &t);
    else t.tm_mday = 1, t.tm_year = 100;  // 2000-01-01 reads back as "not set" (clockValid needs 2024+)
    writeChip(t);
    return;
  }
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

// Datasheet order for an exact set: stop the clock, write the time (UTC), start it again (24-hour mode).
static void writeChip(const struct tm &t) {
  writeControl1(CONTROL1_STOP);
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
  writeControl1(0x00);
}

// The current system time onto the chip.
static void writeChipNow() {
  time_t now = time(nullptr);
  struct tm t;
  gmtime_r(&now, &t);
  writeChip(t);
}


const char *clockSync() {
  const char *err = netConnect();  // the WiFi saved from the website's Notes page
  if (!err) {
    // Wait for a real NTP answer: getLocalTime() alone returns at once if the clock was already set.
    sntp_set_sync_status(SNTP_SYNC_STATUS_RESET);
    configTzTime(TZ_LONDON, "pool.ntp.org", "time.google.com");
    const uint32_t t0 = millis();
    while (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED && millis() - t0 < NTP_TIMEOUT_MS) delay(100);
    if (sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) writeChipNow();
    else err = "no time server";
    sntp_stop();
    netOff();
  }
#if DEBUG
  Serial.printf("sync: %s, WiFi mode %d (0 = off)\n", err ? err : "ok", (int)WiFi.getMode());
#endif
  return err;
}

void clockSet(time_t utc) {
  start();
  struct timeval tv = {utc, 0};
  settimeofday(&tv, nullptr);
  writeChipNow();
}

void clockStatus(char *out, size_t len) {
  start();
  uint8_t r[11] = {};
  Wire.beginTransmission(PCF_ADDR);
  Wire.write(0x00);
  bool ok = Wire.endTransmission(false) == 0 && Wire.requestFrom((uint16_t)PCF_ADDR, (size_t)11) == 11;
  for (uint8_t &b : r) b = ok ? Wire.read() : 0;
  snprintf(out, len, "ctrl1=%02x sec=%02x min=%02x hour=%02x day=%02x mon=%02x year=%02x sys=%ld%s", r[0], r[4], r[5],
           r[6], r[7], r[9], r[10], (long)time(nullptr), ok ? "" : " i2c-fail");
}

struct tm clockLocal() {
  time_t t = time(nullptr);
  struct tm now;
  localtime_r(&t, &now);
  return now;
}
