// ES8311 microphone capture, and sound on the speaker (recordings, a chime). Register values follow Espressif's es8311 driver in esp_codec_dev (Apache-2.0),
// as used by Waveshare's 08_Audio_Test for this board. That library needs ESP-IDF 5, so this is a small
// standalone version on the legacy I2S driver and the Wire bus the clock chip already uses.
#include "audio.h"
#include <Arduino.h>
#include <Wire.h>
#include <driver/i2s.h>
#include "storage.h"

static const uint8_t ES8311_ADDR = 0x18;
// Active LOW; an external pull-up keeps it off. It also powers the I2C pull-ups, so the clock (clock.cpp) turns it on
// at start-up and it stays on: when sound stops, the codec is only put to sleep.
static const gpio_num_t PIN_AUDIO_PWR = GPIO_NUM_42;
static const i2s_port_t PORT = I2S_NUM_0;
static const int PIN_MCLK = 14, PIN_BCLK = 15, PIN_WS = 38, PIN_DOUT = 45, PIN_DIN = 16;  // Waveshare board config
static const int PIN_AMP = 46;  // the speaker amplifier's enable, active HIGH
static const uint8_t MIC_GAIN = 7;  // REG16 PGA step: 0 = 0 dB ... 7 = 42 dB (a small onboard mic needs most of it)

static bool running, playing;  // the mic / the speaker (one at a time: they share the I2S port)
static int dmaBuffers;  // of 1024 samples: how much sound can be queued and not yet heard

static bool writeReg(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(ES8311_ADDR);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

static int readReg(uint8_t reg) {
  Wire.beginTransmission(ES8311_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0 || Wire.requestFrom((uint16_t)ES8311_ADDR, (size_t)1) != 1) return -1;
  return Wire.read();
}

// Slave mode, MCLK from the ESP32 at 256 x 16 kHz = 4.096 MHz, I2S 16-bit, ADC (mic) only.
static bool configureCodec() {
  writeReg(0x44, 0x08);  // I2C noise immunity; written twice: the first write after power-up can fail
  if (!writeReg(0x44, 0x08)) return false;
  static const uint8_t init[][2] = {
    {0x01, 0x30}, {0x02, 0x00}, {0x03, 0x10}, {0x16, 0x24}, {0x04, 0x10}, {0x05, 0x00},
    {0x0B, 0x00}, {0x0C, 0x00}, {0x10, 0x1F}, {0x11, 0x7F}, {0x00, 0x80},  // reset, then power on
    {0x01, 0x3F},  // MCLK from the pin, all clocks on
    {0x13, 0x10}, {0x1B, 0x0A}, {0x1C, 0x6A},  // ADC high-pass filters
    {0x44, 0x08},  // no DAC reference on the ADC's second channel: only the mic
    // 16 kHz from 4.096 MHz (coeff_div row): pre_div 1, mult 1, adc/dac div 1, osr 0x10/0x20, lrck 0x00ff, bclk 4
    {0x02, 0x00}, {0x05, 0x00}, {0x03, 0x10}, {0x04, 0x20}, {0x07, 0x00}, {0x08, 0xFF}, {0x06, 0x03},
    {0x09, 0x4C}, {0x0A, 0x0C},  // serial ports: I2S, 16-bit; DAC input muted (bit 6), ADC output on
    {0x17, 0xBF}, {0x0E, 0x02}, {0x12, 0x00}, {0x14, 0x1A},  // ADC volume 0 dB, analog PGA on, analog mic
    {0x0D, 0x01}, {0x15, 0x40}, {0x37, 0x08}, {0x45, 0x00},  // power up, ADC ramp
    {0x16, MIC_GAIN},
  };
  for (auto &r : init)
    if (!writeReg(r[0], r[1])) return false;
  return true;
}

static void suspendCodec() {
  static const uint8_t off[][2] = {
    {0x32, 0x00}, {0x17, 0x00}, {0x0E, 0xFF}, {0x12, 0x02}, {0x14, 0x00}, {0x0D, 0xFA}, {0x15, 0x00},
    {0x02, 0x10}, {0x00, 0x00}, {0x00, 0x1F}, {0x01, 0x30}, {0x01, 0x00}, {0x45, 0x00}, {0x0D, 0xFC}, {0x02, 0x00},
  };
  for (auto &r : off) writeReg(r[0], r[1]);
}

// Powers the codec and starts I2S: capture from the mic, or (play) output to the speaker.
static bool start(bool play, int buffers = 16) {
  pinMode(PIN_AUDIO_PWR, OUTPUT);
  digitalWrite(PIN_AUDIO_PWR, LOW);  // already on (see above); kept so audio never depends on the clock going first
  delay(20);  // let the codec's supply settle before talking to it

  // The ESP32 is the I2S master and drives MCLK, which the codec needs before its registers take effect.
  i2s_config_t cfg = {};
  cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | (play ? I2S_MODE_TX : I2S_MODE_RX));
  cfg.sample_rate = AUDIO_RATE;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  // The mic is the ADC's left channel. Played sound goes out on both, whichever one the DAC takes.
  cfg.channel_format = play ? I2S_CHANNEL_FMT_RIGHT_LEFT : I2S_CHANNEL_FMT_ONLY_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.dma_buf_count = dmaBuffers = buffers;  // 16 x 1024 samples = 1 s: a screen refresh (~0.4 s) never drops any
  cfg.dma_buf_len = 1024;
  cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
  i2s_pin_config_t pins = {};
  pins.mck_io_num = PIN_MCLK;
  pins.bck_io_num = PIN_BCLK;
  pins.ws_io_num = PIN_WS;
  pins.data_out_num = PIN_DOUT;
  pins.data_in_num = PIN_DIN;
  if (i2s_driver_install(PORT, &cfg, 0, nullptr) != ESP_OK) return false;
  i2s_set_pin(PORT, &pins);

  if (!configureCodec()) {
#if DEBUG
    Serial.printf("audio: ES8311 not answering (id %02x)\n", readReg(0xFD));
#endif
    i2s_driver_uninstall(PORT);
    return false;
  }
  i2s_zero_dma_buffer(PORT);
  return true;
}

static void stop() {
  suspendCodec();
  i2s_driver_uninstall(PORT);
  pinMode(PIN_MCLK, INPUT);  // stop driving the unpowered codec's pins
  pinMode(PIN_BCLK, INPUT);
  pinMode(PIN_WS, INPUT);
  pinMode(PIN_DOUT, INPUT);
}

bool audioBegin() {
  if (running) return true;
  if (playing) return false;
  running = start(false);
  return running;
}

size_t audioRead(int16_t *out, size_t samples) {
  if (!running) return 0;
  size_t bytes = 0;
  i2s_read(PORT, out, samples * sizeof(int16_t), &bytes, pdMS_TO_TICKS(100));
  return bytes / sizeof(int16_t);
}

void audioEnd() {
  if (!running) return;
  stop();
  running = false;
}

// One note: a sine at `hz` for `ms`, faded in and out so it doesn't click.
static void tone(float hz, int ms) {
  static int16_t frames[256 * 2];  // stereo
  const int total = AUDIO_RATE * ms / 1000, fade = AUDIO_RATE / 100;  // 10 ms
  float phase = 0;
  const float step = 2 * PI * hz / AUDIO_RATE;
  for (int done = 0; done < total;) {
    const int n = min(256, total - done);
    for (int i = 0; i < n; i++, done++) {
      const float env = min(1.0f, min(done, total - done) / (float)fade);
      frames[2 * i] = frames[2 * i + 1] = (int16_t)(sinf(phase) * env * 9000);  // about a quarter of full scale
      phase += step;
    }
    size_t wrote;
    i2s_write(PORT, frames, n * 2 * sizeof(int16_t), &wrote, portMAX_DELAY);
  }
}

// Volume 0-10, saved. 8 is the DAC at 0 dB (how it always played); each step is 3 dB, so 10 is +6 dB and 1 is
// -21 dB. 0 is off: the amplifier stays off.
static const int VOLUME_DEFAULT = 8;
static int volume = -1;  // not read yet

int audioVolume() {
  if (volume < 0) volume = constrain(storageGetInt("volume", VOLUME_DEFAULT), 0, AUDIO_VOLUME_MAX);
  return volume;
}

void audioSetVolume(int v) {
  volume = constrain(v, 0, AUDIO_VOLUME_MAX);
  storagePutInt("volume", volume);
}

// `buffers`: 16 (1 s) for a recording, which plays on while the screen refreshes; a chime needs only a few.
static bool playBegin(int buffers) {
  if (running || playing || !start(true, buffers)) return false;  // never over a recording
  // Speaker path: DAC input on (16-bit I2S, not muted), DAC unmuted at the volume (REG32: 0xBF = 0 dB, 0.5 dB steps).
  const int v = audioVolume();
  writeReg(0x09, 0x0C);
  writeReg(0x31, 0x00);
  writeReg(0x32, v ? 0xBF + (v - VOLUME_DEFAULT) * 6 : 0x00);
  pinMode(PIN_AMP, OUTPUT);
  digitalWrite(PIN_AMP, v ? HIGH : LOW);
  delay(10);  // the amplifier wakes up
  playing = true;
  return true;
}

bool audioPlayBegin() { return playBegin(16); }

void audioPlay(const int16_t *mono, size_t samples) {
  static int16_t frames[256 * 2];  // stereo
  while (playing && samples) {
    const size_t n = min((size_t)256, samples);
    for (size_t i = 0; i < n; i++) frames[2 * i] = frames[2 * i + 1] = mono[i];
    size_t wrote;
    i2s_write(PORT, frames, n * 2 * sizeof(int16_t), &wrote, portMAX_DELAY);
    mono += n;
    samples -= n;
  }
}

void audioPlayEnd(bool now) {
  if (!playing) return;
  // Played to the end: silence pushes the last of the sound out of the DMA buffers before the amplifier goes off.
  if (!now) tone(0, dmaBuffers * 1024 * 1000 / AUDIO_RATE + 20);
  digitalWrite(PIN_AMP, LOW);
  stop();
  playing = false;
}

bool audioChime() {
  if (!playBegin(2)) return false;
  tone(880, 140);
  tone(1319, 220);
  audioPlayEnd(false);
  return true;
}
