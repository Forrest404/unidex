# unidex

A tiny pocket OS for a 1.54" e-ink board: a home screen and four small apps, driven by two buttons,
running for days on a battery because it sleeps whenever you aren't pressing something.

| App | What it does |
|---|---|
| **Timetable** | Shows your next class or calendar event with a countdown. Reads a weekly CSV and, optionally, your Apple Calendar (synced from a Mac over USB). |
| **Name Badge** | Flips through full-screen 1-bit images: name tags, logos, photos. Includes a drag-and-drop converter. |
| **Dex** | A WiFi Pokédex. Scan, and every new network name you hear is logged with a rarity. |
| **Chooser** | Pick 2–6 squares, spin, get a random winner. Keeps a tally. |

Built with PlatformIO + Arduino (ESP32-S3). WiFi is never used unless you ask for it (a Dex scan, or an NTP time sync).

## What you need

- **Board:** [Waveshare ESP32-S3-ePaper-1.54](https://docs.waveshare.com/ESP32-S3-ePaper-1.54), **V2**
  (ESP32-S3-PICO-1, 8 MB flash, 8 MB PSRAM, 200×200 black/white e-paper, BOOT + PWR buttons, PCF85063 clock chip).
  Other boards would need different pins and a different display driver.
- A USB-C data cable.
- Optional: a 3.7 V LiPo on the board's battery connector, to use it untethered.
- [PlatformIO](https://platformio.org/install) (CLI or the VS Code extension).
- Optional: a Mac for the calendar sync; Python 3 + [Pillow](https://pillow.readthedocs.io) for the badge script.

## Quick start

```sh
git clone https://github.com/Forrest404/unisex.git && cd unisex

# Optional, only for NTP time sync over WiFi (git-ignored, never committed):
cp src/secrets.example.h src/secrets.h   # then fill in WIFI_SSID / WIFI_PASS

pio run -t upload       # build and flash the firmware
pio run -t uploadfs     # upload data/ (badges, timetable) to the board's filesystem
```

Run the two uploads as separate commands, and do both: the firmware doesn't contain your badges or timetable,
and without the filesystem image the apps show "no badges" / "nothing coming up".

If upload can't connect: the USB port disappears while the board sleeps, so press a button and start the
upload within 10 seconds. Still stuck: hold BOOT, tap RESET (or re-plug USB), release BOOT, retry.

## Using it

Two buttons: **A** = BOOT, **B** = PWR.

| Press | Meaning |
|---|---|
| A short | next / scroll |
| A long (hold ~0.3 s) | back to the home screen |
| B short | select / action |
| B long | app-specific extra |

The home screen is a 2×2 grid: A moves the highlight, B opens. After 10 seconds without a press the board goes
into deep sleep. The screen keeps showing what it last drew (e-ink needs no power for that); press either button
to wake it and it carries on where you were.

### Timetable

Shows the next class in large type, then "in 42 min", "now, ends in 20 min" or "Tue 09:00", then time and room.

- **A**: next upcoming item (up to 5)
- **B long**: the rest of today (A scrolls, B returns)
- **B short**: sync the clock over WiFi (needs `secrets.h`)

The countdown updates when you press a button, never on a timer, to save power.
Edit `data/timetable.csv` for weekly classes:

```csv
day,start,end,module,room
Mon,09:00,10:00,Maths,B12
Wed,18:00,19:30,Robotics Club,Lab 1
```

`day` is `Mon`…`Sun`; times are 24 h `HH:MM`. The header row is optional, and unparseable rows are skipped.
Names are cut at 23 characters and rooms at 11. Up to 96 entries in total (classes + calendar events).
Then run `pio run -t uploadfs`.

#### Apple Calendar sync (macOS)

`tools/calsync/` is a small LaunchAgent. Whenever the board is plugged into your Mac **and awake**, it sends the
current time and the next 7 days of events from all your calendars (repeats expanded).

```sh
tools/calsync/install.sh              # builds UnidexSync.app, asks for Calendar access, starts the agent
tools/calsync/install.sh uninstall
```

Plugging in doesn't wake the board, so press a button; the sync takes about 2 seconds. Log:
`~/Library/Application Support/UnidexSync/sync.log`. Calendar permission lives in
System Settings → Privacy & Security → Calendars.

**Stop the agent before flashing** so it doesn't grab the serial port:
`launchctl bootout gui/$(id -u)/com.forrest.unidex-sync`, and afterwards
`launchctl bootstrap gui/$(id -u) ~/Library/LaunchAgents/com.forrest.unidex-sync.plist`.

Synced events are stored in `/events.csv` on the board (not in `data/`, so `uploadfs` wipes them until the next
sync). Titles and locations are converted to plain ASCII because the display font has nothing else.

#### Timezone

The timezone is hard-coded to London. To change it, edit `TZ_LONDON` (a POSIX TZ string) in
`src/core/clock.cpp` and `Europe/London` in `tools/calsync/calsync.swift`.

### Name Badge

**A** next, **B** previous. The badge fills the screen with no header. Each flip is a full refresh (no ghosting),
and the last badge shown is remembered across sleep and power loss.

Badges are 1-bit, uncompressed BMPs up to 200×200 (smaller ones are centred) in `data/badges/`. They're shown in
filename order, up to 32, so number them: `01-hello.bmp`, `02-…`.

To make one from any image:

- **In the browser:** open `tools/badge-maker.html` (double-click it). Drop in an image, pick fit or fill, photo
  or line art, adjust the lightness, and download the BMP. Put it in `data/badges/`. Nothing is uploaded anywhere.
- **From the command line** (PNG, JPG, HEIC, …; needs Pillow):

  ```sh
  python3 tools/badges.py ~/Downloads/photo.jpg    # -> data/badges/<next number>-photo.bmp
  python3 tools/badges.py --crop selfie.heic       # fill the screen, cutting the edges
  ```

  Photos are dithered, line art gets clean black-and-white edges (`--dither` / `--no-dither` to force), and
  transparency becomes white.
- **In Illustrator, Figma or anything else:** design at 200×200 px in pure black and white (lines thinner than
  1 px vanish; `tools/badge-template.svg` is a blank artboard), export a PNG at 72 ppi, then run it through the
  script above.

Then run `pio run -t uploadfs`. Everything in `data/` is copied to the board's 1.5 MB filesystem, so keep large
source images elsewhere.

### Dex

A WiFi Pokédex. **B** scans (about 2 seconds): you get "NEW!" plus the best new find, or "nothing new".

- **A**: list of everything found, newest first, 5 per page (A pages, B goes back)
- **B long**: counts per rarity. Hold **B** again there to clear the dex (then B = yes, A = no)

Each network **name** is logged once, however many access points share it. Rarity, first match wins:
`eduroam` = starter; hidden or weaker than −80 dBm = rare; open = common; everything else = uncommon.

It only listens for beacons and never connects to anything. Finds are appended to `/dex.csv` on the board
(`hash,ssid,rssi,enc,rarity,first_seen`). The raw BSSID (the access point's MAC) is never stored, only the first
8 bytes of SHA-256(salt + BSSID) with a random per-device salt, because a MAC with a known vendor prefix could
be brute-forced back from a plain hash. Wiping the board's NVS changes the salt and makes everything new again.

### Chooser

Opens on 2 squares; **A** cycles 2 → 6. **B** spins: the highlight walks the grid, slowing down, and lands on a
winner that was picked up front with the hardware random number generator. The reveal inverts the winning square
("You got #3"). **B** = spin again, **A** = back to the count, **B long** = tally of wins per square.

## Project layout

```
src/
  main.cpp              setup/loop: input -> launcher -> sleep
  apps/                 one folder per app (timetable, badge, dex, chooser) + apps.cpp (launcher order)
  core/
    launcher.*          splash, home grid, routes buttons to the open app
    display.*           GxEPD2 wrapper and the refresh rule
    input.*             debounce + short/long press events
    power.*             deep sleep, light sleep between polls, wake, pin holds
    storage.*           LittleFS files + NVS key/value (apps never touch either directly)
    clock.*             PCF85063 clock chip, NTP
    usbsync.*           serial protocol for the Mac calendar sync
    theme.*             fonts, header/footer helpers, 40x40 pixel icons
data/                   uploaded to the board with `pio run -t uploadfs`
  badges/  timetable.csv
tools/                  badge-maker.html, badges.py, badge-template.svg, calsync/ (macOS)
```

### Adding an app

1. Create `src/apps/<name>/<name>.cpp`. Define static `onEnter`, `onButton`, `draw` and `onExit`, then export
   `extern const App <name>App = {"Name", ICON_X, onEnter, onButton, draw, onExit};`
   (any existing app is a template; the interface is in `src/core/app.h`).
2. Add `<name>App` to the `extern` line and to `APPS[]` in `src/apps/apps.cpp`. That sets the launcher order.
3. Add a 40×40 icon in `src/core/theme.cpp` / `theme.h` (rows of `#` and `.`).

Rules that keep it fast and cheap on battery:

- **`draw()` paints the whole screen**; the launcher does the refresh. Return `Redraw::Partial` (small change),
  `Redraw::Full` (whole new image) or `Redraw::None` from `onButton`.
- **RAM is lost in deep sleep** and `onEnter` isn't called again after a wake. Keep state in `RTC_DATA_ATTR`
  variables (survive sleep) or NVS via `storage.h` (survives power loss), and rebuild caches lazily on first use
  (see `ensureList()` in the badge app).
- **Turn radios on only inside the app and off again** before returning. Nothing redraws on a timer.
- Flash wear: open a file once, write everything, close it. Never write inside a loop.
- The home grid fits 4 apps; a fifth needs a second page or a smaller grid.

### The refresh rule

E-ink ghosts. `displayShow()` uses a fast partial refresh by default and a full (flashing) refresh when switching
apps, when an app asks for one, and after every 10 partials (the counter survives sleep). Animations use
`displayFrame()` (partials that don't count toward the 10) and must end with a full `displayShow()`.

### Build options

`platformio.ini` sets `-DDEBUG=0`. Set it to `1` for serial logs (`pio device monitor`, 115200) and a short wait
for USB on cold boot; with it on, a missing filesystem prints "run pio run -t uploadfs". The filesystem is never
auto-formatted on mount, so a failed mount can't erase your files.

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
| Driver | GxEPD2 1.6.9, class `GxEPD2_154_D67`, rotation 0 | confirmed |
| Also on board | SHTC3 temp/humidity, ES8311 audio codec, TF slot, mic, speaker header | vendor (unused here) |

| Function | GPIO | Notes |
|---|---|---|
| EPD SCK / MOSI | 12 / 13 | SPI, no MISO |
| EPD CS / DC / RST / BUSY | 11 / 10 / 9 / 8 | |
| EPD power enable | 6 | **active LOW**; an external pull-up turns the panel off if the pin floats |
| Audio power enable | 42 | active LOW; kept HIGH (off) |
| Battery power latch | 17 | **HIGH = stay on**; has a pull-down, so it must be held HIGH, including through deep sleep |
| BOOT button (A) | 0 | active LOW, RTC GPIO, external 10k pull-up |
| PWR button (B) | 18 | active LOW, RTC GPIO, external 10k pull-up |
| I2C SDA / SCL | 47 / 48 | PCF85063 clock (0x51), SHTC3 |
| RTC interrupt | 5 | active LOW (vendor, unused) |
| Battery voltage | 4 (ADC1 ch3) | ×2 divider (vendor, unused) |

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
- The CPU runs at 80 MHz (240 MHz only during a Dex scan). WiFi is off except inside the Dex scan and the NTP sync.
- Datasheet estimates, **not measured** with a meter: about 2 mA awake with light sleep (vs about 20 mA), and
  tens of µA in deep sleep. Real battery life depends on how often you press things.

</details>

<details>
<summary><b>Clock and storage</b></summary>

- Time lives on the onboard **PCF85063** (own crystal, battery-backed through a diode), stored as UTC, so it
  keeps counting through deep sleep and power-off. It's read once per boot. If its "oscillator stopped" flag is
  set (never set, or battery lost) the Timetable says "time not set". Every Mac sync writes the Mac's time;
  NTP over WiFi (B short in Timetable) is the backup.
- Files live in LittleFS on the 1.5 MB `spiffs` partition of `default_8MB.csv`, built from `data/`. Small
  settings live in NVS (namespace `unidex`):

| NVS key | Used by |
|---|---|
| `badge` | Badge: filename of the last badge shown |
| `events_crc` | Mac sync: crc32 of the saved `/events.csv`, to skip identical writes |
| `ch_w1`…`ch_w6` | Chooser: wins per square |
| `dex_salt` | Dex: random salt for hashing BSSIDs |

</details>

<details>
<summary><b>Mac sync protocol</b></summary>

Text lines over USB serial, Mac → device (`src/core/usbsync.cpp`, `tools/calsync/calsync.swift`):

| Mac sends | Device replies |
|---|---|
| `?` | `unidex 1` |
| `T <unix seconds>` | `OK T` (clock chip set) |
| `E <count> <crc32>` + `count` lines `YYYY-MM-DD,HH:MM,HH:MM,title,location` | `OK E <crc>` or `ERR` |

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
- Display library: [GxEPD2](https://github.com/ZinggJM/GxEPD2)
