#pragma once
#include <Arduino.h>

// Hold-to-talk, shared by Notes and Reminders: records while B is held, with the time and a waveform on the
// screen, into the job's buffer (job.h). Returns nullptr with `samples` and `count` filled (the buffer is then the
// caller's to hand to the job, or give back with jobGiveBuffer), or a short reason for the screen.
// `seconds` stops it sooner than the buffer would (a reminder is a sentence, not a lecture).
const char *recordWhileHeld(uint32_t seconds, int16_t *&samples, size_t &count);

// The display fonts are ASCII: folds the usual UTF-8 punctuation from transcripts and drops accents (Latin-1 and
// Latin Extended-A: French, Spanish, German, Polish, Czech, Turkish...). Anything else becomes '?'; `missing`, if
// given, counts those, so a caller can tell a note in another script from a few odd characters.
String asciiText(const String &s, int *missing = nullptr);
