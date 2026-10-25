#include "play.h"
#include "store.h"
#include "../../core/audio.h"
#include "../../core/battery.h"
#include "../../core/devtools.h"
#include "../../core/display.h"
#include "../../core/input.h"
#include "../../core/power.h"
#include "../../core/storage.h"
#include "../../core/theme.h"

static uint32_t playedMs, totalMs;

static void clockText(char *out, size_t len, uint32_t ms) {
  const uint32_t s = ms / 1000;
  snprintf(out, len, "%lu:%02lu", (unsigned long)(s / 60), (unsigned long)(s % 60));
}

static void drawPlaying() {
  char now[12], total[12];
  clockText(now, sizeof now, playedMs);
  clockText(total, sizeof total, totalMs);
  drawHeader("Playing", total);
  display.setFont(FONT_LARGE);
  drawCentered(now, 66);
  drawProgress(100, totalMs ? playedMs * 100 / totalMs : 0);
  display.setFont(FONT_SMALL);
  drawCentered("any button stops it", 132);
  drawHints("", "stop", "");
}

const char *playNote(const String &id) {
  if (const char *low = batteryTooLow()) return low;
  if (!audioVolume()) return "Volume is off: Settings";
  if (!storeValidId(id)) return "No recording";
  const String path = storeWavPath(id);
  if (!storageExists(path.c_str())) return "No recording";
  fs::File f = storageOpen(path.c_str());
  if (!f || f.size() <= 44 || !f.seek(44)) return "Couldn't read the card";
  const size_t total = (f.size() - 44) / 2;
  if (!audioPlayBegin()) return "Speaker not answering";
  static int16_t buf[1024];
  size_t done = 0;
  uint32_t shown = 0;
  bool stopped = false;
  playedMs = 0;
  totalMs = (uint64_t)total * 1000 / AUDIO_RATE;
  displayFrame(drawPlaying);
  while (done < total) {
    powerAlive();
#if UNIDEX_DEV
    devShotDuringAnimation();  // a test can take a shot of this screen
#endif
    if (inputPoll() != Event::None) {
      stopped = true;
      break;
    }
    const int got = f.read((uint8_t *)buf, min(sizeof buf, (total - done) * 2));
    if (got <= 0) break;
    audioPlay(buf, got / 2);  // blocks while the DMA buffers are full, so this runs at the sound's pace
    done += got / 2;
    const size_t queued = AUDIO_PLAY_QUEUE;  // not heard yet
    playedMs = done > queued ? (uint64_t)(done - queued) * 1000 / AUDIO_RATE : 0;
    if (playedMs / 1000 != shown / 1000) {  // each new second; the refresh blocks ~0.35 s, the queue keeps playing
      shown = playedMs;
      displayFrame(drawPlaying);
    }
  }
  audioPlayEnd(stopped);
  f.close();
  powerActivity();
  return nullptr;
}
