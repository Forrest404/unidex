#include "display.h"
#include <SPI.h>
#include "storage.h"

static const int PIN_SCK = 12, PIN_MOSI = 13, PIN_CS = 11, PIN_DC = 10, PIN_RST = 9, PIN_BUSY = 8;
static const int PIN_EPD_PWR = 6;  // panel power switch, active LOW
static SPISettings spi(4000000, MSBFIRST, SPI_MODE0);  // 4 MHz (X FTEST can try faster)

void displaySetSpiHz(uint32_t hz) { spi = SPISettings(hz, MSBFIRST, SPI_MODE0); }

Display display;

void Display::drawPixel(int16_t x, int16_t y, uint16_t color) {
  if (x < 0 || y < 0 || x >= WIDTH || y >= HEIGHT) return;
  const uint8_t bit = 0x80 >> (x & 7);
  uint8_t &b = buf[(y * WIDTH + x) >> 3];
  if (ink(color) == WHITE) b |= bit;
  else b &= ~bit;
}

void Display::fillScreen(uint16_t color) { memset(buf, ink(color) == WHITE ? 0xFF : 0x00, sizeof buf); }

// ---- The controller (SSD1681): commands over SPI, DC low for the command byte and high for its data. BUSY is
// high while it works.

static void send(uint8_t cmd, const uint8_t *data = nullptr, size_t len = 0) {
  SPI.beginTransaction(spi);
  digitalWrite(PIN_CS, LOW);
  digitalWrite(PIN_DC, LOW);
  SPI.transfer(cmd);
  digitalWrite(PIN_DC, HIGH);
  if (len) SPI.writeBytes(data, len);
  digitalWrite(PIN_CS, HIGH);
  SPI.endTransaction();
}

static void send(uint8_t cmd, std::initializer_list<uint8_t> data) { send(cmd, data.begin(), data.size()); }

static void waitBusy(uint32_t timeoutMs) {
  const uint32_t t0 = millis();
  delay(1);  // BUSY goes high a moment after the command
  while (digitalRead(PIN_BUSY) == HIGH && millis() - t0 < timeoutMs) delay(1);
}

static bool asleep = true;     // in deep sleep: needs a reset before anything else
static bool powered = false;   // the analog side (the voltages that drive the pixels) is on
static bool needFull = true;   // RAM doesn't hold what's on screen (cold boot): the next refresh is full
static bool fastUsed = false;  // our waveform and voltages are loaded, not the panel's own
static uint8_t fastFrames;     // this refresh: 0 = the panel's own waveform; n = ours, the long phase n frames
static uint8_t fastRate;       // frame rate code for ours (higher = faster frames)
static uint32_t busyMs;        // the last refresh: time the panel was busy

// Out of deep sleep (or power-up): reset, then set the panel up. A reset keeps the RAM, so the last image is
// still there to compare against for a partial refresh.
static void wake() {
  if (!asleep) return;
  digitalWrite(PIN_RST, LOW);
  delay(10);
  digitalWrite(PIN_RST, HIGH);
  delay(10);
  waitBusy(100);
  send(0x12);  // software reset: registers back to their defaults, RAM kept
  waitBusy(100);
  send(0x01, {(Display::HEIGHT - 1) & 0xFF, (Display::HEIGHT - 1) >> 8, 0x00});  // 200 gate lines, top to bottom
  send(0x11, {0x03});                                   // RAM address: x then y, both counting up
  send(0x44, {0x00, Display::WIDTH / 8 - 1});           // RAM x range, in bytes
  send(0x45, {0x00, 0x00, (Display::HEIGHT - 1) & 0xFF, (Display::HEIGHT - 1) >> 8});  // RAM y range
  send(0x3C, {0x05});                                   // border: stays white
  send(0x18, {0x80});                                   // the built-in temperature sensor picks the waveform
  asleep = powered = fastUsed = false;
}

// RAM 0x24 is the new image; RAM 0x26 the previous one, which a partial refresh compares against.
static void writeRam(uint8_t ram) {
  send(0x4E, {0x00});
  send(0x4F, {0x00, 0x00});
  send(ram, display.buf, sizeof display.buf);
}

// Runs a refresh. The control byte says what the controller does in order: clock on (0x80), analog on (0x40),
// read the temperature (0x20), load the panel's own waveform (0x10), mode 2 = partial (0x08), show the image
// (0x04), analog off (0x02), clock off (0x01).
static void update(uint8_t control, uint32_t timeoutMs) {
  const uint32_t t0 = millis();
  send(0x22, {control});
  send(0x20);
  waitBusy(timeoutMs);
  busyMs = millis() - t0;
  powered = !(control & 0x02);
}

// The partial waveform Waveshare publishes for this panel (1.54" V2): 153 bytes of waveform (voltage per
// phase for each pixel change, then phase lengths in frames, frame rates), then the gate, source and VCOM
// voltages. Game frames use it with the first, long phase shortened (fastFrames frames instead of 15) and the
// frame rate raised: faster, a little more ghosting, which the full refresh after each round clears.
static const uint8_t PARTIAL_WAVE[159] = {
  0x00, 0x40, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // voltages, one row per kind of pixel change
  0x80, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0x40, 0x40, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0x00, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0x0F, 0, 0, 0, 0, 0, 0,  // group 0: phase A 15 frames (WAVE_LONG_PHASE)
  0x01, 0x01, 0, 0, 0, 0, 0,  // group 1: one frame each of phases A and B
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0x22, 0x22, 0x22, 0x22, 0x22, 0x22,  // frame rates (WAVE_RATE)
  0, 0, 0,
  0x02, 0x17, 0x41, 0xB0, 0x32, 0x28,  // option, gate voltage, source voltages (3), VCOM
};
static const int WAVE_LONG_PHASE = 60, WAVE_RATE = 144;

static void loadFastWave() {
  uint8_t wave[sizeof PARTIAL_WAVE];
  memcpy(wave, PARTIAL_WAVE, sizeof wave);
  wave[WAVE_LONG_PHASE] = fastFrames;
  for (int i = WAVE_RATE; i < WAVE_RATE + 6; i++) wave[i] = fastRate << 4 | fastRate;
  send(0x32, wave, 153);      // waveform
  send(0x3F, {wave[153]});
  send(0x03, {wave[154]});    // gate voltage
  send(0x04, wave + 155, 3);  // source voltages
  send(0x2C, {wave[158]});    // VCOM
  fastUsed = true;
}

static void refreshFull() {
  wake();
  writeRam(0x24);
  writeRam(0x26);  // both the same, so nothing is left over to compare against
  update(0xF7, 5000);
  needFull = false;
}

static void refreshPartial() {
  if (needFull) return refreshFull();
  wake();
  writeRam(0x24);
  if (fastFrames) {
    loadFastWave();
    // Mode 2 with the waveform above (no reload from the panel). Powering the analog side up again costs
    // ~90 ms, so only when it's off; it stays on between frames.
    update(powered ? 0x0C : 0xCC, 1000);
  } else {
    update(0xFC, 2000);  // mode 2 with the panel's own waveform; the analog side stays on
  }
  // The new image into both RAMs: the previous one for the next comparison, and the current one again
  // (skipping this second write left the old image showing through on the next refresh).
  writeRam(0x26);
  writeRam(0x24);
}

// Deep sleep: the image stays on the panel with no power. The next refresh resets the controller, which brings
// back its own waveform and voltages.
static void sleepPanel() {
  if (asleep) return;
  if (powered) update(0x83, 1000);  // analog off
  send(0x10, {0x01});               // deep sleep, keeping the RAM
  asleep = true;
  fastUsed = false;
}

void displayInit(bool initial) {
  pinMode(PIN_EPD_PWR, OUTPUT);
  digitalWrite(PIN_EPD_PWR, LOW);
  if (initial) delay(10);  // let the panel rail settle; after a wake it stayed powered

  pinMode(PIN_CS, OUTPUT);
  digitalWrite(PIN_CS, HIGH);
  pinMode(PIN_DC, OUTPUT);
  digitalWrite(PIN_DC, HIGH);
  pinMode(PIN_RST, OUTPUT);
  digitalWrite(PIN_RST, HIGH);
  pinMode(PIN_BUSY, INPUT);
  SPI.begin(PIN_SCK, -1, PIN_MOSI, PIN_CS);
  asleep = true;
  needFull = initial;
  display.setTextWrap(false);  // with wrap on, measuring a long line reports only its first wrapped piece
  display.inverted = storageGetInt("invert", 0);
}

void displaySetInverted(bool on) {
  display.inverted = on;
  storagePutInt("invert", on);
}

static const int FULL_EVERY = 10;
// Game frames' waveform: the long phase 3 frames at rate code 7 (the fastest that behaves; higher codes got
// slower): ~30 ms on the panel, ~40 ms a frame in all. Measured with X FTEST; 8 frames (~69 ms) gives darker
// blacks if these look too faint.
static const uint8_t FAST_FRAMES = 3, FAST_RATE = 7;
RTC_DATA_ATTR static int partialsSinceFull;  // survives sleep, so the count is honest

static DisplayRefresh last;

DisplayRefresh displayLastRefresh() { return last; }

static void render(void (*draw)(), bool full, char kind) {
  const uint32_t t0 = millis();
  display.fillScreen(WHITE);
  display.setTextColor(BLACK);
  draw();
  if (full) refreshFull();
  else refreshPartial();
  last = {kind, millis() - t0, last.count + 1, busyMs};
}

void displayShow(void (*draw)(), bool full) {
  if (fastUsed) sleepPanel();  // back to the panel's own waveform and voltages
  full = full || partialsSinceFull >= FULL_EVERY;
  partialsSinceFull = full ? 0 : partialsSinceFull + 1;
  render(draw, full, full ? 'F' : 'P');
  sleepPanel();  // e-ink keeps the image
}

static uint8_t frameWave, frameRate = FAST_RATE;  // the waveform displayFrame uses (0: the panel's own)

void displayFrame(void (*draw)()) {
  fastFrames = frameWave, fastRate = frameRate;  // only for this frame: any other refresh gets the panel's own
  render(draw, false, 'f');
  fastFrames = 0;
}

void displayFastFrames(bool on) {
  frameWave = on ? FAST_FRAMES : 0;
  frameRate = FAST_RATE;
}

#if UNIDEX_DEV
void displayFastWave(uint8_t frames, uint8_t rate) { frameWave = frames, frameRate = rate; }
#endif

void displayTick(void (*draw)()) {
  if (fastUsed) sleepPanel();
  render(draw, false, 'T');
  sleepPanel();
}
