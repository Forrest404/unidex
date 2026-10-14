#pragma once
#include <stddef.h>

// The website's store page installs, lists and removes apps with these (docs/APPS.md). Called from usbsync for
// every line; returns false if it isn't one of them.
//   A LIST                 -> "A <id> <version|-> <code bytes> <data bytes> <ok|hex problem>" per app ("-": offloaded,
//                             only its data is left), then "OK A end"
//   A SPACE                -> "OK A <apps> <free card bytes>"
//   A BEGIN <id>           -> "OK A BEGIN": starts an install (clears any unfinished one)
//   A PUT <id> <path> <bytes> <crc32> -> "OK A PUT", then "D <hex>" lines (64 bytes at most) each answered "K", and
//                             after the last byte "OK A FILE" or "ERR" (the file is dropped)
//   A DONE <id>            -> "OK A DONE <id>" once the app is checked and in place, or "ERR <hex reason>"
//   A DATA <id>            -> "OK A DATA <bytes>"
//   A RM <id> [keep]       -> "OK A RM <id>" ("keep": offload, its data stays), "ERR busy" while it's open
bool appsUsb(const char *line);
bool appsUsbData(const char *line);  // a "D <hex>" line for an app upload in progress (false: not ours)
void appsUsbPoll();                  // every loop: drops an upload that stalled
