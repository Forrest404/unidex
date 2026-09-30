# unidex — tiny e-ink OS for the Waveshare ESP32-S3-ePaper-1.54

PlatformIO + Arduino firmware: a launcher with four apps (Timetable, Name Badge, WiFi Pokédex, Chooser).

## Status

| Step | What | State |
|---|---|---|
| 0 | Identify board + button serial test | done |
| 1 | Project setup + display "hello" | done |
| 2 | Button input (short/long) | done |
| 3 | Power management + deep sleep | done |
| 4 | Storage layer + theme | done |
| 5 | Launcher, splash, icons | done |
| 6 | Name Badge | done |
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
| Panel | 1.54" black/white e-paper, 200 × 200 | confirmed (STEP 1) |
| Controller | SSD1681 | vendor code |
| GxEPD2 class | `GxEPD2_154_D67` (SSD1681 200×200), GxEPD2 1.6.9 | confirmed (STEP 1) |
| Rotation | 0 = upright | confirmed (STEP 1) |

### Pins

| Function | GPIO | Notes | Source |
|---|---|---|---|
| EPD SCK | 12 | SPI | confirmed (STEP 1) |
| EPD MOSI | 13 | SPI (no MISO) | confirmed (STEP 1) |
| EPD CS | 11 | | confirmed (STEP 1) |
| EPD DC | 10 | | confirmed (STEP 1) |
| EPD RST | 9 | | confirmed (STEP 1) |
| EPD BUSY | 8 | | confirmed (STEP 1) |
| EPD power enable | 6 | **active LOW** (LOW = panel powered); external pull-up (R71) turns the panel off if the pin floats | confirmed (STEP 1) |
| Audio power enable | 42 | active LOW, keep HIGH (off) | vendor |
| Battery power latch | 17 | **HIGH = stay on**; has a pull-down, so it must be held HIGH through deep sleep | confirmed (STEP 3) |
| BOOT button | 0 | active LOW, RTC GPIO, external 10k pull-up (R1) | confirmed (STEP 0 test) |
| PWR button | 18 | active LOW, RTC GPIO, external 10k pull-up (R58) | confirmed (STEP 0 test) |
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
  is held HIGH during sleep. *confirmed (STEP 3, on USB and battery).*
- **BOOT (GPIO0)** is a strapping pin. Holding it at reset or power-on enters download
  mode. After boot it's a normal input. Strapping pins are latched only on a chip reset,
  not on a deep-sleep wake, so it's safe as a wake button. *confirmed (STEP 3).*

### Controls

A = BOOT (next/scroll), B = PWR (select/action). Both buttons read cleanly, alone and together,
with no bounce seen at 30 ms debounce. Quick taps measured 145–300 ms, deliberate holds 1.6–3.8 s.

| Event | Meaning |
|---|---|
| A short | next / scroll |
| A long | back to home |
| B short | select / action |
| B long | app-specific extra |

Timing (`src/core/input.cpp`): debounce 30 ms; long press = held 400 ms. The long event fires
while the button is still held, and the release after it is ignored. Presses aren't read while a
screen refresh is running (about 0.3–0.5 s).

### Power

- Deep sleep after 10 s idle (`IDLE_MS` in `src/core/power.cpp`), never while a button is held
  (a held button would wake it straight back up).
- Wake: ext1 ANY_LOW on GPIO0 + GPIO18. The waking press is ignored until released.
- In sleep: GPIO17 held HIGH, GPIO6 held LOW, and panel RST/CS held HIGH. So the panel stays in its
  own deep sleep with its RAM kept, and the first refresh after a wake is partial (no flash).
  At boot each level is set before its hold is released; a floating pin would cut power.
- CPU at 80 MHz. WiFi/BT are never started unless an app starts them.
- `RTC_DATA_ATTR` variables survive deep sleep but not power loss.

### Sources

- Waveshare wiki: https://docs.waveshare.com/ESP32-S3-ePaper-1.54
- Waveshare code: https://github.com/waveshareteam/ESP32-S3-ePaper-1.54
  (`02_Example/Arduino/*/user_config.h`, `src/power/board_power_bsp.cpp`,
  `src/display/epaper_driver_bsp.cpp`, `01_ADC_Test/adc_bsp.cpp`, ESP-IDF V2 `sdkconfig`,
  `04_Hardware/Schematics/ESP32-S3-Touch-ePaper-1.54-Schematic.pdf`)
- Community note on the PWR/GPIO17 latch: https://www.espboards.dev/blog/waveshare-esp32-s3-epaper-esphome-climate/

## Storage

All file and NVS access goes through `src/core/storage.h`. Apps never touch LittleFS or
Preferences directly, so moving files to an SD card later only changes `storage.cpp`.

- Files: LittleFS on internal flash (the 1.5 MB `spiffs` partition in `default_8MB.csv`).
  Sources live in `/data` and are uploaded as a filesystem image. The mount never auto-formats;
  if no image was uploaded, `storageInit()` returns false.
- `storageOpen(path, mode)` returns a standard `fs::File` ("r", "w", "a"; also opens folders for
  `openNextFile()`).
- NVS (namespace `unidex`): `storageGet/PutInt`, `storageGet/PutString`. Puts skip unchanged values.
- Flash wear: open a file once, write everything, close it. Never write inside a loop.

| NVS key | Type | Used by |
|---|---|---|
| `badge` | string | Badge: filename of the last badge shown |

## Theme

`src/core/theme.h`: `FONT_SMALL` (FreeSans 9 pt: header, footer, secondary text) and `FONT_LARGE`
(FreeSans 18 pt: the one focal element). Margin 8 px, header 24 px (title + 1 px rule), footer
22 px (1 px rule + "A …" left, "B …" right). Helpers: `drawHeader`, `drawFooter`, `drawCentered`.
Icons: 40×40, stored as rows of `#`/`.` in `theme.cpp` (2 px strokes, no anti-aliasing), drawn with `drawIcon`.

## File formats

### Badges (`data/badges/*.bmp`)

- 1-bit (monochrome), uncompressed BMP, up to 200×200. Smaller images are centred.
- Either palette order works (index 0 black or white), and bottom-up or top-down rows.
- Shown in filename order, so prefix them: `01-hello.bmp`, `02-…`. Up to 32 files.
- Making one: GIMP → Image → Mode → Indexed → "Use black and white (1-bit) palette" → Export as BMP.
  Or Pillow: `Image.new('1', (200, 200))` … `.save('x.bmp')` (see `tools/make_test_badges.py`).
- Upload with `pio run -t uploadfs`.

In the app: A = next, B = previous; the badge fills the screen with no header or footer. Each
flip is a full refresh (no ghosting); the last one shown is saved in NVS.

## Launcher and apps

- Cold boot: splash (full refresh), then home (partial). After a deep-sleep wake nothing is
  redrawn: the panel still shows where you were.
- Home: 2×2 grid, the selected app inverted. A short = next, B short = open.
- In an app: A long = back home. A short, B short and B long go to the app.
- The open app and the home selection are `RTC_DATA_ATTR`, so they survive sleep.

### Refresh rule (`displayShow` in `src/core/display.cpp`)

Every screen is drawn whole by a `draw()` function. Partial refresh normally; full refresh when
switching apps/home and after every 10 partials (the counter survives sleep), to clear ghosting.
Nothing redraws on a timer: only on input (the Chooser spin will be the one exception).

### App interface (`src/core/app.h`)

```cpp
struct App {
  const char *name;
  const char *const *icon;    // 40x40 pixel art from theme.h
  void (*onEnter)();
  Redraw (*onButton)(Event e);  // None, Partial (small change) or Full (whole image changed)
  void (*draw)();             // draw the whole screen; the launcher refreshes the panel
  void (*onExit)();
};
```

### How to add an app

1. Create `src/apps/<name>/<name>.cpp`. Define static `onEnter`, `onButton`, `draw` and `onExit`,
   then `extern const App <name>App = {"Name", ICON_X, onEnter, onButton, draw, onExit};`
   (see any existing app).
2. Add `<name>App` to the `extern` line and to `APPS[]` in `src/apps/apps.cpp` (sets launcher order).
3. Add a 40×40 icon to `theme.cpp`/`theme.h`.
4. Keep state in `RTC_DATA_ATTR` variables (survives sleep) or NVS via `storage.h` (survives power
   loss). Plain RAM is lost in deep sleep and `onEnter` isn't called again after a wake, so rebuild
   RAM caches lazily (see `ensureList()` in the badge app). Use `drawHeader`/`drawFooter` and the two
   theme fonts. Return `Redraw::None` from `onButton` when nothing changed, `Full` when the whole
   image changes. Turn radios on only inside the app and off before returning.

The home grid fits 4 apps; a 5th needs a second page or a smaller grid.

## Build and flash

```sh
pio run -t upload        # build + flash firmware
pio run -t uploadfs      # upload /data as the LittleFS image
pio device monitor       # serial at 115200 (native USB)
```

Run the two uploads separately: `-t upload -t uploadfs` in one command only wrote the filesystem.
The USB port disappears while the board is asleep: press a button, then upload within 10 s.
If upload still can't connect: hold BOOT, tap RESET (or re-plug USB), release BOOT, retry.
