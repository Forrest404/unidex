#pragma once

// The website's Setup page talks to these over USB (site/serial.js). Called from usbsync for every line
// starting with "N "; returns false if it isn't one of them.
//   N ?                      -> "NS <name> <set|unset> <hex>" per setting (credentials.h), "NC <card 0|1> <notes>
//                               <waiting>", then "OK N ?"
//   N SET <name> <hex>       -> "OK N SET <name>" or "ERR"           (hex of the value; empty = clear)
//   N ADD <wifi_ca|wifi2_ca..wifi5_ca> <hex> -> "OK N ADD <name>" or "ERR"  (a chunk of that network's CA, which
//                               is longer than a line; kept in RAM and saved with the next N SET for the same
//                               name, its last chunk)
//   N CLR <name|all>         -> "OK N CLR"
//   N TEST <what>            -> "OK N TEST <what> ok" or "OK N TEST <what> fail <reason>"  (wifi, openai,
//                               elevenlabs, anthropic, github; takes a few seconds)
//   N MIC [wifi]             -> records 2 s: "OK N MIC <peak> <rms>" (0-32767) or "OK N MIC fail <reason>";
//                               with "wifi", WiFi is connecting meanwhile (a check for radio hum)
//   N PLAY <id>              -> plays the recording on the speaker (blocks until it ends or a button is pressed):
//                               "OK N PLAY" or "OK N PLAY fail <reason>"
//   N LIST                   -> "NF <id> <bytes> <text 0|1> <pushed 0|1> <hex title>" per note, then "OK N LIST"
//   N READ <id>              -> "ND <hex, up to 64 bytes>" lines, then "OK N READ <bytes> <crc32>" (or "ERR")
//   N DEL <id>               -> "OK N DEL" ("ERR busy" for the note being sent)
//   N UNSEND <id>            -> test build: drops the note's text and GitHub mark, so its recording waits again
//   N SYNC                   -> starts sending waiting notes in the background (as B on the Notes screen):
//                               "OK N SYNC started <waiting>", "OK N SYNC none", "OK N SYNC busy",
//                               "OK N SYNC fail <reason>" (no SD card, WiFi not set up); then watch N JOB
//   N JOB                    -> "OK N JOB <busy 0|1> <step> <gen> <hex result> <hex note id> <stack bytes left>"
bool notesUsb(const char *line);
