#include "input.h"
#include <Arduino.h>

static const uint32_t DEBOUNCE_MS = 30;  // no bounce seen at this value in STEP 0
static const uint32_t LONG_MS = 400;     // tuned on the device in STEP 2

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

void inputInit() {
  for (Button &b : buttons) pinMode(b.pin, INPUT_PULLUP);
}

Event inputPoll() {
  uint32_t now = millis();
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
