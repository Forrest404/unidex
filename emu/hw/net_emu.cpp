// No WiFi in the browser: everything that needs the network says so.
#include "../../src/core/net.h"
#include "../../src/core/link.h"
#include "../../src/core/audio.h"
static const char *NO_NET = "Not in the demo";
const char *netConnect() { return NO_NET; }
const char *netBegin() { return NO_NET; }
void netOff() {}
void netClaim(bool) {}
bool netClaimed() { return false; }
int netHttps(const char *, const char *, const char *, const String &, const NetPart *, int, String &, size_t) { return -1; }
int netHttpsGet(const char *, const char *, const std::function<bool(const uint8_t *, size_t)> &, String &) { return -1; }
int netHttpsHeader(const char *, const char *, const char *, String &) { return -1; }
const char *linkStart() { return "Not in the demo"; }
void linkStop() {}
bool linkOn() { return false; }
bool linkBroadcast(const uint8_t *, size_t) { return false; }
bool linkSend(const uint8_t[6], const uint8_t *, size_t) { return false; }
bool linkPoll(LinkPacket &) { return false; }
bool audioBegin() { return false; }
size_t audioRead(int16_t *, size_t) { return 0; }
void audioEnd() {}
bool audioChime() { return false; }
bool audioPlayBegin() { return false; }
void audioPlay(const int16_t *, size_t) {}
void audioPlayEnd(bool) {}
static int volume = 8;
int audioVolume() { return volume; }
void audioSetVolume(int v) { volume = v; }
