#pragma once
#include <Arduino.h>
#include <vector>

// Reminders said but not yet in the list, on the SD card in /reminders, named by when they were said:
//   <id>.wav   the recording, kept until it's turned into words (it waits there while there's no WiFi)
//   <id>.txt   the words, for the app to add to the list (then removed)
// The id is YYYYMMDD-HHMMSS in local time, so "tomorrow" counts from when it was said, not from when it was sent.
// Without the clock set it's "x" and a random number (no time can be worked out then anyway).
String waitingNewId();
bool waitingSave(const String &id, const int16_t *samples, size_t count);  // the recording, as a WAV
String waitingWavPath(const String &id);
std::vector<String> waitingRecordings();  // ids with a .wav still to send, oldest first
bool waitingSetWords(const String &id, const String &words);  // the .txt, then the .wav goes
std::vector<String> waitingWords();       // ids with a .txt to add, oldest first
String waitingReadWords(const String &id);
void waitingRemoveWords(const String &id);
long waitingSaidAt(const String &id);  // when it was said, in local minutes from 1970 (-1: unknown)
