#pragma once
// The Pet's friends: other Pets it has met and both owners said yes to. Kept in NVS as one block (pet.cpp /
// meet.cpp save it with storagePutBytes). No Arduino code, so it can be checked on a computer (tools/meettest).
#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace friends {

static const int MAX = 16;
static const uint8_t VERSION = 1;  // bumped if Friend changes: an older block is then read as no friends

struct Friend {
  uint32_t id, look;
  char name[13];
  uint16_t met;      // times met
  uint32_t lastMet;  // unix time, 0 if the clock wasn't set
};

struct List {
  uint8_t version = VERSION;
  uint8_t count = 0;
  Friend f[MAX];

  int find(uint32_t id) const {
    for (int i = 0; i < count; i++)
      if (f[i].id == id) return i;
    return -1;
  }

  // A new friend, or a friend met again: its look and name brought up to date, met counted, the time noted.
  // With MAX friends already, the one met longest ago makes room. Returns its place.
  int meet(uint32_t id, uint32_t look, const char *name, uint32_t now) {
    int i = find(id);
    if (i < 0) {
      if (count < MAX) {
        i = count++;
      } else {
        i = 0;
        for (int j = 1; j < count; j++)
          if (f[j].lastMet < f[i].lastMet) i = j;
      }
      f[i] = {};
      f[i].id = id;
    }
    f[i].look = look;
    strncpy(f[i].name, name, sizeof f[i].name - 1);
    f[i].name[sizeof f[i].name - 1] = 0;
    if (f[i].met < 0xFFFF) f[i].met++;
    f[i].lastMet = now;
    return i;
  }

  void remove(int i) {
    if (i < 0 || i >= count) return;
    memmove(&f[i], &f[i + 1], (count - i - 1) * sizeof(Friend));
    count--;
  }

  // From a saved block: false (and empty) if it isn't one this version wrote.
  bool load(const void *data, size_t len) {
    count = 0;
    if (len != sizeof(List) || ((const List *)data)->version != VERSION) return false;
    memcpy(this, data, len);
    if (count > MAX) count = 0;
    for (int i = 0; i < count; i++) f[i].name[sizeof f[i].name - 1] = 0;
    return true;
  }
};

}  // namespace friends
