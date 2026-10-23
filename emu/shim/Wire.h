#pragma once
#include <Arduino.h>

// No I2C bus in the browser: nothing answers (the clock and codec have their own stand-ins in emu/hw/).
class TwoWire {
 public:
  bool begin(int = -1, int = -1, uint32_t = 0) { return true; }
  void beginTransmission(uint16_t) {}
  size_t write(uint8_t) { return 1; }
  uint8_t endTransmission(bool = true) { return 2; }  // 2: no device at that address
  size_t requestFrom(uint16_t, size_t, bool = true) { return 0; }
  int read() { return -1; }
};
static TwoWire Wire;
