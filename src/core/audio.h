#pragma once
#include <stddef.h>
#include <stdint.h>

// Microphone through the onboard ES8311 codec: 16 kHz, 16-bit mono. And the speaker: recordings and a chime.
// Powered (GPIO42) only between audioBegin() and audioEnd(); the I2C bus is the clock chip's (Wire).
static const int AUDIO_RATE = 16000;

bool audioBegin();                               // power up the codec and start capture; false if it doesn't answer
size_t audioRead(int16_t *out, size_t samples);  // blocks until `samples` mono samples arrive (or ~100 ms pass)
void audioEnd();                                 // stop capture and power the codec down
// Sound out of the speaker, 16 kHz mono like the mic: begin (false while recording or if the codec is silent), play
// (blocks while the DMA buffers are full), end. now: stop at once; else what's queued plays out first.
// A sample is heard AUDIO_PLAY_QUEUE samples after it's played: the buffers start out full of silence.
const size_t AUDIO_PLAY_QUEUE = 8 * 1024;  // 0.5 s: longer than a screen refresh (~0.35 s), so none drops any sound
bool audioPlayBegin();
void audioPlay(const int16_t *mono, size_t samples);
void audioPlayEnd(bool now);
// The speaker's volume, 0 (off) to AUDIO_VOLUME_MAX, saved in NVS (Settings > Volume). Used from the next sound.
static const int AUDIO_VOLUME_MAX = 10;
int audioVolume();
void audioSetVolume(int volume);
// Alerts (a reminder falling due) on screen only, without the chime: Settings > Mute alerts, saved in NVS.
bool audioAlertsMuted();
void audioSetAlertsMuted(bool muted);
bool audioChime();  // two short notes on the speaker (blocks ~0.5 s); false while recording or if the codec is silent
