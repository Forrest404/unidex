#pragma once
#include <stddef.h>
#include <stdint.h>

// Microphone through the onboard ES8311 codec: 16 kHz, 16-bit mono.
// Powered (GPIO42) only between audioBegin() and audioEnd(); the I2C bus is the clock chip's (Wire).
static const int AUDIO_RATE = 16000;

bool audioBegin();                               // power up the codec and start capture; false if it doesn't answer
size_t audioRead(int16_t *out, size_t samples);  // blocks until `samples` mono samples arrive (or ~100 ms pass)
void audioEnd();                                 // stop capture and power the codec down
