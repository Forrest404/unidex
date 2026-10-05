#pragma once

// The website's Notes page talks to these over USB (site/serial.js). Called from usbsync for every line
// starting with "N "; returns false if it isn't one of them.
//   N ?                      -> "NS <name> <set|unset> <hex>" per setting (credentials.h), "NC <card 0|1> <notes>
//                               <waiting>", then "OK N ?"
//   N SET <name> <hex>       -> "OK N SET <name>" or "ERR"           (hex of the value; empty = clear)
//   N ADD wifi_ca <hex>      -> "OK N ADD wifi_ca" or "ERR"           (a chunk of the CA, which is longer than
//                               a line; kept in RAM and saved with the next N SET wifi_ca, its last chunk)
//   N CLR <name|all>         -> "OK N CLR"
//   N TEST <what>            -> "OK N TEST <what> ok" or "OK N TEST <what> fail <reason>"  (wifi, openai,
//                               anthropic, github; takes a few seconds)
//   N MIC [wifi]             -> records 2 s: "OK N MIC <peak> <rms>" (0-32767) or "OK N MIC fail <reason>";
//                               with "wifi", WiFi is connecting meanwhile (a check for radio hum)
//   N LIST                   -> "NF <id> <bytes> <text 0|1> <pushed 0|1> <hex title>" per note, then "OK N LIST"
//   N READ <id>              -> "ND <hex, up to 64 bytes>" lines, then "OK N READ <bytes> <crc32>" (or "ERR")
//   N DEL <id>               -> "OK N DEL" ("ERR busy" for the note being sent)
//   N JOB                    -> "OK N JOB <busy 0|1> <step> <gen> <hex result> <hex note id> <stack bytes left>"
bool notesUsb(const char *line);
