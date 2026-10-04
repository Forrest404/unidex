#pragma once

// Test build only (pio run -e dev): USB commands that let tools/devshot.py drive the device and read the
// screen. In the normal build the switches below are always off and devUsb() doesn't exist.
//   X SHOT            -> "XS 200 200 <refresh kind> <ms> <screen name hex>", 50 "XD <hex>" lines (4 rows each,
//                        1 = black), then "OK X SHOT <crc32 of the 5000 bytes>"
//   X BTN <a|A|b|B|R> -> presses A short, A long, B short, B long, both (restart); replies after the refresh:
//                        "OK X BTN <refresh kind> <ms>"
//   X HOLD B <ms>     -> holds B for <ms> (hold-to-talk), same reply
//   X STATE           -> "OK X STATE screen=<hex> sel=<hex> clock=<0|1> dry=<0|1> fake=<0|1> netfail=<0|1>"
//   X MEM             -> "OK X MEM internal=<free> block=<largest> psram=<free>"
//   X CLOCK UNSET     -> system time to 0 in RAM only (the clock chip keeps its time); "T <unix>" sets it back
//   X DRY|FAKE|NETFAIL|NOCARD <0|1>  -> destructive actions only pretend / cloud steps are canned / WiFi fails /
//                        the SD card reads as missing
#if UNIDEX_DEV
bool devUsb(const char *line);  // true if the line was an X command
bool devDryRun();
bool devFakeCloud();
bool devNetFail();
bool devNoCard();
#else
inline bool devDryRun() { return false; }
inline bool devFakeCloud() { return false; }
inline bool devNetFail() { return false; }
inline bool devNoCard() { return false; }
#endif
