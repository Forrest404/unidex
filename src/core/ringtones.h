#pragma once
// The chime's tunes (Settings > Ringtone): what alarms, the timer, reminders and apps' unidex.chime() play. Each is
// a few notes, short enough to repeat every few seconds while an alarm rings (it blocks while it plays). No Arduino
// here, so tools/ringtonetest checks them on a computer.
#include <stdint.h>

namespace ringtones {

struct Note {
  uint16_t hz, ms;  // hz 0: a rest
};

struct Tune {
  const char *name;
  bool struck;      // each note fades away like a bell or a marimba, instead of holding steady
  Note notes[8];    // ends at the first {0, 0}
};

const Tune TUNES[] = {
    {"Chime", false, {{880, 140}, {1319, 220}}},                                    // the two notes it always had
    {"Bell", true, {{1568, 700}}},                                                  // one bell, ringing out
    {"Rise", false, {{784, 110}, {988, 110}, {1175, 110}, {1568, 260}}},            // up the chord
    {"Beeps", false, {{2000, 90}, {0, 70}, {2000, 90}, {0, 70}, {2000, 90}}},       // the classic alarm
    {"Ripple", true, {{1319, 120}, {1047, 120}, {1319, 120}, {1568, 380}}},         // a marimba run
};
const int COUNT = sizeof(TUNES) / sizeof(TUNES[0]);
const int MAX_MS = 1200;

inline int lengthMs(const Tune &t) {
  int ms = 0;
  for (const Note &n : t.notes) {
    if (!n.hz && !n.ms) break;
    ms += n.ms;
  }
  return ms;
}

}  // namespace ringtones
