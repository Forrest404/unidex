// No internal flash files in the browser build: begin() fails, so the one-time copy never runs.
#pragma once
#include <FS.h>
class LittleFSFS : public fs::FS {
 public:
  bool begin(bool = false, const char * = "/littlefs", uint8_t = 10, const char * = "spiffs") { return false; }
  void end() {}
  bool format() { return true; }
};
extern LittleFSFS LittleFS;
