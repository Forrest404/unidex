# Developing unidex

How unidex is built, tested and published, for anyone changing the code. Using it: see the [README](../README.md).

## Building from source

You need the board ([Waveshare ESP32-S3-ePaper-1.54](https://docs.waveshare.com/ESP32-S3-ePaper-1.54) **V2**), a
USB-C data cable and [PlatformIO](https://platformio.org/install) (CLI or the VS Code extension).

```sh
git clone https://github.com/Forrest404/unidex.git && cd unidex
pio run -t upload       # build and flash the firmware (keeps the settings saved on the board)
```

If the upload can't connect: the USB port disappears while the board sleeps, so press a button and start the upload
within 10 seconds. Still stuck: hold BOOT, tap RESET (or re-plug USB), release BOOT, try again.

WiFi and API keys are never compiled in: they're set over USB from the website's Notes page. The firmware version
comes from git (`tools/version.py`).

## Project layout

```
src/
  main.cpp              setup/loop: input -> launcher -> sleep
  apps/                 one folder per app (timetable, notes, pet, badge, dex, chooser, games, settings)
                        + apps.cpp (launcher order)
  apps/notes/           notes.cpp (screens), job (the online steps in the background), store (files on the card),
                        cloud (Whisper, tidy-up, GitHub), usb (N commands), phone* (Open on phone)
  apps/pet/             pet.cpp (Pet, Dress up, Name), parts.h (the art), meet.cpp (Meet), meet_logic.h,
                        friends_logic.h, send_logic.h (the radio rules, tested on a computer), usb.cpp (P commands)
  apps/badge/           badge.cpp, badge_file.* (BMP checks, shared with the USB upload and Pet)
  core/
    launcher.*          splash, home carousel, routes buttons to the open app
    display.*           e-paper driver (SSD1681) and the refresh rule
    input.*             debounce + short/long press events
    battery.*           battery voltage and percent
    power.*             deep sleep, light sleep between polls, wake, pin holds
    storage.*           files on the SD card + NVS key/value (apps never touch either directly); a file that
                        replaces another is written to a temp file, then storageReplace() checks its size and
                        swaps it in (the old copy is kept until the new one is in place)
    clock.*             PCF85063 clock chip, NTP
    audio.*             ES8311 microphone (16 kHz mono, I2S)
    net.*               WiFi on/off and a small HTTPS client (checks certificates)
    credentials.*       WiFi, API keys and GitHub settings in their own NVS namespace
    usbsync.*           serial protocol for the Mac calendar sync, the Tools page and the Notes page
    link.*              device-to-device radio (ESP-NOW) for the Pet's Meet
    theme.*             fonts, header, button hints, toast, confirm sheet, empty states, 40x40 pixel icons
    devtools.*          test build only: USB screenshots and virtual buttons (tools/devshot.py)
tools/                  badges.py, badge-template.svg, calsync/ (macOS), upload_nostub.py, check_unit.py,
                        devshot.py + walkthroughs/ (screenshots and button walkthroughs), gfxfont.py (fonts)
site/                   the website: installer (index.html), Tools (badge maker, clock, calendar file), Notes
                        (notes.html/notes.js: WiFi, keys, tests, download), serial.js
certs/                  root CA bundle embedded in the firmware for HTTPS (see certs/README.md)
starter/                files to copy to a new SD card: 3 generic badges, an empty timetable
.github/workflows/      site.yml: builds the firmware and publishes the website on each release
```

## Adding an app

1. Create `src/apps/<name>/<name>.cpp`. Define static `onEnter`, `onButton`, `draw` and `onExit`, and optionally
   `onBack` (hold A: up a level, `Redraw::Exit` at the top), `status` (the home screen's line) and `tick` (live
   redraws), then export `extern const App <name>App = {"Name", ICON_X, onEnter, onButton, draw, onExit, onBack,
   status, tick, needsCard};` (any existing app is a template; the interface is in `src/core/app.h`). Every screen
   ends with `drawHints()` so its buttons are shown; content stays above `HINTS_TOP`.
2. Add `<name>App` to the `extern` line and to `APPS[]` in `src/apps/apps.cpp`. That sets the launcher order.
3. Add a 40×40 icon in `src/core/theme.cpp` / `theme.h` (rows of `#` and `.`).

Rules that keep it fast and cheap on battery:

- **`draw()` paints the whole screen**; the launcher does the refresh. Return `Redraw::Partial` (a change),
  `Redraw::Full` (whole new image), `Redraw::Tick` (a live update) or `Redraw::None` from `onButton`/`tick`.
- **RAM is lost in deep sleep** and `onEnter` isn't called again after a wake. Keep state in `RTC_DATA_ATTR`
  variables (survive sleep) or NVS via `storage.h` (survives power loss), and rebuild caches lazily on first use
  (see `ensureList()` in the badge app).
- **Turn radios on only inside the app and off again** before returning (`netClaimed()`: a note is still sending,
  so leave the WiFi alone). Only `tick` redraws without a press.
- Flash wear: open a file once, write everything, close it. Never write inside a loop.
- The home carousel takes any number of apps (one dot each; around 8 still fit across).

## The refresh rule

E-ink ghosts. `displayShow()` uses a fast partial refresh by default and a full (flashing) refresh when switching
apps, when an app asks for one, and after every 10 partials (the counter survives sleep). Animations use
`displayFrame()` (partials that don't count toward the 10) and must end with a full `displayShow()`.

## Build options and the test build

`platformio.ini` sets `-DDEBUG=0`. Set it to `1` for serial logs (`pio device monitor`, 115200) and a short wait
for USB on cold boot. The SD card is never formatted on mount, so a failed mount can't erase your files.

`pio run -e dev` builds a test version with USB screenshots and virtual buttons (`src/core/devtools.h`). It never
sleeps, so flash the normal build again afterwards. Its version ends in `-test` (and any build with uncommitted
changes in `-dirty`), so the website's Sync leaves it alone. For the safety features: `X BATTMV <mv>` pretends the
battery reads that (`X BATTMV 3400`: low; `3300`: "Charge me", which the test build shows but doesn't act on;
`0`: the real reading), `X HANG` stops the main loop so the 30 s watchdog restarts the board, and `X WELCOME` shows
the first-start screens. For the screen and battery: `X GAMEWAVE <frames> [rate]` tries another game waveform,
`X FILL b|w` does a full black or white refresh (several in turn recover a panel left grey), and `X BATTLOG` /
`X BATTCALLS` print the battery readings and every % worked out. `tools/devshot.py run tools/walkthroughs/<app>.txt` presses
through an app and saves every screen as a PNG (with an `index.html` contact sheet); switches make clearing a dry
run and the cloud steps fake, so a walkthrough changes nothing.

Uploads run at 115200 baud, and firmware uploads use esptool's ROM loader (`--no-stub`, added by
`tools/upload_nostub.py`). On this board the faster default and the esptool stub drop the USB link partway
through ("No serial data received"). On USB the board stays awake, otherwise press a button first: its USB port
disappears while it sleeps.

## Tests

- **On a computer** (no board): the rules that don't need hardware live in small headers with their own checks.
  Each test file's first lines give its build command, e.g.
  `c++ -std=c++17 -O2 -I src/apps/pet tools/meettest/meet_test.cpp -o /tmp/meet_test && /tmp/meet_test`.
  `tools/pettest`, `tools/meettest` (Meet, friends, badge sending), `tools/batterytest`, `tools/notetest`,
  `tools/gametest`. `tools/run_tests.sh` builds and runs them all.
- **On GitHub:** every push and pull request runs `.github/workflows/ci.yml`: the computer tests and both firmware
  builds. Library and platform versions are pinned in `platformio.ini`, so a build only changes when they're
  changed there.
- **On one board** (test build): `tools/devshot.py run tools/walkthroughs/<app>.txt` (see above).
  `tools/safetyshot.py` checks the low and flat battery, the watchdog and safe saving, and puts everything back
  (on a board without an SD card it also checks that a calendar or badge is refused rather than reported saved).
- **On two boards** (both on the test build, side by side): `tools/meetshot.py` (meeting, the shared room, timing),
  `tools/friendshot.py` (friends; `--orders` tries every way two people can answer), `tools/badgeshot.py` (sending a
  badge). They put each board's saved data back afterwards.

## The Pet

- **Parts:** `src/apps/pet/parts.h` holds the art (32×32 per part, `#` ink, `o` paper, `.` see-through). The website's
  Pet designer uses the same parts: after changing them, run `python3 tools/pet/export.py` to update
  `site/pet-parts.js` (`--check` says if it's out of date).
- **Meet** (`meet.cpp`, rules in `meet_logic.h`): devices say hello over ESP-NOW about once a second (four times a
  second with someone nearby), each with a random radio address per session. Closeness is the signal strength both
  ways: it tells "within about 15 cm" from "a metre away", not touching from 15 cm, so meeting needs a press. Once
  connected, the device with the lower Pet number is the left screen and runs the show: it sends each act, and both
  play the same frames on a 550 ms beat from when the message arrived (they stay within about 50 ms). Everything is
  sent three times; copies that arrive before the last act ended are ignored.
- **Friends** (`friends_logic.h`): up to 16, in NVS (`pet_friends`). A "yes" is repeated until both sides have
  settled.
- **Sending a badge** (`send_logic.h`): an offer, then 200-byte pieces, each acknowledged and resent if lost, and a
  CRC over the whole file; the receiver checks it's a valid 1-bit BMP before showing it.
- **USB:** `P GET` / `P SET <look hex> <name hex>` (the Tools page's Pet designer).

## Calendar files and the Mac sync

Weekly classes come from `timetable.csv` in the SD card's root (`day,start,end,module,room`, `day` is `Mon`…`Sun`,
24 h times). Synced events are stored in `/events.csv`, one per line:
`YYYY-MM-DD,HH:MM,HH:MM,title,location,notes` (empty times = all day). Title, location and notes are cut at 63, 47
and 160 characters and converted to plain ASCII (the display font has nothing else). Up to 96 entries in total.

The Mac agent (`tools/calsync/`, `tools/calsync/install.sh`) sends the time and the next 7 days of events whenever
the board is plugged in and awake. The timezone is London: `TZ_LONDON` in `src/core/clock.cpp` and `Europe/London`
in `tools/calsync/calsync.swift`.

## The website

`site/` is published to GitHub Pages by `.github/workflows/site.yml` on every **published release** (or by hand
from the Actions tab, for website changes). The workflow builds the firmware from the latest release tag (so the site
always offers a released version), and writes two manifests for
[ESP Web Tools](https://esphome.github.io/esp-web-tools/):

- **Install**: bootloader, partitions, boot_app0 and the app, offered with a full erase.
- **Update**: the app only, so badges, the Dex, events and settings stay.

ESP Web Tools flashes at 115200 baud with a modern esptool stub, which works on this board. The PlatformIO upload
problems came from its older bundled esptool together with a baud switch. The Tools page talks to the device with the
same USB protocol as the Mac agent (`site/serial.js`). Calendar files are parsed with ical.js (repeating events,
moved or cancelled occurrences, time zones) and sent as London time, like `calsync.swift`.

**Sync everything** (top of Tools) does it all in one click:

1. updates the firmware if the site has a newer release (the device reports its version with `V`, taken from
   git at build time by `tools/version.py`; a newer or test build is left alone);
2. sets the clock;
3. sends the calendar file you chose once, which the page remembers;
4. starts the device sending notes still waiting (`N SYNC`; the device carries on by itself).

Your settings and files stay through the update (`site/update.js` writes only the app, like Update).

- **The calendar file:** Chrome may ask once per visit before reading the file again. Export the calendar to the
  same file when it changes. Calendar links (Google, Outlook, iCloud) can't be read by a web page without a
  third-party server, so it's a file.
- **"Sync by itself":** the same sync runs when the device is plugged in and awake while the page is open. It waits
  a few seconds so it doesn't talk over the Mac agent. Both write the calendar, and the last one wins.

**Pet** (Tools): dress up the Pet with a mouse and send it, with an optional name; Load brings back what's on
the device.

Files live on the SD card, so the installer writes no filesystem image; people copy `starter/` to their card or use
the badge maker. `tools/badges.py` makes the badges in `starter/badges/`: anything put there is committed and
public, so keep personal badges on your own SD card instead.

## Hardware notes

Everything here was read from the chip, seen working on the device, or taken from Waveshare's own example code
(marked *vendor*, not independently verified).

<details>
<summary><b>Chip, display and pins</b></summary>

| Item | Value | Source |
|---|---|---|
| Chip | ESP32-S3-PICO-1 (LGA56), rev v0.2, 40 MHz crystal | esptool |
| Flash / PSRAM | 8 MB quad (GD) / 8 MB octal (`memory_type = qio_opi`) | esptool, runtime |
| USB | Native USB-Serial/JTAG (`303A:1001`), no UART bridge | confirmed |
| Panel | 1.54" black/white e-paper, 200×200, SSD1681 | confirmed |
| Driver | our own SSD1681 driver (`src/core/display.cpp`), drawing with Adafruit GFX | confirmed |
| Also on board | SHTC3 temp/humidity, ES8311 audio codec, mic, speaker header | vendor (unused here) |
| Micro SD slot | SD_MMC 1-bit, FAT32, holds all files | confirmed (mounted a 128 GB SDHC card) |

| Function | GPIO | Notes |
|---|---|---|
| EPD SCK / MOSI | 12 / 13 | SPI, no MISO |
| EPD CS / DC / RST / BUSY | 11 / 10 / 9 / 8 | |
| EPD power enable | 6 | **active LOW**; an external pull-up turns the panel off if the pin floats |
| Audio power enable | 42 | active LOW; LOW only while Notes records, HIGH (off) otherwise |
| Audio codec (ES8311) | I2S MCLK 14, BCLK 15, WS 38, DOUT 45, DIN 16 | I2C address 0x18 on the clock chip's bus. Mic only (speaker amp pin 46 unused). `src/core/audio.cpp` is a small driver on the legacy I2S API: Espressif's `esp_codec_dev` needs ESP-IDF 5. Confirmed: records |
| Battery power latch | 17 | **HIGH = stay on**; has a pull-down, so it must be held HIGH, including through deep sleep |
| BOOT button (A) | 0 | active LOW, RTC GPIO, external 10k pull-up |
| PWR button (B) | 18 | active LOW, RTC GPIO, external 10k pull-up |
| I2C SDA / SCL | 47 / 48 | PCF85063 clock (0x51), SHTC3 |
| RTC interrupt | 5 | active LOW (vendor, unused) |
| SD CLK / CMD / D0 | 39 / 41 / 40 | SD_MMC 1-bit (D3/CS not connected, pulled up). Separate from the display's SPI pins, so no bus sharing. No card-detect pin, and no power switch: the card stays powered in deep sleep (idle cards draw roughly 50–200 µA) |
| Battery voltage | 4 (ADC1 ch3) | ×2 divider (R21/R38, 200k 1%), read by `src/core/battery.cpp`. The ADC reads ~2% low: a full battery (charger finished) read 4.08–4.09 V where the cell is ~4.17 V, so readings are scaled by 4170/4085. 100% from 4.15 V (resting LiPo curve); on battery the % only goes down, so noise can't make it bounce |

</details>

<details>
<summary><b>Buttons and power latch</b></summary>

- **PWR (GPIO18)** is a normal readable input as well as the power button. On battery, pressing it powers the
  board through the button; firmware must then drive **GPIO17 HIGH** to latch power on, or the board dies the
  moment you let go. On USB the board is always powered, so the latch makes no difference there.
- **BOOT (GPIO0)** is a strapping pin: held at reset it enters download mode, but afterwards it's a normal input.
  Strapping is only sampled on a chip reset, not on a deep-sleep wake, so it's safe as a wake button.
- Both buttons wake the board from deep sleep (ext1, any-low) as long as GPIO17 is held HIGH.
- Timing: 30 ms debounce, long press = held 300 ms. The long event fires while still held (so you know when to
  let go) and the release afterwards is ignored. Presses aren't read during a screen refresh (~0.3–0.5 s).

</details>

<details>
<summary><b>Power and battery</b></summary>

- Deep sleep after 10 s idle (`IDLE_MS` in `src/core/power.cpp`), never while a button is held.
- In sleep, GPIO17 stays HIGH, GPIO6 LOW, and the panel's RST/CS HIGH, so the panel sits in its own deep sleep
  with its RAM intact and the first refresh after a wake is a partial (no flash). At boot each level is set
  *before* its hold is released; a floating pin would cut battery or panel power.
- While awake, the main loop light-sleeps between polls and wakes on a button or at the 10 s deadline. It skips
  light sleep while a button is held and while USB is connected (light sleep pauses USB, which the Mac sync needs).
- **On USB power it doesn't deep sleep**, so the Mac sync, the badge maker and flashing always find it. The
  board can't sense USB power directly (USB 5 V isn't wired to any GPIO), so "USB power" means a computer is
  talking to it, or the battery reads at least 4.19 V (the charger holding it at 4.2 V while topping up). A plain
  charger that has *finished* charging looks like battery, so it sleeps. Checked at most every 10 s.
- The CPU runs at 80 MHz (240 MHz only while WiFi is on). WiFi is off except during a Dex scan, Notes and the NTP
  sync.
- Datasheet estimates, **not measured** with a meter: about 2 mA awake with light sleep (vs about 20 mA). In deep
  sleep the chip itself draws µA, but the SD card stays powered (roughly 50–200 µA idle), plus the board's
  regulator and charger. Real battery life depends on how often you press things.

</details>

<details>
<summary><b>Clock and storage</b></summary>

- Time lives on the onboard **PCF85063** (own crystal, battery-backed through a diode), stored as UTC, so it
  keeps counting through deep sleep and power-off. It's read once per boot. If its "oscillator stopped" flag is
  set (never set, or battery lost) the Timetable says "time not set". Every Mac sync writes the Mac's time;
  NTP over WiFi (B short in Timetable) and Settings → Date & time are the backups.
- The chip arrived from the factory **stopped and in 12-hour mode** (Control_1 = `0x22`: STOP and 12_24 set), so its
  time was frozen. Now every set uses the datasheet order (STOP, write the time, start in 24-hour mode), and at
  boot a stopped or 12-hour chip is started in 24-hour mode. Its frozen time isn't trusted: after a deep-sleep
  wake the board's own time is written back; after power-on it's set to 2000-01-01, which reads as "not set".
  The USB command `C` prints the chip's registers and the system time, to check it's counting.
- Files live on the **micro SD card** (FAT32, root: `timetable.csv`, `badges/`, `dex.csv`, `events.csv`). It's
  mounted on first use and unmounted before deep sleep; a missing or unreadable card shows "No SD card" in
  Timetable, Badge and Dex instead of crashing. The USB command `S` prints the card's state and files.
- Files used to live in LittleFS on internal flash. The first time a card mounts, any of them missing on the card
  are copied over once (never overwriting); NVS `sd_copied` records that it's done. Small settings live in NVS
  (namespace `unidex`):

| NVS key | Used by |
|---|---|
| `badge` | Badge: filename of the last badge shown |
| `events_crc` | Mac sync: crc32 of the saved `/events.csv`, to skip identical writes |
| `ch_w1`…`ch_w6` | Chooser: wins per square |
| `dex_salt` | Dex: random salt for hashing BSSIDs |
| `sleep_s` | Settings: seconds awake after the last press (10/20/30/60) |
| `invert` | Settings: 1 = white on black |
| `sd_copied` | Storage: the one-time copy from internal flash to the card is done |
| `pet_look`, `pet_name` | Pet: its look (5 parts, 4 bits each) and name |
| `pet_id`, `pet_friends` | Pet: its random number (sent instead of anything about the device) and its friends |

</details>

<details>
<summary><b>USB protocol (Mac sync, website)</b></summary>

Text lines over USB serial, computer → device (`src/core/usbsync.cpp`; senders: `tools/calsync/calsync.swift`,
`site/serial.js`):

| Computer sends | Device replies |
|---|---|
| `?` | `unidex 1` |
| `V` | `OK V <firmware version>` |
| `T <unix seconds>` | `OK T` (clock chip set) |
| `C` | `OK C ctrl1=.. sec=.. min=.. … sys=<unix>` (clock chip diagnostics) |
| `S` | `OK S ok <type> <size> MB, <used> MB used`, then `D <dir>` / `F <path> <bytes>` lines, then `OK S end` (or `OK S none …`) |
| `E <count> <crc32>` + `count` lines `YYYY-MM-DD,HH:MM,HH:MM,title,location` | `OK E <crc>` or `ERR` |
| `L` (badge maker) | `F <name>` per badge, then `OK L` |
| `B <name> <bytes> <crc32>` | `OK B` or `ERR` (name: `[a-z0-9-]+.bmp`, max 16 KB) |
| `D <hex>` (≤ 64 bytes per line) | `K` per line; after the last byte `OK F <name>` (then the board opens it) or `ERR` |
| `N ?` / `N SET <name> <hex>` / `N CLR <name\|all>` (Notes page) | settings, as `NS <name> <set\|unset> <hex>` lines then `OK N ?`; secrets are never sent back |
| `N TEST <wifi\|openai\|anthropic\|github>`, `N MIC` | `OK N TEST <what> ok` or `… fail <reason>`; `OK N MIC <peak> <rms>` |
| `N LIST` / `N READ <id>` / `N DEL <id>` | `NF …` lines then `OK N LIST`; `ND <hex>` lines then `OK N READ <bytes> <crc32>`; `OK N DEL` |
| `N SYNC` | starts sending waiting notes in the background: `OK N SYNC started <n>`, `none`, `busy` or `fail <reason>` |
| `P GET` / `P SET <look hex> <name hex>` (Tools page, Pet) | `OK P <look> <name hex>`; `OK P SET` or `ERR` |

The crc32 (zlib) covers each line plus `\n`. The device writes a temp file and renames it only if the crc matches,
so a cut transfer never leaves a broken file. Empty times mean an all-day event. Gotchas: the ESP32-S3 resets if
RTS is asserted while DTR is not, so the sender clears RTS first, then DTR; replies end in `\r\n`; and a cold
boot keeps the board busy with the splash for about 3.5 s, so the sender waits up to 6 s.

</details>

## Sources

- Waveshare wiki: https://docs.waveshare.com/ESP32-S3-ePaper-1.54
- Waveshare example code: https://github.com/waveshareteam/ESP32-S3-ePaper-1.54
  (`user_config.h` pin definitions, `board_power_bsp.cpp`, `epaper_driver_bsp.cpp`, the V2 schematic)
- PWR/GPIO17 latch behaviour: https://www.espboards.dev/blog/waveshare-esp32-s3-epaper-esphome-climate/
- Waveshare's 1.54" V2 panel driver (MIT): https://github.com/waveshareteam/e-Paper (the partial waveform used
  for game frames)
