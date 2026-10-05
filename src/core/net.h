#pragma once
#include <Arduino.h>
#include <FS.h>

// WiFi with the network saved from the website (credentials.h), and a small HTTPS client that checks
// certificates against the embedded root CA bundle. Turn WiFi off again as soon as the work is done.
const char *netConnect();  // nullptr when connected, else a short reason for the screen
const char *netBegin();    // starts connecting and returns at once (netConnect() then waits for it)
void netOff();
void netClaim(bool on);  // background work (a note sending) owns the WiFi: others must not turn it off
bool netClaimed();

// One piece of a request body: text, a buffer, or the rest of an open file.
struct NetPart {
  const uint8_t *data;
  size_t len;
  fs::File *file;  // used when data is null
  static NetPart text(const char *s) { return {(const uint8_t *)s, strlen(s), nullptr}; }
  static NetPart text(const String &s) { return {(const uint8_t *)s.c_str(), s.length(), nullptr}; }
  static NetPart bytes(const void *p, size_t n) { return {(const uint8_t *)p, n, nullptr}; }
  static NetPart rest(fs::File &f) { return {nullptr, (size_t)(f.size() - f.position()), &f}; }
};

// Sends `method path` to https://host with `headers` (each line ending in \r\n; Host, Content-Length and
// Connection are added) and the body parts. Returns the HTTP status, or -1 if the connection failed.
// The response body (de-chunked) goes in `body`, cut at `maxBody` bytes.
int netHttps(const char *host, const char *method, const char *path, const String &headers,
             const NetPart *parts, int nParts, String &body, size_t maxBody = 65536);
