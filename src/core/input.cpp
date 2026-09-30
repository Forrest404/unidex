#include "input.h"
#include <Arduino.h>
#include <esp_sleep.h>

static const uint32_t DEBOUNCE_MS = 30;  // no bounce seen at this value in STEP 0
static const uint32_t LONG_MS = 300;     // tuned on the device in STEP 2
static const uint32_t RESET_MS = 1000;   // both buttons held this long = restart

struct Button {
  int pin;
  Event shortEv, longEv;
  bool down;
  bool longSent;
  uint32_t changedAt;
};

// Both buttons are active LOW. GPIO0 is a strapping pin, but only at reset, so it's a normal input here.
static Button buttons[] = {
  {0, Event::AShort, Event::ALong, false, false, 0},
  {18, Event::BShort, Event::BLong, false, false, 0},
};
static Event pending = Event::None;  // a wake tap that was over before we could see it
static bool resetSent;

void inputInit() {
  // After a deep-sleep wake the press that woke us counts as input. After power-on it doesn't:
  // holding PWR to switch on would otherwise open an app.
  const bool buttonWake = esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT1;
  const uint64_t wokeBy = buttonWake ? esp_sleep_get_ext1_wakeup_status() : 0;
  for (Button &b : buttons) {
    pinMode(b.pin, INPUT_PULLUP);
    const bool held = digitalRead(b.pin) == LOW;
    b.down = held;
    b.longSent = held && !buttonWake;  // power-on press: its release produces nothing
    b.changedAt = 0;                   // a held wake press started at about boot time
    if (!held && (wokeBy & (1ULL << b.pin))) pending = b.shortEv;  // tapped and released during boot
  }
}

bool inputAnyDown() {
  for (Button &b : buttons)
    if (b.down) return true;
  return false;
}

Event inputPoll() {
  if (pending != Event::None) {
    Event e = pending;
    pending = Event::None;
    return e;
  }
  uint32_t now = millis();
  // Both held: neither gives its own events (no "home" on the way to a restart), and after 3 s: Reset.
  Button &a = buttons[0], &b = buttons[1];
  if (a.down && b.down) {
    a.longSent = b.longSent = true;
    if (!resetSent && now - max(a.changedAt, b.changedAt) >= RESET_MS) {
      resetSent = true;
      return Event::Reset;
    }
  } else {
    resetSent = false;
  }
  for (Button &b : buttons) {
    bool pressed = digitalRead(b.pin) == LOW;
    if (pressed != b.down && now - b.changedAt > DEBOUNCE_MS) {
      b.down = pressed;
      b.changedAt = now;
      if (pressed) {
        b.longSent = false;
      } else if (!b.longSent) {
        return b.shortEv;
      }
    }
    // Long fires while still held, so the user knows when to let go.
    if (b.down && !b.longSent && now - b.changedAt >= LONG_MS) {
      b.longSent = true;
      return b.longEv;
    }
  }
  return Event::None;
}
