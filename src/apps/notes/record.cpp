#include "record.h"
#include "job.h"
#include "../../core/audio.h"
#include "../../core/devtools.h"
#include "../../core/display.h"
#include "../../core/input.h"
#include "../../core/power.h"
#include "../../core/theme.h"

// The buffer is allocated once at this size and kept (job.h), so every recording asks for all of it.
static const uint32_t BUFFER_SECONDS = 180, MIN_MS = 600;  // 3 min = 5.8 MB of PSRAM
static uint32_t maxSeconds;  // this recording's limit
static uint32_t recordMs;

// The waveform: one bar per 1024 samples (64 ms), newest on the right, so the last ~4 s of speech scroll past.
static const int BAR_PITCH = 3, BAR_W = 2, WAVE_CY = 88, WAVE_HALF = 36;  // bars 3 px apart, up to 72 px tall
static const int MAX_BARS = 64;
static uint8_t bars[MAX_BARS];  // 0-255, oldest first
static int barCount;

static void addBar(int peak) {
  const int level = min(255, peak * 5 * 255 / 32767);  // x5: speech rarely gets near full scale
  if (barCount == MAX_BARS) memmove(bars, bars + 1, --barCount);
  bars[barCount++] = level;
}

static void drawRecording() {
  char time[12];
  const uint32_t s = recordMs / 1000;
  snprintf(time, sizeof time, "%lu:%02lu", (unsigned long)(s / 60), (unsigned long)(s % 60));
  drawHeader("Recording", time);
  // Mirrored bars around a centre line, as the voice was heard; silence is the bare line.
  const int fit = min(MAX_BARS, (display.width() - 2 * MARGIN) / BAR_PITCH), shown = min(barCount, fit);
  const int16_t right = display.width() - MARGIN;
  display.drawFastHLine(MARGIN, WAVE_CY, display.width() - 2 * MARGIN, BLACK);
  for (int i = 0; i < shown; i++) {
    const int h = bars[barCount - shown + i] * WAVE_HALF / 255;
    if (h) display.fillRect(right - (shown - i) * BAR_PITCH, WAVE_CY - h, BAR_W, 2 * h + 1, BLACK);
  }
  display.setFont(FONT_SMALL);
  drawCentered(recordMs / 1000 + 10 >= maxSeconds ? "nearly at the limit" : "let go to stop", 142);
  drawHints("", "", "");
}

const char *recordWhileHeld(uint32_t seconds, int16_t *&samples, size_t &count) {
  size_t limit = 0;
  count = 0;
  samples = jobTakeBuffer(BUFFER_SECONDS * AUDIO_RATE, limit);
  limit = min(limit, (size_t)seconds * AUDIO_RATE);
  maxSeconds = limit / AUDIO_RATE;  // less than asked if memory was already split up
  if (!samples || !audioBegin()) {
    jobGiveBuffer(samples);
    return samples ? "Microphone not answering" : "Not enough memory";
  }
  const uint32_t t0 = millis();
  uint32_t shown = 0;
  int peak = 0;
  size_t inBar = 0;  // samples in the bar being measured
  recordMs = 0;
  barCount = 0;
  displayFrame(drawRecording);
  while (inputBHeld() && count < limit) {
    powerAlive();
#if UNIDEX_DEV
    devShotDuringAnimation();  // a test can take a shot of this screen
#endif
    const size_t n = audioRead(samples + count, min((size_t)1024, limit - count));
    for (size_t i = 0; i < n; i++) {
      peak = max(peak, abs((int)samples[count + i]));
      if (++inBar == 1024) {
        addBar(count + i < AUDIO_RATE * 3 / 10 ? 0 : peak);  // the codec clicks as it powers up: not the voice
        peak = 0;
        inBar = 0;
      }
    }
    count += n;
    recordMs = millis() - t0;
    if (recordMs - shown >= 1000) {  // the refresh blocks ~0.4 s; the 1 s DMA buffer keeps recording
      shown = recordMs;
      displayFrame(drawRecording);
    }
  }
  audioEnd();
  powerActivity();
  if (recordMs < MIN_MS) {
    jobGiveBuffer(samples);
    return "Too short: hold B longer";
  }
  return nullptr;
}

String asciiText(const String &s, int *missing) {
  // U+0100-U+017F without their marks (Polish, Czech, Turkish...); ł, đ, ı and the like become their plain letter
  static const char EXTENDED_A[] =
    "AaAaAaCcCcCcCcDdDdEeEeEeEeEeGgGgGgGgHhHhIiIiIiIiIiIiJjKkkLlLlLlLlLlNnNnNnnNnOoOoOoOoRrRrRrSsSsSsSsTtTtTtUuUuUuUuUuUuWwYyYZzZzZzs";
  String out;
  if (missing) *missing = 0;
  for (size_t i = 0; i < s.length(); i++) {
    const uint8_t c = s[i];
    if (c < 0x80) {
      out += (char)c;
      continue;
    }
    if (c == 0xE2 && i + 2 < s.length()) {  // U+2000 block: quotes, dashes, ellipsis
      const uint8_t b = s[i + 2];
      if (b == 0x98 || b == 0x99) out += '\'';
      else if (b == 0x9C || b == 0x9D) out += '"';
      else if (b == 0x93 || b == 0x94) out += '-';
      else if (b == 0xA6) out += "...";
      i += 2;
      continue;
    }
    if (c == 0xC3 && i + 1 < s.length()) {  // Latin-1 letters lose their accents
      static const char *const FOLD = "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYTsaaaaaaaceeeeiiiidnooooo/ouuuuyty";
      const uint8_t b = s[i + 1];
      if (b >= 0x80 && b < 0xC0) out += FOLD[b - 0x80];
      i += 1;
      continue;
    }
    if ((c == 0xC4 || c == 0xC5) && i + 1 < s.length()) {  // Latin Extended-A
      const int cp = ((c & 0x1F) << 6) | (s[i + 1] & 0x3F);
      if (cp >= 0x100 && cp < 0x180) {
        out += EXTENDED_A[cp - 0x100];
        i += 1;
        continue;
      }
    }
    while (i + 1 < s.length() && (s[i + 1] & 0xC0) == 0x80) i++;  // skip the rest of anything else
    out += '?';
    if (missing) (*missing)++;
  }
  return out;
}
