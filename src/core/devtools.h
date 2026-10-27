#pragma once
#include <Arduino.h>
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
//   X I2C             -> "OK X I2C sda=<0|1> scl=<0|1> <addresses that answer, hex>" on the clock chip's bus
//                        (0x18 codec, 0x51 clock, 0x70 SHTC3); both lines 0 = the bus has no power
//   X BEEP            -> the reminder chime on the speaker; "OK X BEEP" (or "ERR codec" if it didn't play)
//   X BATT            -> "OK X BATT mv=<battery mV> pct=<% shown> usb=<0|1> up=<s> kept=<0|1>" (kept: the % carried
//                        on from before the last restart)
//   X CLOCK UNSET     -> system time to 0 in RAM only (the clock chip keeps its time); "T <unix>" sets it back
//   X SEED <n>        -> games start from random seed n (0 = truly random); "OK X SEED"
//   X FRAMES <n>      -> the game being played runs n frames at once, then shows the result; "OK X FRAMES"
//   X PLAY <0/1...>   -> like X FRAMES, one frame per character with B held (1) or not (0); "OK X PLAY"
//   X MANUAL <0|1>    -> games only move on with X FRAMES (not with time), for repeatable screenshots
//   X SLEEP <ms>      -> "OK X SLEEP", then real deep sleep woken by a timer (RAM is lost, as in use)
//   X DRY|FAKE|NETFAIL|NOCARD|NOPUSH <0|1>  -> destructive actions only pretend / cloud steps are canned /
//                        WiFi fails / the SD card reads as missing / real transcription but nothing sent to GitHub
//   X SWEEPONLY <prefix>  -> with FAKE on, a sync (sweep) handles only notes whose id starts with prefix (none: off)
//   X FAILNOTE <n>    -> with FAKE on, the next n canned transcriptions fail with "Whisper: error 400"
//   X NOSAVE <0|1>    -> Notes can't write a note's text or its "on GitHub" mark (a card that filled up after
//                        the recording was saved)
//   X EMPTY <0|1>     -> with FAKE on, the canned transcription hears nothing (the "Heard nothing" note)
//   X AUTOSYNC <s>    -> Notes tries sending waiting recordings by itself every s seconds (0 = the usual 10 min)
//   X WELCOME         -> the first-start welcome screens (finishing them saves "welcomed" = 1)
//   X FLASHFILE <path> <bytes> -> a file in internal flash (made-up bytes; 0 removes it), to test the one-time copy
//                        to the card (X KEY sd_copied none, then restart)
//   X ADDLINE <path> <text> -> adds a line to a card file (test data, e.g. a timetable.csv)
//   X RMFILE <path>   -> removes one file from the card (cleaning up after a test)
//   X NOTES <n>       -> n made-up notes on the card (test-0001 ...), for long lists; X NOTES 0 removes them
//   X CALSYNC         -> what Settings > Sync does: "OK X CALSYNC clock=<ok|reason> link=<0|1> calendar=<ok|reason>
//                        events=<n>"
//   X FETCH [<id>]    -> over WiFi, the store's list to the card, or the app <id> installed from it (as the phone
//                        portal does): "OK X FETCH <ok|reason>"
//   X HANG            -> the main loop stops for good: the loop watchdog restarts the device after 30 s
//   X BATTMV <mv>     -> pretend the battery reads mv, as if on battery (0 = the real reading): low-battery tests
//   X KEY <name> [<int>|none] -> reads (or sets, or removes) one saved number in NVS:
//                        "OK X KEY <name> <value|none>"; walkthroughs use it to leave a setting as they found it
// Pet > Meet also prints "XW <step>" as each step of a walk between two devices shows (tools/meetshot.py).
//   X DEMO <0|1>      -> Timetable and Notes show built-in sample classes and notes instead of the card's (for
//                        screenshots without personal data); nothing on the card changes
#if UNIDEX_DEV
bool devUsb(const char *line);  // true if the line was an X command
bool devDryRun();
bool devFakeCloud();
bool devNetFail();
bool devNoCard();
bool devNoPush();
bool devNoSave();
bool devEmptyHeard();
bool devFailNote();                       // consumes one of X FAILNOTE's failures
bool devSweepOn();                        // X SWEEPONLY is set
bool devSweepAllows(const String &id);    // a sync may handle this note
bool devDemo();
uint32_t devAutoSync();  // X AUTOSYNC's seconds, 0 when not set
uint32_t devSeed();
bool devManualFrames();
// What X FRAMES and X PLAY call (the Games app sets it). held: B per frame ('1' down), or null for the real B.
void devSetFrameStepper(void (*fn)(int frames, const char *held));
void devSetDetail(const char *(*fn)());  // extra state for X STATE ("detail=..."), set by the open app
// During a long animation that blocks the loop: answers X SHOT and X STATE (anything else gets "ERR busy"), so a
// test can see frames mid-animation. Call between frames.
void devShotDuringAnimation();
void devTick();  // every loop: the battery log (X BATTLOG)
#else
inline bool devDryRun() { return false; }
inline bool devFakeCloud() { return false; }
inline bool devNetFail() { return false; }
inline bool devEmptyHeard() { return false; }
inline bool devFailNote() { return false; }
inline bool devSweepOn() { return false; }
inline bool devSweepAllows(const String &) { return true; }
inline bool devNoCard() { return false; }
inline bool devNoPush() { return false; }
inline bool devNoSave() { return false; }
inline bool devDemo() { return false; }
inline uint32_t devAutoSync() { return 0; }
inline uint32_t devSeed() { return 0; }
inline bool devManualFrames() { return false; }
#endif
