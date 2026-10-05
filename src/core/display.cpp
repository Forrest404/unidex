#include "display.h"
#include <SPI.h>
#include "storage.h"

static const int PIN_SCK = 12, PIN_MOSI = 13, PIN_CS = 11, PIN_DC = 10, PIN_RST = 9, PIN_BUSY = 8;
static const int PIN_EPD_PWR = 6;  // panel power switch, active LOW
static const uint32_t SPI_HZ = 4000000;  // the library's default (X FTEST can try faster)

void displaySetSpiHz(uint32_t hz) { display.epd2.selectSPI(SPI, SPISettings(hz, MSBFIRST, SPI_MODE0)); }

Display display(Panel(PIN_CS, PIN_DC, PIN_RST, PIN_BUSY));

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

void Panel::refresh(int16_t x, int16_t y, int16_t w, int16_t h) {
  const uint32_t t0 = millis();
  if (!fastFrames || _initial_refresh) {
    GxEPD2_154_D67::refresh(x, y, w, h);
    busyMs = millis() - t0;
    return;
  }
  // The RAM window is already set: the library wrote this same area just before refreshing it.
  uint8_t wave[sizeof PARTIAL_WAVE];
  memcpy(wave, PARTIAL_WAVE, sizeof wave);
  wave[WAVE_LONG_PHASE] = fastFrames;
  for (int i = WAVE_RATE; i < WAVE_RATE + 6; i++) wave[i] = fastRate << 4 | fastRate;
  _writeCommand(0x32);  // waveform
  _writeData(wave, 153);
  _writeCommand(0x3F);
  _writeData(wave[153]);
  _writeCommand(0x03);  // gate voltage
  _writeData(wave[154]);
  _writeCommand(0x04);  // source voltages
  _writeData(wave + 155, 3);
  _writeCommand(0x2C);  // VCOM
  _writeData(wave[158]);
  fastUsed = true;
  const uint32_t t1 = millis();
  _writeCommand(0x22);
  // Display mode 2 with the waveform above (no reload from OTP). Powering the analog side up again costs
  // ~90 ms, so only when it's off; it stays on between frames.
  _writeData(_power_is_on ? 0x0C : 0xCC);
  _writeCommand(0x20);
  _waitWhileBusy("fast", 1000);
  _power_is_on = true;
  busyMs = millis() - t1;
}

void Panel::hibernate() {
  GxEPD2_154_D67::hibernate();  // the next refresh resets the controller: its own waveform and voltages again
  fastUsed = false;
}

void displayInit(bool initial) {
  pinMode(PIN_EPD_PWR, OUTPUT);
  digitalWrite(PIN_EPD_PWR, LOW);
  if (initial) delay(10);  // let the panel rail settle; after a wake it stayed powered

  // Must come before display.init(): GxEPD2 calls SPI.begin() with default pins,
  // which is a no-op once SPI is already started, so these pins stick.
  SPI.begin(PIN_SCK, -1, PIN_MOSI, PIN_CS);
  displaySetSpiHz(SPI_HZ);
  display.init(0, initial);  // no GxEPD2 timing logs: they'd clutter the USB sync line
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
  if (full) display.setFullWindow();
  else display.setPartialWindow(0, 0, display.width(), display.height());
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
    draw();
  } while (display.nextPage());
  last = {kind, millis() - t0, last.count + 1};
}

void displayShow(void (*draw)(), bool full) {
  if (display.epd2.fastUsed) display.hibernate();  // back to the panel's own waveform and voltages
  full = full || partialsSinceFull >= FULL_EVERY;
  partialsSinceFull = full ? 0 : partialsSinceFull + 1;
  render(draw, full, full ? 'F' : 'P');
  display.hibernate();  // hibernated e-ink keeps the image
}

static uint8_t frameWave, frameRate = FAST_RATE;  // the waveform displayFrame uses (0: the panel's own)

void displayFrame(void (*draw)()) {
  display.epd2.fastFrames = frameWave, display.epd2.fastRate = frameRate;  // only for this frame: any other
  render(draw, false, 'f');                                                // refresh gets the panel's own
  display.epd2.fastFrames = 0;
}

void displayFastFrames(bool on) {
  frameWave = on ? FAST_FRAMES : 0;
  frameRate = FAST_RATE;
}

#if UNIDEX_DEV
void displayFastWave(uint8_t frames, uint8_t rate) { frameWave = frames, frameRate = rate; }
#endif

void displayTick(void (*draw)()) {
  if (display.epd2.fastUsed) display.hibernate();
  render(draw, false, 'T');
  display.hibernate();
}
