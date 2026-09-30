// STEP 0 — button test. Prints every press/release of BOOT and PWR over USB serial.
// Replaced by the real firmware in STEP 1.
#include <Arduino.h>

static const int PIN_BOOT  = 0;   // BOOT button, active LOW
static const int PIN_PWR   = 18;  // PWR button sense, active LOW
static const int PIN_LATCH = 17;  // battery power latch: HIGH keeps the board on

struct Btn { const char *name; int pin; bool down; uint32_t since; };
static Btn btns[] = { {"BOOT", PIN_BOOT, false, 0}, {"PWR", PIN_PWR, false, 0} };

void setup() {
  // Latch power first, so on battery the board stays on after PWR is released.
  pinMode(PIN_LATCH, OUTPUT);
  digitalWrite(PIN_LATCH, HIGH);

  pinMode(PIN_BOOT, INPUT_PULLUP);
  pinMode(PIN_PWR, INPUT_PULLUP);

  Serial.begin(115200);
  uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 3000) delay(10);  // give the USB host a moment

  Serial.println("\n=== STEP 0 button test ===");
  Serial.printf("reset reason: %d\n", (int)esp_reset_reason());
  Serial.printf("PSRAM: %u bytes, flash: %u bytes\n", ESP.getPsramSize(), ESP.getFlashChipSize());
  Serial.println("Press BOOT and PWR (short and long).");
}

void loop() {
  for (Btn &b : btns) {
    bool now = digitalRead(b.pin) == LOW;
    if (now != b.down && millis() - b.since > 30) {  // 30 ms debounce
      uint32_t held = millis() - b.since;
      b.down = now;
      b.since = millis();
      if (now) Serial.printf("%s down\n", b.name);
      else     Serial.printf("%s up   (held %lu ms)\n", b.name, (unsigned long)held);
    }
  }
  delay(5);
}
