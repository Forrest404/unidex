#pragma once
#include <stdint.h>

// Test build only (pio run -e dev): USB commands that let tools/devshot.py drive the device and read the
// screen. In the normal build the switches below are always off and devUsb() doesn't exist.
//   X SHOT            -> "XS 200 200 <refresh kind> <ms> <screen name hex>", 50 "XD <hex>" lines (4 rows each,
//                        1 = black), then "OK X SHOT <crc32 of the 5000 bytes>"
//   X BTN <a|A|b|B|R> -> presses A short, A long, B short, B long, both (restart); replies after the refresh:
//                        "OK X BTN <refresh kind> <ms>"
//   X HOLD B <ms>     -> holds B for <ms> (hold-to-talk), same reply
//   X STATE           -> "OK X STATE screen=<hex> sel=<hex> clock=<0|1> ... detail=<hex>" (detail: from the app)
//   X MEM             -> "OK X MEM internal=<free> block=<largest> psram=<free>"
//   X CLOCK UNSET     -> system time to 0 in RAM only (the clock chip keeps its time); "T <unix>" sets it back
//   X SEED <n>        -> games start from random seed n (0 = truly random); "OK X SEED"
//   X FRAMES <n>      -> the game being played runs n frames at once, then shows the result; "OK X FRAMES"
//   X PLAY <0/1...>   -> like X FRAMES, one frame per character with B held (1) or not (0); "OK X PLAY"
//   X MANUAL <0|1>    -> games only move on with X FRAMES (not with time), for repeatable screenshots
//   X SLEEP <ms>      -> "OK X SLEEP", then real deep sleep woken by a timer (RAM is lost, as in use)
//   X DRY|FAKE|NETFAIL|NOCARD|NOPUSH <0|1>  -> destructive actions only pretend / cloud steps are canned /
//                        WiFi fails / the SD card reads as missing / real transcription but nothing sent to GitHub
#if UNIDEX_DEV
bool devUsb(const char *line);  // true if the line was an X command
bool devDryRun();
bool devFakeCloud();
bool devNetFail();
bool devNoCard();
bool devNoPush();
uint32_t devSeed();
bool devManualFrames();
// What X FRAMES and X PLAY call (the Games app sets it). held: B per frame ('1' down), or null for the real B.
void devSetFrameStepper(void (*fn)(int frames, const char *held));
void devSetDetail(const char *(*fn)());  // extra state for X STATE ("detail=..."), set by the open app
#else
inline bool devDryRun() { return false; }
inline bool devFakeCloud() { return false; }
inline bool devNetFail() { return false; }
inline bool devNoCard() { return false; }
inline bool devNoPush() { return false; }
inline uint32_t devSeed() { return 0; }
inline bool devManualFrames() { return false; }
#endif
