#include "power.h"
#include "input.h"
#include "storage.h"
#include "battery.h"
#include <Arduino.h>
#include <driver/gpio.h>
#include <esp_sleep.h>

static uint32_t idleMs = 10000;  // Settings: 10, 20, 30 or 60 s (NVS sleep_s)

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
  idleMs = storageGetInt("sleep_s", 10) * 1000;

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
  if (millis() - lastActivity < idleMs || inputAnyDown()) return;
  // Stay awake on USB power: a computer is connected, or the charger is holding the battery at 4.2 V.
  // The board can't sense USB power directly (VBUS isn't wired to any GPIO), so a plain charger that
  // has finished charging looks like battery and it sleeps. The voltage is read at most every 10 s.
  static uint32_t lastCheck;
  static bool onUsb;
  if (lastCheck == 0 || millis() - lastCheck > 10000) {
    onUsb = batteryCharging();
    lastCheck = millis();
  }
  if (onUsb) return;

  // Keep the panel powered in its own deep sleep (RAM retained), so the first refresh
  // after waking can be partial. RST/CS stay HIGH so it isn't woken or selected.
  gpio_hold_en(PIN_LATCH);
  gpio_hold_en(PIN_EPD_PWR);
  gpio_hold_en(PIN_EPD_RST);
  gpio_hold_en(PIN_EPD_CS);
  gpio_deep_sleep_hold_en();

  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);  // drop powerNap's timer and GPIO sources
  esp_sleep_enable_ext1_wakeup(WAKE_MASK, ESP_EXT1_WAKEUP_ANY_LOW);
  esp_deep_sleep_start();
}

void powerNap() {
  // Light sleep pauses USB, so skip it while a host is connected (the Mac sync needs the port).
  // Also skip while a button is held: its release and long-press timing need polling.
  const uint32_t idle = millis() - lastActivity;
  if (HWCDC::isPlugged() || inputAnyDown() || idle >= idleMs) {
    delay(5);
    return;
  }
  esp_sleep_enable_timer_wakeup((uint64_t)(idleMs - idle) * 1000);  // wake for the deep-sleep check
  gpio_wakeup_enable(GPIO_NUM_0, GPIO_INTR_LOW_LEVEL);
  gpio_wakeup_enable(GPIO_NUM_18, GPIO_INTR_LOW_LEVEL);
  esp_sleep_enable_gpio_wakeup();
  esp_light_sleep_start();
}

void powerSetSleepSeconds(int s) {
  idleMs = s * 1000;
  storagePutInt("sleep_s", s);
}
