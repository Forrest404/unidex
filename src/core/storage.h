#pragma once
#include <FS.h>

// All file and NVS access goes through here, so moving files to an SD card later only
// changes storage.cpp. Flash wears out: open a file once, write everything, close it.
// Never write inside a loop.

bool storageInit();  // false if the filesystem image hasn't been uploaded
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
void storageUsage(size_t &used, size_t &total);  // filesystem bytes
