#pragma once
#include <Arduino.h>

// Notes' online work in the background: save the recording, connect, transcribe, tidy up, push. The Notes
// screen returns as soon as B is let go, and the device can be used (or another note recorded) meanwhile.
// The work runs in its own task; it never draws: the screen reads jobStatus() and redraws when gen changes.

enum class JobStep : uint8_t { Idle, Saving, Connecting, Transcribing, Tidying, Pushing };

struct JobStatus {
  JobStep step;
  uint32_t gen;       // goes up whenever anything here changes
  char result[48];    // the last run's outcome for the main screen ("saved: Title"), "" while running
  char noteId[24];    // the note being worked on now
};

bool jobBusy();
bool jobCanTake();  // a new recording can start (its buffer can be handed over when it ends)
// Hands over a finished recording (the ps_malloc'd buffer becomes the job's) and returns at once.
bool jobStartNote(const String &id, int16_t *samples, size_t count, bool onCard);
bool jobStartSweep();  // transcribes waiting recordings and pushes notes GitHub doesn't have yet
JobStatus jobStatus();
size_t jobStackLeft();  // bytes of the job's stack never used so far (0 before it first runs)
// No card and no GitHub: the finished note is shown once instead of kept. True (and filled) if one is waiting.
bool jobTakeShown(String &markdown, String &title);
