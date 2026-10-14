#pragma once
// Which saved WiFi network to try, and in what order. Plain C++, no Arduino: tools/nettest checks it.

namespace wifi {

constexpr int SLOTS = 5;  // networks a device can keep (slot 0 is the original wifi_ssid/wifi_pass/... set)

// The slots to try, in order: the one that worked last first (the usual case joins at once), then the rest in
// slot order. `has[i]` is true when slot i holds a network; `last` is that slot or -1. Returns how many.
inline int order(const bool has[SLOTS], int last, int out[SLOTS]) {
  int n = 0;
  if (last >= 0 && last < SLOTS && has[last]) out[n++] = last;
  for (int i = 0; i < SLOTS; i++)
    if (has[i] && i != last) out[n++] = i;
  return n;
}

// How long one attempt may take before the next network gets its turn: a WPA2-Enterprise login is slow.
constexpr unsigned ATTEMPT_MS = 12000, ATTEMPT_ENTERPRISE_MS = 20000;

}  // namespace wifi
