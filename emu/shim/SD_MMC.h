// The "SD card" is a folder in the browser's memory (/sd), so storage.cpp runs unchanged.
#pragma once
#include <FS.h>
class SDMMCFS : public fs::FS {
 public:
  void setPins(int, int, int) {}
  bool begin(const char * = "/sdcard", bool = true, bool = false, int = 0, int = 0) { return true; }
  void end() {}
  uint8_t cardType() { return 2; }  // CARD_SD
  uint64_t cardSize() { return 4ull << 30; }
  uint64_t usedBytes() { return 1ull << 20; }
  uint64_t totalBytes() { return 4ull << 30; }
};
extern SDMMCFS SD_MMC;
#define CARD_NONE 0
