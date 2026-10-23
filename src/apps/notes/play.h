#pragma once
#include <Arduino.h>

// Plays a note's recording (<id>.wav on the card) on the speaker, with the time and a bar on the screen, until it
// ends or any button is pressed. Returns nullptr, or a short reason for a toast.
const char *playNote(const String &id);
