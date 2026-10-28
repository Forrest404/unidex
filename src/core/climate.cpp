// The SHTC3 temperature and humidity sensor, on the Wire bus the clock chip already uses (clockBegin starts it).
#include "climate.h"

#include <Arduino.h>
#include <Wire.h>

static const uint8_t SHTC3_ADDR = 0x70;
static const uint16_t CMD_WAKEUP = 0x3517;
static const uint16_t CMD_MEASURE = 0x7866;  // normal mode, temperature first, no clock stretching
static const uint16_t CMD_SLEEP = 0xB098;

static bool command(uint16_t c) {
  Wire.beginTransmission(SHTC3_ADDR);
  Wire.write((uint8_t)(c >> 8));
  Wire.write((uint8_t)(c & 0xFF));
  return Wire.endTransmission() == 0;
}

// CRC-8, polynomial 0x31, starting at 0xFF (the datasheet's).
static uint8_t crc8(const uint8_t *data, size_t length) {
  uint8_t crc = 0xFF;
  for (size_t i = 0; i < length; i++) {
    crc ^= data[i];
    for (int bit = 0; bit < 8; bit++) crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
  }
  return crc;
}

bool climateRead(ClimateReading &out) {
  if (!command(CMD_WAKEUP)) return false;
  delay(1);  // it wakes in 240 us at most
  uint8_t d[6];
  bool ok = command(CMD_MEASURE);
  if (ok) {
    delay(20);  // a measurement takes 12.1 ms at most
    ok = Wire.requestFrom((uint16_t)SHTC3_ADDR, (size_t)6) == 6;
  }
  if (ok) {
    for (uint8_t &b : d) b = (uint8_t)Wire.read();
    ok = crc8(d, 2) == d[2] && crc8(d + 3, 2) == d[5];
  }
  command(CMD_SLEEP);
  if (!ok) return false;
  out.temperatureC = -45.0f + 175.0f * (uint16_t)(d[0] << 8 | d[1]) / 65535.0f;
  out.humidityRH = 100.0f * (uint16_t)(d[3] << 8 | d[4]) / 65535.0f;
  return true;
}
