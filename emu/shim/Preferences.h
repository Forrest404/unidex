// NVS as a map in memory (a browser visit starts fresh, as a new device does).
#pragma once
#include <Arduino.h>
#include <map>
#include <vector>
class Preferences {
 public:
  bool begin(const char *, bool = false) { return true; }
  void end() {}
  bool clear();
  bool remove(const char *k);
  int32_t getInt(const char *k, int32_t d = 0);
  size_t putInt(const char *k, int32_t v);
  uint32_t getUInt(const char *k, uint32_t d = 0) { return (uint32_t)getInt(k, (int32_t)d); }
  size_t putUInt(const char *k, uint32_t v) { return putInt(k, (int32_t)v); }
  String getString(const char *k, const String &d = String());
  size_t putString(const char *k, const char *v);
  size_t putString(const char *k, const String &v) { return putString(k, v.c_str()); }
  size_t getBytes(const char *k, void *out, size_t len);
  size_t putBytes(const char *k, const void *v, size_t len);
  size_t getBytesLength(const char *k);
  bool isKey(const char *k);
};
