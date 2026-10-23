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
// The recording buffer: one block of up to `want` samples, allocated the first time and then kept for good (null if
// it is in use). Freeing and allocating a 5.7 MB block each time failed after the first recording: the audio driver
// leaves small blocks in PSRAM behind it, and no free block that large was left. `got` is its size in samples.
int16_t *jobTakeBuffer(size_t want, size_t &got);
void jobGiveBuffer(int16_t *buffer);  // back from the recording screen, or done with by the job
bool jobCanTake();  // a new recording can start (its buffer can be handed over when it ends)
// Hands over a finished recording (the buffer from jobTakeBuffer becomes the job's, which gives it back) and returns at once.
bool jobStartNote(const String &id, int16_t *samples, size_t count, bool onCard);
// Transcribes waiting recordings and pushes notes GitHub doesn't have yet. quiet: Notes trying by itself, so a try
// that sends nothing leaves no result line.
bool jobStartSweep(bool quiet = false);
// Reminders: transcribes a recording from memory and keeps only the words (nothing is saved). Like jobStartNote,
// the buffer becomes the job's. The words (or why there are none) wait for jobTakeText.
bool jobStartText(int16_t *samples, size_t count);
bool jobTakeText(String &text, String &error);  // true (and filled) once, when a jobStartText has finished
JobStatus jobStatus();
size_t jobStackLeft();  // bytes of the job's stack never used so far (0 before it first runs)
// No card and no GitHub: the finished note is shown once instead of kept. True (and filled) if one is waiting.
bool jobTakeShown(String &markdown, String &title);
