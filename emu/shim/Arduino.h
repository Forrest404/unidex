// Stand-in for the Arduino core, just enough for the browser build of unidex (see emu/README.md).
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>
#include <string>
#include <algorithm>

#define PROGMEM
#define RTC_DATA_ATTR
#define IRAM_ATTR
class __FlashStringHelper;
#define F(x) (reinterpret_cast<const __FlashStringHelper *>(x))
#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#define pgm_read_byte(a) (*(const uint8_t *)(a))
#define pgm_read_word(a) (*(const uint16_t *)(a))
#define pgm_read_dword(a) (*(const uint32_t *)(a))
#define pgm_read_pointer(a) (*(void *const *)(a))

using std::max;
using std::min;

inline size_t strlcpy(char *dst, const char *src, size_t n) {
  const size_t len = strlen(src);
  if (n) {
    const size_t c = len >= n ? n - 1 : len;
    memcpy(dst, src, c);
    dst[c] = 0;
  }
  return len;
}

uint32_t millis();
uint32_t micros();
void delay(uint32_t ms);
inline void delayMicroseconds(uint32_t) {}
inline void yield() {}
inline void pinMode(int, int) {}
inline void digitalWrite(int, int) {}
int digitalRead(int pin);
inline int analogRead(int) { return 0; }
inline long random(long n) { return n > 0 ? rand() % n : 0; }
inline long random(long a, long b) { return a + random(b - a); }
inline void randomSeed(unsigned long s) { srand((unsigned)s); }
inline void setCpuFrequencyMhz(int) {}
#define radians(d) ((d) * 0.017453292519943295)
#define degrees(r) ((r) * 57.29577951308232)
#define constrain(x, lo, hi) ((x) < (lo) ? (lo) : (x) > (hi) ? (hi) : (x))
#include "esp_random.h"
struct HWCDC { static bool isPlugged() { return true; } };
inline void *ps_malloc(size_t n) { return malloc(n); }

class String;
class Print {
 public:
  virtual ~Print() {}
  virtual size_t write(uint8_t c) = 0;
  virtual size_t write(const uint8_t *b, size_t n) {
    size_t i = 0;
    while (i < n && write(b[i])) i++;
    return i;
  }
  size_t write(const char *s) { return write((const uint8_t *)s, strlen(s)); }
  size_t print(const String &s);
  size_t print(const char *s) { return write(s); }
  size_t print(const __FlashStringHelper *s) { return write((const char *)s); }
  size_t println(const __FlashStringHelper *s) { return print(s) + write((uint8_t)'\n'); }
  size_t print(char c) { return write((uint8_t)c); }
  size_t print(int v, int base = 10) { return printNum((long)v, base); }
  size_t print(unsigned v, int base = 10) { return printNum((unsigned long)v, base); }
  size_t print(long v, int base = 10) { return printNum(v, base); }
  size_t print(unsigned long v, int base = 10) { return printNum(v, base); }
  size_t print(double v, int digits = 2) {
    char b[40];
    snprintf(b, sizeof b, "%.*f", digits, v);
    return print(b);
  }
  template <class T> size_t println(const T &v) { return print(v) + write((uint8_t)'\n'); }
  size_t println() { return write((uint8_t)'\n'); }
  size_t printf(const char *fmt, ...) __attribute__((format(printf, 2, 3))) {
    char b[512];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    if (n < 0) return 0;
    if (n >= (int)sizeof b) n = sizeof b - 1;
    return write((const uint8_t *)b, n);
  }
  virtual void flush() {}

 private:
  size_t printNum(unsigned long v, int base) {
    char b[34];
    snprintf(b, sizeof b, base == 16 ? "%lx" : "%lu", v);
    return print(b);
  }
  size_t printNum(long v, int base) {
    char b[34];
    snprintf(b, sizeof b, base == 16 ? "%lx" : "%ld", v);
    return print(b);
  }
};

// Arduino's String over std::string, with the methods unidex uses.
class String {
 public:
  String() {}
  String(const char *s) : s_(s ? s : "") {}
  String(const __FlashStringHelper *s) : s_((const char *)s) {}
  String(const String &o) = default;
  String(String &&o) = default;
  String(char c) : s_(1, c) {}
  String(const std::string &s) : s_(s) {}
  String(int v) : s_(std::to_string(v)) {}
  String(unsigned v) : s_(std::to_string(v)) {}
  String(long v) : s_(std::to_string(v)) {}
  String(unsigned long v) : s_(std::to_string(v)) {}
  String(long long v) : s_(std::to_string(v)) {}
  String(unsigned long long v) : s_(std::to_string(v)) {}
  String(float v, int d = 2) { char b[40]; snprintf(b, sizeof b, "%.*f", d, v); s_ = b; }
  String(double v, int d = 2) { char b[40]; snprintf(b, sizeof b, "%.*f", d, v); s_ = b; }
  String &operator=(const String &) = default;
  String &operator=(String &&) = default;
  String &operator=(const char *s) { s_ = s ? s : ""; return *this; }
  const char *c_str() const { return s_.c_str(); }
  size_t length() const { return s_.size(); }
  bool isEmpty() const { return s_.empty(); }
  bool reserve(size_t n) { s_.reserve(n); return true; }
  char operator[](size_t i) const { return i < s_.size() ? s_[i] : 0; }
  char &operator[](size_t i) { return s_[i]; }
  char charAt(size_t i) const { return (*this)[i]; }
  explicit operator bool() const { return !s_.empty(); }
  String &operator+=(const String &o) { s_ += o.s_; return *this; }
  String &operator+=(const char *o) { s_ += o; return *this; }
  String &operator+=(char c) { s_ += c; return *this; }
  bool concat(const char *b, size_t n) { s_.append(b, n); return true; }
  bool concat(const String &o) { s_ += o.s_; return true; }
  bool concat(const char *o) { s_ += o; return true; }
  bool concat(char c) { s_ += c; return true; }
  bool operator==(const String &o) const { return s_ == o.s_; }
  bool operator==(const char *o) const { return s_ == o; }
  bool operator!=(const String &o) const { return s_ != o.s_; }
  bool operator!=(const char *o) const { return s_ != o; }
  bool operator<(const String &o) const { return s_ < o.s_; }
  bool operator>(const String &o) const { return s_ > o.s_; }
  bool equals(const char *o) const { return s_ == o; }
  bool equalsIgnoreCase(const String &o) const {
    if (o.s_.size() != s_.size()) return false;
    for (size_t i = 0; i < s_.size(); i++)
      if (tolower((unsigned char)s_[i]) != tolower((unsigned char)o.s_[i])) return false;
    return true;
  }
  int compareTo(const String &o) const { return s_.compare(o.s_); }
  bool startsWith(const String &p) const { return s_.compare(0, p.s_.size(), p.s_) == 0 && s_.size() >= p.s_.size(); }
  bool endsWith(const String &p) const {
    return s_.size() >= p.s_.size() && s_.compare(s_.size() - p.s_.size(), p.s_.size(), p.s_) == 0;
  }
  int indexOf(char c, size_t from = 0) const { size_t i = s_.find(c, from); return i == std::string::npos ? -1 : (int)i; }
  int indexOf(const String &t, size_t from = 0) const { size_t i = s_.find(t.s_, from); return i == std::string::npos ? -1 : (int)i; }
  int lastIndexOf(char c) const { size_t i = s_.rfind(c); return i == std::string::npos ? -1 : (int)i; }
  int lastIndexOf(const String &t) const { size_t i = s_.rfind(t.s_); return i == std::string::npos ? -1 : (int)i; }
  String substring(size_t from) const { return from >= s_.size() ? String() : String(s_.substr(from)); }
  String substring(size_t from, size_t to) const {
    if (to < from) std::swap(from, to);
    if (from >= s_.size()) return String();
    return String(s_.substr(from, to - from));
  }
  void remove(size_t i) { if (i < s_.size()) s_.erase(i); }
  void remove(size_t i, size_t n) { if (i < s_.size()) s_.erase(i, n); }
  void replace(const String &a, const String &b) {
    if (a.s_.empty()) return;
    size_t p = 0;
    while ((p = s_.find(a.s_, p)) != std::string::npos) { s_.replace(p, a.s_.size(), b.s_); p += b.s_.size(); }
  }
  void replace(char a, char b) { for (char &c : s_) if (c == a) c = b; }
  void trim() {
    size_t a = 0, b = s_.size();
    while (a < b && isspace((unsigned char)s_[a])) a++;
    while (b > a && isspace((unsigned char)s_[b - 1])) b--;
    s_ = s_.substr(a, b - a);
  }
  void toLowerCase() { for (char &c : s_) c = tolower((unsigned char)c); }
  void toUpperCase() { for (char &c : s_) c = toupper((unsigned char)c); }
  long toInt() const { return atol(s_.c_str()); }
  float toFloat() const { return (float)atof(s_.c_str()); }
  void getBytes(unsigned char *b, size_t n) const { size_t c = std::min(n ? n - 1 : 0, s_.size()); if (n) { memcpy(b, s_.data(), c); b[c] = 0; } }
  const char *begin() const { return s_.c_str(); }
  const char *end() const { return s_.c_str() + s_.size(); }
  const std::string &std() const { return s_; }
  friend String operator+(const String &a, const String &b) { return String(a.s_ + b.s_); }
  friend String operator+(const String &a, const char *b) { return String(a.s_ + b); }
  friend String operator+(const char *a, const String &b) { return String(std::string(a) + b.s_); }
  friend String operator+(const String &a, char b) { return String(a.s_ + b); }

 private:
  std::string s_;
};

inline size_t Print::print(const String &s) { return write((const uint8_t *)s.c_str(), s.length()); }

// Serial is a byte queue: the page writes commands in, and reads replies out (emu/hw/serial_emu.cpp).
class SerialPort : public Print {
 public:
  void begin(unsigned long) {}
  void setRxBufferSize(size_t) {}
  int available();
  int read();
  size_t write(uint8_t c) override;
  size_t write(const uint8_t *b, size_t n) override;
  using Print::write;
  void flush() override {}
  operator bool() const { return true; }
};
extern SerialPort Serial;

// FreeRTOS: no second task in the browser, so a task that would be created simply isn't started.
typedef void (*TaskFunction_t)(void *);
typedef void *TaskHandle_t;
#define pdMS_TO_TICKS(ms) (ms)
inline void vTaskDelay(uint32_t ms) { delay(ms); }
inline int xTaskCreatePinnedToCore(TaskFunction_t, const char *, uint32_t, void *, int, TaskHandle_t *, int) { return 1; }
void esp_restart();

struct EspClass {
  void restart();
  uint32_t getFreeHeap() { return 200000; }
};
extern EspClass ESP;
