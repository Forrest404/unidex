# unidex — tiny e-ink OS for the Waveshare ESP32-S3-ePaper-1.54

PlatformIO + Arduino firmware: a launcher with four apps (Timetable, Name Badge, WiFi Pokédex, Chooser).

## Status

| Step | What | State |
|---|---|---|
| 0 | Identify board + button serial test | done |
| 1 | Project setup + display "hello" | — |
| 2 | Button input (short/long) | — |
| 3 | Power management + deep sleep | — |
| 4 | Storage layer + theme | — |
| 5 | Launcher, splash, icons | — |
| 6 | Name Badge | — |
| 7 | Timetable + NTP | — |
| 8 | Chooser | — |
| 9 | WiFi Pokédex | — |
| 10 | Polish + battery audit | — |

## Hardware

Legend: **confirmed** = read from the chip itself or seen working on the device;
**vendor** = taken from Waveshare's own example code/docs, not yet seen working here;
**assumed** = inferred, needs checking.

### Board

| Item | Value | Source |
|---|---|---|
| Board | Waveshare ESP32-S3-ePaper-1.54 **V2** | vendor (the V2 is the only version with this chip) |
| Chip | ESP32-S3-PICO-1 (LGA56), revision v0.2 | confirmed (esptool) |
| Flash | 8 MB, GD (mfr 0xC8, dev 0x4017), quad, 3.3 V | confirmed (esptool) |
| PSRAM | 8 MB embedded, octal (OPI) → `memory_type = qio_opi` | confirmed (vendor sdkconfig says octal; 8 MB PSRAM detected at runtime with qio_opi) |
| Crystal | 40 MHz | confirmed (esptool) |
| MAC | 14:c1:9f:d4:69:b0 | confirmed (esptool) |
| USB | Native USB-Serial/JTAG, VID:PID 303A:1001 (no UART bridge) | confirmed |
| Extras on board | PCF85063 RTC, SHTC3 temp/humidity, ES8311 audio codec, TF slot, mic, speaker header | vendor |

### Display

| Item | Value | Source |
|---|---|---|
| Panel | 1.54" black/white e-paper, 200 × 200 | vendor |
| Controller | SSD1681 | vendor code |
| GxEPD2 class | `GxEPD2_154_D67` (SSD1681 200×200) | assumed — verified in STEP 1 |

### Pins

| Function | GPIO | Notes | Source |
|---|---|---|---|
| EPD SCK | 12 | SPI | vendor |
| EPD MOSI | 13 | SPI (no MISO) | vendor |
| EPD CS | 11 | | vendor |
| EPD DC | 10 | | vendor |
| EPD RST | 9 | | vendor |
| EPD BUSY | 8 | | vendor |
| EPD power enable | 6 | **active LOW** (LOW = panel powered) | vendor |
| Audio power enable | 42 | active LOW, keep HIGH (off) | vendor |
| Battery power latch | 17 | **HIGH = stay on**; must be held through deep sleep (`rtc_gpio_hold_en`) | vendor |
| BOOT button | 0 | active LOW, RTC GPIO | confirmed (STEP 0 test) |
| PWR button | 18 | active LOW, RTC GPIO | confirmed (STEP 0 test) |
| RTC interrupt (PCF85063) | 5 | active LOW | vendor |
| I2C SDA / SCL | 47 / 48 | RTC + SHTC3 | vendor |
| Battery voltage | 4 (ADC1 ch3) | ×2 divider | vendor |
| LED | 3 | only used in one vendor example | assumed |

### Buttons and power: how they work

- **PWR (GPIO18)** is a normal readable input, not only a power switch. On battery,
  pressing PWR powers the board through the button. Firmware then drives **GPIO17 HIGH**
  to latch power on. If firmware never latches, the board dies as soon as PWR is released.
  To power off, firmware sets GPIO17 LOW. The vendor example does this when PWR wakes it.
  On USB the board is always powered, so the latch makes no difference there.
  *Readable: confirmed (STEP 0 test, on USB; holding PWR 2.4 s did not cut power).*
- **PWR as a wake source**: the vendor sleep example uses `ext1` wake, ANY_LOW, on
  GPIO0 + GPIO5 + GPIO18. So both buttons can wake from deep sleep, as long as GPIO17
  is held HIGH during sleep. *vendor — verified in STEP 3.*
- **BOOT (GPIO0)** is a strapping pin. Holding it at reset or power-on enters download
  mode. After boot it's a normal input. Strapping pins are latched only on a chip reset,
  not on a deep-sleep wake, so it should be safe as a wake button. *assumed — verified in STEP 3.*

### Control mapping (decided in STEP 0)

A = BOOT (next/scroll), B = PWR (select/action). Both buttons read cleanly, alone and together,
with no bounce seen at 30 ms debounce. Quick taps measured 145–300 ms, deliberate holds 1.6–3.8 s.

### Sources

- Waveshare wiki: https://docs.waveshare.com/ESP32-S3-ePaper-1.54
- Waveshare code: https://github.com/waveshareteam/ESP32-S3-ePaper-1.54
  (`02_Example/Arduino/*/user_config.h`, `src/power/board_power_bsp.cpp`,
  `src/display/epaper_driver_bsp.cpp`, `01_ADC_Test/adc_bsp.cpp`, ESP-IDF V2 `sdkconfig`)
- Community note on the PWR/GPIO17 latch: https://www.espboards.dev/blog/waveshare-esp32-s3-epaper-esphome-climate/

## Build and flash

```sh
pio run -t upload        # build + flash firmware
pio device monitor       # serial at 115200 (native USB)
```

If upload can't connect: hold BOOT, tap RESET (or re-plug USB), release BOOT, retry.
