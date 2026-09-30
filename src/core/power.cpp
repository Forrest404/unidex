#include "power.h"
#include "input.h"
#include <Arduino.h>
#include <driver/gpio.h>
#include <esp_sleep.h>

static const uint32_t IDLE_MS = 10000;

static const gpio_num_t PIN_LATCH = GPIO_NUM_17;      // HIGH keeps battery power on; has a pull-down
static const gpio_num_t PIN_AUDIO_PWR = GPIO_NUM_42;  // active LOW; external pull-up keeps it off in sleep
static const gpio_num_t PIN_EPD_PWR = GPIO_NUM_6;     // active LOW
static const gpio_num_t PIN_EPD_RST = GPIO_NUM_9;
static const gpio_num_t PIN_EPD_CS = GPIO_NUM_11;
// Both buttons have external 10k pull-ups, so no internal pull-ups are needed in sleep.
static const uint64_t WAKE_MASK = (1ULL << GPIO_NUM_0) | (1ULL << GPIO_NUM_18);

static uint32_t lastActivity;

void powerInit() {
  // Set each level before releasing the hold from the last sleep. A released pin floats, and
  // the pull-ups/downs would then cut battery power or the panel's power (losing its image).
  const struct { gpio_num_t pin; int level; } held[] = {
    {PIN_LATCH, HIGH}, {PIN_EPD_PWR, LOW}, {PIN_EPD_RST, HIGH}, {PIN_EPD_CS, HIGH},
  };
  for (auto &h : held) {
    pinMode(h.pin, OUTPUT);
    digitalWrite(h.pin, h.level);
    gpio_hold_dis(h.pin);
  }
  gpio_deep_sleep_hold_dis();

  pinMode(PIN_AUDIO_PWR, OUTPUT);
  digitalWrite(PIN_AUDIO_PWR, HIGH);

  setCpuFrequencyMhz(80);
  lastActivity = millis();
}

bool powerWokeFromSleep() {
  return esp_reset_reason() == ESP_RST_DEEPSLEEP;
}

void powerActivity() {
  lastActivity = millis();
}

void powerSleepIfIdle() {
  // Sleeping with a button held would wake straight away, in a loop.
  if (millis() - lastActivity < IDLE_MS || inputAnyDown()) return;

  // Keep the panel powered in its own deep sleep (RAM retained), so the first refresh
  // after waking can be partial. RST/CS stay HIGH so it isn't woken or selected.
  gpio_hold_en(PIN_LATCH);
  gpio_hold_en(PIN_EPD_PWR);
  gpio_hold_en(PIN_EPD_RST);
  gpio_hold_en(PIN_EPD_CS);
  gpio_deep_sleep_hold_en();

  esp_sleep_enable_ext1_wakeup(WAKE_MASK, ESP_EXT1_WAKEUP_ANY_LOW);
  esp_deep_sleep_start();
}
