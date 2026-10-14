#include "appsusb.h"
#include <Arduino.h>
#include <esp_rom_crc.h>
#include "appstore.h"
#include "scripts.h"
#include "../../core/storage.h"

static const uint32_t STALL_MS = 3000;

static String upId, upPath;
static int32_t upLeft;  // bytes still to come
static uint32_t upCrc, upExpected, upBytes, upLastAt;
static fs::File upOut;

static void abortUpload() {
  if (upOut) upOut.close();
  if (upLeft > 0) storageRemove(appsStagePath(upId, upPath.c_str()).c_str());
  upLeft = 0;
}

static void hexLine(const char *text) {
  for (const char *c = text; *c; c++) Serial.printf("%02x", (uint8_t)*c);
}

static int hexValue(char c) {
  return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
}

bool appsUsbData(const char *l) {
  if (upLeft <= 0 || l[0] != 'D' || l[1] != ' ') return false;
  upLastAt = millis();
  const char *hex = l + 2;
  uint8_t buf[64];
  const size_t n = strlen(hex) / 2;
  bool ok = strlen(hex) % 2 == 0 && n > 0 && n <= sizeof buf && (int32_t)n <= upLeft;
  for (size_t i = 0; ok && i < n; i++) {
    const int hi = hexValue(hex[2 * i]), lo = hexValue(hex[2 * i + 1]);
    ok = hi >= 0 && lo >= 0;
    buf[i] = hi << 4 | lo;
  }
  if (!ok || upOut.write(buf, n) != n) {
    abortUpload();
    Serial.println("ERR");
    return true;
  }
  upCrc = esp_rom_crc32_le(upCrc, buf, n);
  upLeft -= n;
  Serial.println("K");
  if (upLeft > 0) return true;
  upOut.close();
  const String path = appsStagePath(upId, upPath.c_str());
  fs::File check = storageOpen(path.c_str());
  const bool whole = check && check.size() == upBytes;  // a full card loses the end
  check.close();
  if (upCrc != upExpected || !whole) {
    storageRemove(path.c_str());
    Serial.println("ERR");
    return true;
  }
  Serial.println("OK A FILE");
  return true;
}

void appsUsbPoll() {
  if (upLeft > 0 && millis() - upLastAt > STALL_MS) abortUpload();
}

static void list() {
  for (const AppEntry &e : appsList(true)) {
    Serial.printf("A %s %s %llu %llu ", e.id.c_str(), e.offloaded ? "-" : e.version.length() ? e.version.c_str() : "?",
                  (unsigned long long)e.code, (unsigned long long)e.data);
    if (e.problem.length()) hexLine(e.problem.c_str());
    else Serial.print("ok");
    Serial.println();
  }
  Serial.println("OK A end");
}

bool appsUsb(const char *l) {
  if (l[0] != 'A' || l[1] != ' ') return false;
  const char *c = l + 2;
  char id[32] = "", path[64] = "", flag[8] = "";
  if (!strcmp(c, "LIST")) {
    list();
  } else if (!strcmp(c, "SPACE")) {
    uint64_t used = 0, total = 0;
    storageUsage(used, total);
    Serial.printf("OK A %u %llu\n", (unsigned)appsList(false).size(), (unsigned long long)(total - used));
  } else if (sscanf(c, "BEGIN %31s", id) == 1) {
    abortUpload();
    if (!apps::idOk(id) || !appsBegin(id)) Serial.println("ERR");
    else Serial.println("OK A BEGIN");
  } else if (!strncmp(c, "PUT ", 4)) {
    long bytes = 0;
    unsigned long crc = 0;
    abortUpload();
    bool ok = sscanf(c + 4, "%31s %63s %ld %lu", id, path, &bytes, &crc) == 4 && apps::idOk(id) &&
              apps::installPathOk(path) && bytes > 0 && (size_t)bytes <= apps::MAX_FILE &&
              appsStaging(id);  // after A BEGIN
    if (ok) upOut = storageOpen(appsStagePath(id, path).c_str(), "w");
    if (!ok || !upOut) {
      Serial.println("ERR");
      return true;
    }
    upId = id;
    upPath = path;
    upLeft = upBytes = bytes;
    upCrc = 0;
    upExpected = crc;
    upLastAt = millis();
    Serial.println("OK A PUT");
  } else if (sscanf(c, "DONE %31s", id) == 1) {
    abortUpload();
    if (scriptsRunning(id)) {
      Serial.print("ERR ");
      hexLine("open on the device");
      Serial.println();
      return true;
    }
    if (const char *why = apps::idOk(id) ? appsCommit(id) : "bad id") {
      Serial.print("ERR ");
      hexLine(why);
      Serial.println();
    } else {
      scriptsListChanged();
      Serial.printf("OK A DONE %s\n", id);
    }
  } else if (sscanf(c, "DATA %31s", id) == 1) {
    if (!apps::idOk(id)) Serial.println("ERR");
    else Serial.printf("OK A DATA %llu\n", (unsigned long long)storageTreeBytes((appsDir(id) + "/data").c_str()));
  } else if (sscanf(c, "RM %31s %7s", id, flag) >= 1) {
    if (!apps::idOk(id) || (flag[0] && strcmp(flag, "keep"))) Serial.println("ERR");
    else if (scriptsRunning(id)) Serial.println("ERR busy");
    else if (!appsRemove(id, flag[0] != 0)) Serial.println("ERR");
    else {
      scriptsListChanged();
      Serial.printf("OK A RM %s\n", id);
    }
  } else {
    Serial.println("ERR");
  }
  return true;
}
