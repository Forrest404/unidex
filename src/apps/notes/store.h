#pragma once
#include <Arduino.h>
#include <vector>

// Notes on the SD card, one set of files per note in /notes, named by its id (YYYYMMDD-HHMMSS):
//   <id>.wav   the recording (kept)
//   <id>.md    the finished note; a .wav without one is still waiting to be transcribed
//   <id>.gh    the note is on GitHub (holds the path in the repo)
struct NoteInfo {
  String id, title;
  bool text, pushed;  // has its .md / is on GitHub
};

bool storeReady();                                   // the card is readable (mounts it on first use)
String storeNewId();                                 // a fresh id from the clock (or a counter if it isn't set)
bool storeValidId(const String &id);                 // letters, digits and '-' only: safe to put in a path
bool storeSaveWav(const String &id, const int16_t *samples, size_t count);
String storeWavPath(const String &id);
bool storeSaveNote(const String &id, const String &markdown);
String storeReadNote(const String &id);              // "" if missing
void storeMarkPushed(const String &id, const String &path);
void storeDelete(const String &id);
std::vector<NoteInfo> storeList();                   // newest first
String storeTitleOf(const String &markdown);         // the front matter title

// A 44-byte WAV header for 16 kHz 16-bit mono with `dataBytes` of samples.
void storeWavHeader(uint8_t *out, uint32_t dataBytes);
