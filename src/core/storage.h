#pragma once
#include <FS.h>

// All file and NVS access goes through here. Files live on the micro SD card (FAT32), mounted on
// first use; with no readable card, opens fail and return an empty File. Open a file once, write
// everything, close it. Never write inside a loop.

fs::File storageOpen(const char *path, const char *mode = "r");  // "r", "w", "a"; also opens folders
bool storageExists(const char *path);
bool storageRename(const char *from, const char *to);  // replaces `to`: write a temp file, then rename
bool storageRemove(const char *path);

// Small persistent values in NVS. Puts skip the write if the value is unchanged.
int32_t storageGetInt(const char *key, int32_t fallback = 0);
void storagePutInt(const char *key, int32_t value);
String storageGetString(const char *key, const char *fallback = "");
void storagePutString(const char *key, const char *value);
void storageRemoveKey(const char *key);
void storageClearKeys();  // every NVS value (settings, tallies, salt...)
bool storageUsage(uint64_t &used, uint64_t &total);  // card bytes; false with no readable card

// Micro SD card (SD_MMC, 1-bit: CLK 39, CMD 41, D0 40). Mounted on first use, unmounted before deep sleep.
bool storageCardMount();     // true if a readable (FAT) card is mounted
void storageEnd();           // unmount; call before deep sleep
void storageCardTest();      // mount, then print the card's state and files to serial (USB command S)
