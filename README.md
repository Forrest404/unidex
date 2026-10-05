# unidex

A tiny pocket OS for a 1.54" e-ink board: a home screen and seven small apps, driven by two buttons,
built to sleep whenever you aren't pressing something, to save battery (battery life not yet measured).

| App | What it does |
|---|---|
| **Timetable** | Shows your next class or calendar event with a countdown. Reads a weekly CSV and, optionally, your Apple Calendar (synced from a Mac over USB). |
| **Notes** | Hold a button and talk: the note is transcribed (OpenAI Whisper), tidied up (OpenAI or Claude), kept on the SD card and optionally pushed to GitHub as Markdown for Obsidian. |
| **Badge** | Flips through full-screen 1-bit images: name tags, logos, photos. Includes a drag-and-drop converter. |
| **Dex** | A WiFi network collection game. Scan, and every new network name you hear is logged with a rarity. |
| **Chooser** | Pick 2–6 squares, spin, get a random winner. Keeps a tally. |
| **Games** | One-button games: Flappy, Dino, Stack and Jetpack (being added one at a time). |
| **Settings** | Date and time, sleep, invert, battery and info, reset data. |

Built with PlatformIO + Arduino (ESP32-S3). WiFi is never used unless you ask for it (a Dex scan, a Notes recording or sync, or an NTP time sync).

**Install it from your browser, with no tools needed: https://forrest404.github.io/unidex/** (Chrome or Edge on a
computer). The same site has [Tools](https://forrest404.github.io/unidex/tools.html): the badge maker, setting the
clock, sending a calendar file, and the automatic Mac calendar sync; and a
[Notes](https://forrest404.github.io/unidex/notes.html) page for WiFi, API keys and GitHub.

## What you need

- **Board:** [Waveshare ESP32-S3-ePaper-1.54](https://docs.waveshare.com/ESP32-S3-ePaper-1.54), **V2**
  (ESP32-S3-PICO-1, 8 MB flash, 8 MB PSRAM, 200×200 black/white e-paper, BOOT + PWR buttons, PCF85063 clock chip).
  Other boards would need different pins and a different display driver.
- A USB-C data cable.
- Optional: a 3.7 V LiPo on the board's battery connector, to use it untethered.
- [PlatformIO](https://platformio.org/install) (CLI or the VS Code extension).
- For Notes: an [OpenAI API key](https://platform.openai.com/api-keys) (Whisper), 2.4 GHz WiFi, and optionally an
  Anthropic key and a GitHub repo.
- Optional: a Mac for the calendar sync; Python 3 + [Pillow](https://pillow.readthedocs.io) for the badge script.

## Quick start

```sh
git clone https://github.com/Forrest404/unidex.git && cd unidex
pio run -t upload       # build and flash the firmware
```

Files live on a **micro SD card** (FAT32) in the board's slot: copy the contents of `data/` (or `starter/` for the
generic badges) to the root of the card, so it has `timetable.csv` and a `badges/` folder. WiFi and API keys are
never compiled in: set them on the [Notes page](https://forrest404.github.io/unidex/notes.html) (over USB). Badges can also be sent
from the badge maker. Without a readable card, Timetable, Badge and Dex show "No SD card".

If upload can't connect: the USB port disappears while the board sleeps, so press a button and start the
upload within 10 seconds. Still stuck: hold BOOT, tap RESET (or re-plug USB), release BOOT, retry.

## Using it

Two buttons: **A** = BOOT, **B** = PWR. They mean the same thing everywhere:

| Press | Meaning |
|---|---|
| A | next (row, item, page) |
| B | select / open / do |
| hold A (~0.3 s) | back one step; from an app's main screen, home; on the home screen, the previous app |
| hold B | the screen's extra (Today, the picker, rarity, tally, delete, record...) |

Every screen shows its buttons at the bottom in two rows: what a press does, then what a hold does
(`A next  B open` / `hold A: back  hold B: delete`). Anything that can't be undone asks first in a box
(A keeps, B goes ahead), and short messages ("Clock set", "Deleted") pop up in a black pill for a moment.

The home screen shows one app at a time: its icon, its name, a live line under it ("In 12 min: Maths",
"3 notes, 1 waiting", "Badge 5 of 12"), and a dot per app. A moves to the next app, B opens it. After the time set
in Settings (10 s at first) without a press the board goes into deep sleep (it stays awake while on USB power, or
while a note is still sending; see Power below). The screen keeps showing what it last drew (e-ink needs no power
for that). The press that wakes it also counts: tap A on a sleeping home screen and it wakes and moves in one go;
hold a button and it's a long press. Powering on with PWR (from off, on battery) doesn't count, so it can't open an
app.

**Restart:** hold **A and B together for 1 second** ("Restarting / let go of the buttons"), then let go. It boots
fresh to the home screen, as after a flash. Files, badges, the Dex, events, settings and the clock are all kept.
While both are held neither button does its own thing, and it waits for you to let go because BOOT is the chip's
download-mode pin.

The top right of the home screen shows the time and battery level, e.g. `14:32  87%`. While the board is awake
(e.g. on USB) the time and the live line update each minute. While it's asleep the clock chip keeps counting
silently, with no wake-ups, and the screen catches up on the next press. The percentage is an estimate from the
battery voltage, and reads high while charging over USB. A small lightning bolt before it means USB power is present
(it can't tell charging from full). The time is left out until the clock has been set.

### Timetable

The next class or event as a card: "IN 42 MIN" / "NOW, UNTIL 16:00" / "TOMORROW 09:30" on top, the title in
large type (wrapped to 2 lines; a longer title drops to 3 small lines), the time and length, the location, and a
"then 16:30 Maths" line for what comes after.

- **A**: next upcoming item (up to 5; "2/5" in the corner)
- **B**: details: the full title, date, time and length, full location and the event's notes (A pages through long
  ones)
- **Hold B**: Today, the rest of today's list ("now" for what's on): A moves, B opens that event's details
- **Hold A**: back (from details, to where you came from)
- When the clock isn't set, **B** syncs it over WiFi (if WiFi is set up), or set it in Settings

While the board is awake the countdown moves on each minute; asleep, it catches up on the next press.
Edit `data/timetable.csv` for weekly classes:

```csv
day,start,end,module,room
Mon,09:00,10:00,Maths,B12
Wed,18:00,19:30,Robotics Club,Lab 1
```

`day` is `Mon`…`Sun`; times are 24 h `HH:MM`. The header row is optional, and unparseable rows are skipped.
Names are cut at 63 characters and rooms at 47. Up to 96 entries in total (classes + calendar events).
Put it in the root of the SD card.

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

Synced events are stored in `/events.csv` on the SD card, one per line:
`YYYY-MM-DD,HH:MM,HH:MM,title,location,notes` (empty times = all day). Title, location and notes are cut at 63, 47
and 160 characters and converted to plain ASCII, because the display font has nothing else; commas are dropped and
line breaks in notes become " / ". Older 5-field lines still work, just without notes. After updating the firmware,
re-run `tools/calsync/install.sh` once so the Mac agent sends the longer fields.

#### Timezone

The timezone is hard-coded to London. To change it, edit `TZ_LONDON` (a POSIX TZ string) in
`src/core/clock.cpp` and `Europe/London` in `tools/calsync/calsync.swift`.

### Badge

**A** next, **hold B** previous, **B** the picker. The badge fills the screen with no header; its buttons show in a
band along the bottom for a moment when it opens. Each flip is a full refresh (no ghosting), and the last badge shown
is remembered across sleep and power loss.

The picker: 3×3 thumbnails, 9 per page. **A** moves, **B** opens the selected badge, **hold A** goes back to the
one you had. Thumbnails are decoded once and kept in RAM, so moving is only as slow as the panel's partial refresh.

Badges are 1-bit, uncompressed BMPs up to 200×200 (smaller ones are centred) in the SD card's `badges/` folder
(`data/badges/` in the repo is a set to copy there). Files starting with `.` (macOS `._` files) are ignored. They're shown in
filename order, up to 32, so number them: `01-hello.bmp`, `02-…`.

To make one from any image:

- **In the browser:** the badge maker on [Tools](https://forrest404.github.io/unidex/tools.html) (or open
  `site/tools.html` locally). Drop in an image, pick fit or fill, photo
  or line art, adjust the lightness. Then either **Send to device**, which puts it straight on the board over USB
  (Chrome or Edge; plug in and press a button once), and the board opens it at once, or download the BMP for
  the card's `badges/` folder. Sending never overwrites a badge: a taken number moves to the next free one. It keeps the
  Dex, events and settings. Nothing goes to the internet.
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

Then copy the BMPs to the card's `badges/` folder.

### Dex

A WiFi network collection game. **B** scans (about 2 seconds): you get "NEW!" plus the best new find, or
"Nothing new here" with how many networks are nearby.

- **A**: list of everything found, newest first, 5 per page (A pages)
- **Hold B**: counts per rarity. **Hold B** again there to clear the Dex (it asks first)

Each network **name** is logged once, however many access points share it. Hidden networks (no name) are
left out entirely. Rarity, first match wins: `eduroam` = starter; weaker than −80 dBm = rare; open = common;
everything else = uncommon.

It only listens for beacons and never connects to anything. Finds are appended to `/dex.csv` on the board
(`hash,ssid,rssi,enc,rarity,first_seen`). The raw BSSID (the access point's MAC) is never stored, only the first
8 bytes of SHA-256(salt + BSSID) with a random per-device salt, because a MAC with a known vendor prefix could
be brute-forced back from a plain hash. Wiping the board's NVS changes the salt and makes everything new again.

### Chooser

Opens on the number of squares you used last; **A** cycles 2 → 6. **B** spins: the highlight walks the grid,
slowing down, and lands on a winner that was picked up front with the hardware random number generator. The reveal
inverts the winning square ("It's #3."). **B** = spin again, **A** = change the count, **hold B** = the tally of wins
per number over every spin (**hold B** there clears it, after asking).

### Notes

**Hold B** and talk; let go to stop (up to 3 minutes). The screen shows a timer and a level bar while it listens.
About a second after you let go you're back on the Notes screen: the rest happens in the background, with the
step and a progress bar on screen (and on the home screen's line), so you can keep using the device, or record
another note, which waits its turn. It doesn't sleep until it's done. Over WiFi, the device:

1. **transcribes** the recording with OpenAI Whisper (`whisper-1`, about $0.006 a minute),
2. **tidies it up** with OpenAI (`gpt-4o-mini` by default) or Claude (`claude-haiku-4-5` by default), or not at
   all: a one-word topic title, a one-sentence summary, a clean rewrite (keeping every fact, name and number),
   topics, and a calendar event if the note describes a dated plan ("dentist next Friday at 3"),
3. **saves** it to the SD card as Markdown (`/notes/<date-time>.md`, next to the `.wav`), and
4. if switched on, **pushes** it to `<folder>/<Title>.md` in your GitHub repo (`Title 2.md` if the name is
   taken), ready for Obsidian. The original transcript is kept in a folded callout under the clean version.

On the Notes screen: **B** = sync (transcribe recordings made offline, push notes GitHub doesn't have yet),
**A** = the list. In the list: **A** = next, **B** = open, **hold B** = delete. In a note: **A** = next page,
**hold B** = delete (it asks first; a GitHub copy stays). **Hold A** goes back. A recording not transcribed yet
shows when it was made ("2 Oct, 21:50", "waiting").

Set it up on the [Notes page](https://forrest404.github.io/unidex/notes.html): WiFi, the OpenAI key, which
model tidies up (and the Anthropic key for Claude), and GitHub (repo, branch, folder and a
[fine-grained token](https://github.com/settings/personal-access-tokens/new) with Contents: Read and write on that
repo only). Each has a **Test** button that runs on the device. The page also tests the microphone and downloads
notes as `.md` files over USB.

- **eduroam / work WiFi** (WPA2-Enterprise): pick "eduroam / work" on the Notes page and add your username. It
  logs in with PEAP/MSCHAPv2. Without a CA certificate it doesn't check the network's certificate, so a fake access
  point with the same name could capture that password. You can paste your university's CA certificate (from its IT
  pages or https://cat.eduroam.org): the device then checks the network's certificate was signed by that CA. The
  WiFi stack in this Arduino core (ESP-IDF 4.4) can't also check the server's name, so this only stops a fake access
  point if the CA is the university's own private one. The clock's WiFi time sync uses the same network.
- **Without an SD card** it works online only: notes go to GitHub (if on), or are shown once and not kept.
  Recordings made without WiFi are lost unless there's a card.
- **Keys** live in the device's NVS (namespace `unidex_cred`), never in the code or on the card. The page can't
  read them back: it only sees whether each is set and the last 4 characters of API keys. Settings → Reset data
  doesn't remove them; the Notes page's **Clear all keys** and the website's **Install** (full erase) do;
  **Update** keeps them. Anyone with the device and a USB cable could still run `N TEST` with your keys, so treat
  it like an unlocked phone.
- **HTTPS** is checked against the Mozilla root certificates embedded in the firmware (`certs/`).
- The note format and the tidy-up prompt follow [forrest-notes](https://github.com/Forrest404/forrest-notes).

### Games

A list of one-button games, each with its best score. **A** picks a game, **B** plays, **hold A** goes back. In a
game **B** is the one action (tap, or hold); hold A leaves it. The e-ink screen redraws in about 0.4 s, so the games
run at two or three steps a second, in big steps. Best scores are kept on the device; the home line shows your last.

### Settings

The last app on the home screen. A = next row, B = change or open, hold A = back (in the date editor: leave
without saving).

- **Date & time**: set the clock by hand (no WiFi or Mac needed). A steps year → month → day → hour →
  minute → Save; B = +1, hold B = −1; B on Save writes it to the clock chip (London time, summer time
  automatic). The next Mac sync or NTP sync replaces it.
- **Sleep**: 10 / 20 / 30 / 60 s awake after the last press.
- **Invert**: white on black, everywhere (full refresh when switched).
- **Battery & info**: battery voltage and % ("USB" while plugged in), firmware version, storage used,
  time of the last Mac sync.
- **Reset data**: Chooser tally, Dex, calendar events, or everything, each behind a confirm, then a message with
  what happened ("Dex cleared", "Nothing to clear", "No SD card"). Everything also clears the settings, badge
  choice and Dex salt, then restarts. The uploaded badges, timetable, notes and keys stay.

## Preparing a unit for sale

Settings > Reset > Everything keeps WiFi and API keys, so a unit for someone else needs a full erase:

1. **Erase and install.** Use the website's **Install** button (it erases first), or run
   `pio run -t erase && pio run -t upload`.
2. **Check it holds no keys.** Run `~/.platformio/penv/bin/python tools/check_unit.py`. It prints only whether each
   slot is set, never a value, and ends in **PASS** only if every slot reads `unset`. (Pause the Mac agent or
   close the Notes tab first, so the port is free.)
3. **Use a fresh SD card** with only the contents of `starter/`: none of your notes, recordings, timetable or badges.
4. **Include the source link** (https://github.com/Forrest404/unidex, GPL-3.0) in the box.

## How to measure battery life

Nothing here has been measured yet. Measure with the exact battery you'll ship, and don't state a battery life
until you have.

**You need** a multimeter on its mA / µA range in series with the battery lead (a JST extension cable you can cut,
or a breakout), or a power profiler such as a Nordic PPK2 supplying 3.7 V in place of the battery. A USB power meter
won't do: the board stays awake on USB. Unplug USB for every reading.

**Current in each state** (on a multimeter, use the µA range for deep sleep only; the wake-up surge can blow its fuse):

| State | How to get there | Current |
|---|---|---|
| Deep sleep, SD card in | leave it 10 s on the home screen until it sleeps | |
| Deep sleep, no SD card | same, card removed | |
| Awake, idle | press a button and read within 10 s | |
| Screen refresh | moving between apps (peak) | |
| Dex scan | Dex, scan (peak and average) | |
| Notes recording | hold B in Notes | |
| Notes upload | after the recording, while it sends (WiFi) | |

**Run-down test**: charge fully, unplug, use it the way a student would, and note when it shuts off. Settings →
Battery & info shows the voltage along the way.

| Battery (mAh) | How it was used | Start (date, time) | End (date, time) | Hours |
|---|---|---|---|---|
| | | | | |

Rough estimate from the currents: hours ≈ battery mAh × 0.8 ÷ average mA, where the average weights each state by
the time spent in it.

## Project layout

```
src/
  main.cpp              setup/loop: input -> launcher -> sleep
  apps/                 one folder per app (timetable, notes, badge, dex, chooser, settings) + apps.cpp (launcher order)
  apps/notes/           notes.cpp (screens), job (the online steps in the background), store (files on the card),
                        cloud (Whisper, tidy-up, GitHub), usb (N commands)
  core/
    launcher.*          splash, home carousel, routes buttons to the open app
    display.*           GxEPD2 wrapper and the refresh rule
    input.*             debounce + short/long press events
    battery.*           battery voltage and percent
    power.*             deep sleep, light sleep between polls, wake, pin holds
    storage.*           files on the SD card + NVS key/value (apps never touch either directly)
    clock.*             PCF85063 clock chip, NTP
    audio.*             ES8311 microphone (16 kHz mono, I2S)
    net.*               WiFi on/off and a small HTTPS client (checks certificates)
    credentials.*       WiFi, API keys and GitHub settings in their own NVS namespace
    usbsync.*           serial protocol for the Mac calendar sync, the Tools page and the Notes page
    theme.*             fonts, header, button hints, toast, confirm sheet, empty states, 40x40 pixel icons
    devtools.*          test build only: USB screenshots and virtual buttons (tools/devshot.py)
data/                   your files, to copy to the SD card: badges/, timetable.csv
tools/                  badges.py, badge-template.svg, calsync/ (macOS), upload_nostub.py, check_unit.py,
                        devshot.py + walkthroughs/ (screenshots and button walkthroughs), gfxfont.py (fonts)
site/                   the website: installer (index.html), Tools (badge maker, clock, calendar file), Notes
                        (notes.html/notes.js: WiFi, keys, tests, download), serial.js
certs/                  root CA bundle embedded in the firmware for HTTPS (see certs/README.md)
starter/                the filesystem the web installer writes: 3 generic badges, empty timetable
.github/workflows/      site.yml: builds the firmware and publishes the website on each release
```

### The website

`site/` is published to GitHub Pages by `.github/workflows/site.yml` on every **published release** (or by hand
from the Actions tab). The workflow builds the firmware, builds a filesystem image from `starter/` (not `data/`, so
your own badges and timetable stay out of the public installer), and writes two manifests for
[ESP Web Tools](https://esphome.github.io/esp-web-tools/):

- **Install**: bootloader, partitions, boot_app0, app and starter filesystem, offered with a full erase.
- **Update**: the app only, so badges, the Dex, events and settings stay.

ESP Web Tools flashes at 115200 baud with a modern esptool stub, which works on this board. The PlatformIO upload
problems came from its older bundled esptool together with a baud switch. The Tools page talks to the device with the
same USB protocol as the Mac agent (`site/serial.js`). Calendar files are parsed with ical.js (repeating events,
moved or cancelled occurrences, time zones) and sent as London time, like `calsync.swift`.

Files live on the SD card, so the installer writes no filesystem image; people copy `starter/` to their card or use
the badge maker. `data/badges/` holds the same generic set: anything put there is committed and public, so keep
personal badges on your own SD card instead.

### Adding an app

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

### The refresh rule

E-ink ghosts. `displayShow()` uses a fast partial refresh by default and a full (flashing) refresh when switching
apps, when an app asks for one, and after every 10 partials (the counter survives sleep). Animations use
`displayFrame()` (partials that don't count toward the 10) and must end with a full `displayShow()`.

### Build options

`platformio.ini` sets `-DDEBUG=0`. Set it to `1` for serial logs (`pio device monitor`, 115200) and a short wait
for USB on cold boot. The SD card is never formatted on mount, so a failed mount can't erase your files.

`pio run -e dev` builds a test version with USB screenshots and virtual buttons (`src/core/devtools.h`). It never
sleeps, so flash the normal build again afterwards. `tools/devshot.py run tools/walkthroughs/<app>.txt` presses
through an app and saves every screen as a PNG (with an `index.html` contact sheet); switches make clearing a dry
run and the cloud steps fake, so a walkthrough changes nothing.

Uploads run at 115200 baud, and firmware uploads use esptool's ROM loader (`--no-stub`, added by
`tools/upload_nostub.py`). On this board the faster default and the esptool stub drop the USB link partway
through ("No serial data received"). On USB the board stays awake, otherwise press a button first: its USB port
disappears while it sleeps.

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
  regulator and charger. Real battery life depends on how often you press things: see
  [How to measure battery life](#how-to-measure-battery-life).

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

</details>

<details>
<summary><b>Mac sync protocol</b></summary>

Text lines over USB serial, Mac → device (`src/core/usbsync.cpp`, `tools/calsync/calsync.swift`):

| Mac sends | Device replies |
|---|---|
| `?` | `unidex 1` |
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

## Licence and source code

Copyright 2026 Forrest. unidex is free software under the GNU General Public License, version 3 or later
([LICENSE](LICENSE)). Source code: https://github.com/Forrest404/unidex

What the device and website send where: [PRIVACY.md](PRIVACY.md) (a draft).

Third-party libraries, fonts and data built into the firmware, and their licences, are listed in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

To run a modified version, build it with PlatformIO (`pio run`) and flash it over USB (`pio run -t upload`), or
flash a `firmware.bin` from the website's Update button. The board does not check firmware signatures, so any
build you make will run.
