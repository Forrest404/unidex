<h1>
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="docs/brand/logo-white.svg">
    <img alt="unidex" src="docs/brand/logo.svg" width="320">
  </picture>
</h1>

**A pocket e-ink OS for the Waveshare ESP32-S3 1.54" e-paper board.** It shows your next class with a live countdown,
records voice notes and tidies them into Markdown for Obsidian, and reminds you of anything you tell it out loud.
There's a little Pet you can dress up, which meets your friends' Pets on their unidex (they visit, chat and swap
badges). It also flips through name badges, plays four one-button games, runs apps from the app store, and goes to
sleep whenever you aren't pressing a button, looking after its battery and your files as it goes.

**And it's yours to hack.** Anyone can write apps and plugins for it in a few lines of Lua, in a plain text editor,
with no tools to install: a game, a tracker, a clock, a dashboard that fetches from the web, a badge that changes
every day. Install them from the website's Apps page, try them in the emulator, and send them in for the store.
[How to make one](#apps-and-plugins-make-your-own).

It runs on two buttons and a battery. The code is public, and you're free to use, fork and improve it for
non-commercial use. Installing takes about a minute from your browser, with no tools to set up.

Featured on [XDA](https://www.xda-developers.com/open-source-os-waveshare-esp32-s3-epaper/).

**Don't have the board?** Pre-orders for a fully assembled and tested unidex are now closed. To hear when more are
available, join the wait list at [unidex.shop](https://www.unidex.shop).

If you'd like to support the project, there's a [Ko-fi](https://ko-fi.com/forrest404).

<p>
  <a href="https://github.com/Forrest404/unidex/releases/latest"><img height="36" alt="Latest release" src="https://img.shields.io/github/v/release/Forrest404/unidex?style=for-the-badge"></a>
  <a href="LICENSE"><img height="36" alt="Licence: PolyForm Noncommercial" src="https://img.shields.io/badge/licence-PolyForm%20Noncommercial-black?style=for-the-badge"></a>
  <a href="https://tools.unidex.shop/"><img height="36" alt="Install in your browser" src="https://img.shields.io/badge/install-in%20your%20browser-black?style=for-the-badge"></a>
  <a href="https://tools.unidex.shop/store.html"><img height="36" alt="Get apps" src="https://img.shields.io/badge/get-apps-black?style=for-the-badge"></a>
  <a href="https://www.unidex.shop"><img height="36" alt="Join the wait list" src="https://img.shields.io/badge/join%20the-wait%20list-black?style=for-the-badge"></a>
</p>

![unidex on the 200×200 e-paper screen. Top row: the next class on the home screen, the Timetable countdown, today's classes, the notes list and an open note. Bottom row: the welcome a new device shows (the buttons, then a QR code for the website), the Pet, the factory reset question and a round of Dino](docs/screens.png)

| App | What it does |
|---|---|
| **Timetable** | Your next class or calendar event with a live countdown, and today's list. |
| **Notes** | Hold a button and talk: the note is transcribed, tidied up and saved, and can go to GitHub for Obsidian. |
| **Reminders** | A to-do list you talk to: hold B and say "call Sam tomorrow at 5". When it's due, it chimes and shows on screen. |
| **Pet** | A cute creature you dress up and name. Bring two unidex together and the Pets visit, chat, become friends and swap badges. |
| **Badge** | Full-screen name tags, logos and photos. |
| **Games** | Four one-button games, Flappy, Dino, Stack and Jetpack, with best scores, and the games you add from the store. |
| **Settings** | Date and time, sync, sleep, volume, alert sound, invert, battery and info, reset data, factory reset. |
| **Apps** | Everything else: apps and plugins from the store (Chooser, the random picker, is one), or ones you write yourself. |

**New in v1.9: set it up from your phone, and Reminders.** In Settings, hold B and scan the QR code: your phone
sets up WiFi, keys and settings with no computer or cable ([how](#set-up-from-your-phone)). Reminders is a to-do list
you talk to, and it chimes when something's due. Notes plays your recordings back on the speaker, and a note recorded
without WiFi sends itself once WiFi is back. You can also pick ElevenLabs instead of OpenAI to transcribe (its free
plan needs no card), set the speaker's volume, and fetch your calendar over WiFi (Settings > Sync). Every change is
in [CHANGELOG.md](CHANGELOG.md).

If unidex is useful to you, a star on GitHub helps other people find it.

## Getting started

**You'll need:**

- the [Waveshare ESP32-S3-ePaper-1.54](https://docs.waveshare.com/ESP32-S3-ePaper-1.54) board, **V2** (or a
  ready-made unidex: [join the wait list](https://www.unidex.shop));
- a USB-C **data** cable and a computer with Chrome or Edge;
- a **micro SD card** (FAT32) for your files;
- optionally, a 3.7 V LiPo battery on the board's battery connector, so you can use it away from USB.

**Then:**

1. **Install.** Open **https://tools.unidex.shop/**, plug the board in, press one of its buttons so it's
   awake, and click **Install**. There's nothing to download. The first time it starts, it shows the buttons, then
   **Set it up**: a QR code for your phone's camera, which joins the device's own WiFi and opens its setup page
   (see [Set up from your phone](#set-up-from-your-phone)), with the website's address underneath if you'd rather
   use a computer.
2. **Set up the SD card.** Copy the contents of the [`starter`](starter) folder to the card (a few badges and an
   empty timetable) and put it in the board's slot.
3. **Add your timetable.** Send your calendar from the website's
   [Tools](https://tools.unidex.shop/tools.html) page, or edit `timetable.csv` on the card (see
   [Timetable](#timetable)).
4. **Voice notes, if you want them.** Set up WiFi and your API keys on the
   [Setup](https://tools.unidex.shop/setup.html) page, or from your phone: in **Settings**, **hold B** (see
   [Set up from your phone](#set-up-from-your-phone) and [Notes](#notes)).

## The buttons

There are two: **A** (BOOT) and **B** (PWR). They do the same thing everywhere:

| Press | Meaning |
|---|---|
| A | next (row, item, page) |
| B | select / open / do |
| hold A | back; from an app, home; on the home screen, the previous app |
| hold B | the screen's extra (shown at the bottom, e.g. `hold B: delete`) |

Each screen shows what A and B do along the bottom. Anything that can't be undone asks first (A keeps it, B goes
ahead), and short messages like "Clock set" or "Deleted" appear for a moment.

The home screen shows one app at a time, with a live line under it ("In 12 min: Maths", "3 notes", "Mochi,
2 friends"). A moves to the next app and B opens it. The top right shows the time and battery (`14:32 87%`), and a
small lightning bolt means it's on USB power.

- **Sleep:** after a few seconds without a press (10 s, or whatever you set in Settings) it goes to sleep. The
  screen keeps showing what it last drew, and any button wakes it. It stays awake on USB, and while a note is still
  sending.
- **Restart:** hold **A and B together for 1 second**, then let go. Your files, settings and the clock are kept.

## The apps

### Timetable

It shows your next class or event as "IN 42 MIN", "NOW, UNTIL 16:00" or "TOMORROW 09:30", with the title, the time,
the room and what comes next. **A** moves to the next one, **B** opens the details (date, time, room, notes), and
**hold B** shows today's list.

Classes that repeat every week go in `timetable.csv` on the SD card:

```csv
day,start,end,module,room
Mon,09:00,10:00,Maths,B12
Wed,18:00,19:30,Robotics Club,Lab 1
```

Calendar events (Apple, Google, Outlook...) can come three ways:

- **Over WiFi, no computer:** save your calendar's private link on the
  [Setup](https://tools.unidex.shop/setup.html) page (the Calendar card says where Google, Outlook and iCloud keep
  it), then use **Settings > Sync** on the device. It sets the clock and fetches this week's events.
- **From the Tools page:** export a `.ics` file from your calendar and send it, or let **Sync everything** send it
  each time you plug in.
- **On a Mac,** the automatic calendar sync on the Tools page sends your calendar whenever the board is plugged in
  and awake.

The time zone is London.

### Notes

**Hold B** and talk, then let go to stop (a note can be up to 3 minutes). You're back on the screen in about a
second, and the rest happens in the background, with progress shown on screen:

1. the recording is **transcribed** (OpenAI Whisper, about $0.006 a minute, or ElevenLabs Scribe, whose free plan
   covers about half an hour a month),
2. it's **tidied up** (by OpenAI or Claude, or not at all): a title, a one-line summary, a clean version that keeps
   every fact, topics, and a calendar event if you described a dated plan,
3. it's **saved** to the SD card as Markdown next to the recording, and
4. if you switch it on, it's **sent to your GitHub repo**, ready for Obsidian.

No WiFi? The recording waits on the card and goes by itself once WiFi is in reach: the device tries when you wake
it and every 10 minutes while it's on. **B** on the Notes screen sends anything waiting straight away. **A** opens
the list (**B** opens a note and **hold B** deletes it). Inside an open note, **B** plays the recording on the
speaker, sent or not (any button stops it), and **hold B** is **Open on phone**. It shows a QR code: join the
device's own WiFi and you can browse, search, share and copy your notes on your phone. Hold A to stop it.

Set it up on the [Setup page](https://tools.unidex.shop/setup.html), or [from your phone](#set-up-from-your-phone). You can save up to five WiFi networks
(home, eduroam and work networks, a phone hotspot: the device joins whichever is in range), which service
transcribes (OpenAI, or ElevenLabs with its own key), your OpenAI key, which model tidies up (and an Anthropic key
if you pick Claude), and GitHub (a repo and a
[fine-grained token](https://github.com/settings/personal-access-tokens/new) with Contents: Read and write on that
repo only). Each has a **Test** button. If you'd rather not pay for anything, pick ElevenLabs and turn the tidy-up
off: you get the plain transcript with a short title.

**Other languages:** notes are transcribed and tidied up in the language you speak, never translated into English.
On Auto (the default) the service works the language out; on a short note it can guess wrong, so you can set
yours in Settings > Note language or in the Setup tab on your phone (16 languages). The screen only has letters
without accents, so accented letters lose their marks ("Lodz", "Cestina"). A note in a script it can't show at all
(Arabic, Chinese, Greek, Russian...) says so: hold B in the note to read it on your phone.

Your keys stay on the device: the page can't read them back, and they're never in the code or on the card. Treat
the device like an unlocked phone, because anyone holding it could use them.

### Reminders

**Hold B** and say one: "call Sam tomorrow at 5pm", "take the cake out in 20 minutes", "dentist on the 3rd of
November". It's written down by the same service as your notes, so set up Notes first. The device works out the
to-do and its time by itself, with no AI.

In the list, **A** moves down and **B** ticks one off (press it again to undo). The last row clears the ticked ones.
The home screen shows what's next ("5pm: Call Sam"), or how many are late. It holds up to 40.

When one is due, the device wakes up, chimes and shows it over whatever's on screen. **B** marks it done, **A**
brings it back in 10 minutes, and **hold A** leaves it in the list.

No WiFi? The recording waits on the card and is added when WiFi is back. It tries every 10 minutes, or press **B**
on its "waiting for WiFi" row. Times count from when you spoke, so "in 20 minutes" means 20 minutes after you said
it, not after it was sent.

### Clock

A timer, alarms, a stopwatch and the time in other cities. **A** moves down the list and **B** opens one.

- **Timer:** **B** steps through 1, 2, 3, 5, 10, 15, 20, 25, 30 and 45 minutes, 1, 1.5 and 2 hours (**hold B** goes
  back one), and **A** starts it. While it runs, **A** pauses and **hold B** resets.
- **Alarms:** up to three. Each has a time (**B** +1, **hold B** -1, **A** next, as when setting the date), on or
  off, and a repeat: once, every day, weekdays or weekends. A "once" alarm turns itself off after it rings.
- **Stopwatch:** **B** starts and stops it, **A** takes a lap (the last three are shown), **hold B** resets. It keeps
  counting while the device sleeps.
- **World:** up to four cities, each with its time, how far ahead or behind London it is, and "tomorrow" or
  "yesterday". **B** picks the next city (24 to choose from), **hold B** the one before.

When a timer ends or an alarm is due, the device wakes up and rings over whatever's on screen, chiming every few
seconds for up to two minutes. **B** stops it; **A** snoozes an alarm for 9 minutes or gives a timer a minute more.
Alarms and timers ring even with Settings > Alert sound off (Volume still applies, and 0 is silent). The home
screen shows what's going on ("Timer: 5 min left", "Alarm 7:00 tomorrow").

### Pet

The Pet is a creature built from a body (blob, cat, bear, bunny, frog or robot), eyes, a mouth, a hat and an extra.
It blinks every few seconds, and **A** says hi. **B** opens Dress up: **A** moves between rows, **B** changes the
part, and **hold B** goes back one. The **Name** row names it a letter at a time, and the name shows as a tag over
its head. **Your name** (the next row, or the Pet card on your phone) is what it calls you. You can also dress it
up and name it with a mouse on the website's [Tools](https://tools.unidex.shop/tools.html) page.

It also keeps an eye on your day, from your timetable, calendar and reminders, and says the next thing in a speech
bubble: "Maths in 15 min, B12" (excited when it's close), "In Maths till 8:35", "Call Sam at 5pm", "Don't forget:
Hand in the art project", "Morning Sam!" or "Free till tomorrow 9:00". From 22:00 to 07:00 it's asleep. The first
four show on its line on the home screen too, so you see them without opening it.

**Meet** (**hold B** on the Pet) is for two unidex side by side:

1. **Bring them close.** The dots at the top fill up. When it says "press B", press **B** on either one.
2. **The two screens become one room.** The title shows which way round to hold them. The Pets greet each other,
   then **B** sends your Pet to visit the other screen (the other one steps aside and they chat in speech
   bubbles), **A** says something, and **hold B** swaps screens or sends a badge. Every few seconds they also play
   something by themselves.
3. **Become friends.** The first time two Pets meet, both screens ask "Be friends?". If you both say yes, you get a
   heart, their own greeting next time, and a friends list (**A** on the searching screen).
4. **Send a badge** to a friend (**hold B** in the room). Pick one, and your friend sees it first and chooses
   whether to keep it.
5. **Pull them apart** and each Pet walks home.

The radio is only on while Meet is open. It talks directly to the other device (no internet), and it stops after 2
minutes without a press. Your Pet and its friends are kept through restarts and updates.

### Badge

Badge flips through full-screen badges: **A** for the next one, **hold B** for the previous one, and **B** for a
picker with thumbnails. Make badges in the badge maker on the website's
[Tools](https://tools.unidex.shop/tools.html) page: drop in any image, adjust it, and **Send to device**
(it opens straight away) or download it for the card's `badges` folder. They're shown in file name order, up to 32.
To delete one, **hold B** on it in the picker (it asks first), or tap **Delete** under it in the phone portal's Tools
tab.

### Games

There are four built in: Flappy, Dino, Stack and Jetpack. **A** picks one and **B** plays. In a game, **B** is the
one action (tap or hold, depending on the game). Best scores are kept on the device.

Games from the store show up here too, after the four, with their best scores. So do the store's other apps in its
games category, such as Dex (the WiFi collecting game), Dice and Coin & RPS, which open as normal apps. **Hold B** on
a store game to offload or delete it.

### Settings

- **Date & time:** set the clock by hand (the website, the Mac sync and WiFi can also set it).
- **Sync:** **B** sets the clock from the internet and, if you've saved a calendar link, fetches this week's events
  for the Timetable. The row shows when it last synced.
- **Sleep:** stay awake for 10, 20, 30 or 60 seconds after the last press.
- **Volume:** the speaker, for playing notes and the reminder chime. **B** is louder, **hold B** quieter, with a
  chime at each step; 0 turns it off.
- **Alert sound:** off, and a reminder falling due still shows on screen but doesn't chime (nor do apps). The
  Clock's alarms and timer still ring.
- **Note language:** **B** steps through Auto, English, Spanish, French ... Korean: the language your notes are
  in (see [Notes](#notes)).
- **Invert:** white on black.
- **Battery & info:** battery voltage and %, firmware version, storage used, and the device's name (`unidex-1A2B`,
  handy when you ask for help).
- **Reset data:** clear calendar events, the Pet's friends, or everything (settings and calendar; your badges,
  timetable, notes, apps, keys, Pet and your name stay). Each one asks first.
- **Factory reset** (at the bottom of Reset data, and it asks twice) is for passing the device on. It wipes the WiFi
  and keys, the Pet and its friends, your name, every setting and the calendar, then starts like new. Notes, badges
  and the timetable on the SD card stay, so format the card on a computer as well.
- **Hold B** in the list sets it up from your phone (below).

#### Set up from your phone

No computer needed. In **Settings**, **hold B**: the device starts its own WiFi and shows a QR code. Scan it with
your phone's camera and join, and a page opens by itself with five tabs:

- **Notes:** read, search, play, share, copy and delete your notes (Delete at the bottom of a note, or slide it
  left in the list).
- **Settings:** its name (which its WiFi is called from the next start), sleep, volume, invert, and the time
  (set from your phone's clock when it's wrong).
- **Setup:** what the website's Setup page does: WiFi networks (it lists the ones nearby), the OpenAI, ElevenLabs
  and Anthropic keys, the language your notes are in, the tidy-up, GitHub, the calendar link, and Remove everything.
- **Tools:** the badge maker (choose a photo or take one, adjust it, and it's saved as a new badge that the Badge
  app opens on), your badges with Delete, and the Pet designer (each part of its look, its name, and yours).
- **Apps:** the app store. Tap Install and the device downloads the app itself (your phone has no internet while
  it's on the device's WiFi, so the device looks for your saved WiFi first and joins it if it's in range, which
  takes a few seconds before the QR code shows; if it isn't, the code shows straight away). The apps on the device are listed above the store (tap to show them), with Update and Delete, and
  the store has a search box. With no WiFi saved or in
  range, the store shows the list from last time and Install waits until it can connect.

Press **Done** at the bottom (or hold A on the device) when you're finished. If you changed WiFi or keys, the device
then joins your WiFi, checks each service and lists how it went. Keys never come back to the page: a saved one
shows as its last 4 characters.

## Apps and plugins: make your own

unidex is built to be taken apart. Beyond the apps above, it runs **apps, games and plugins written by anyone**, and
writing one is meant to be easy: an app is a folder with two small files, written in [Lua](https://www.lua.org/), in
any text editor. There's nothing to compile and no toolchain to install.

```lua
local count = tonumber(unidex.data.read("count") or "0")

function draw()
  unidex.header("Hello")
  unidex.centered(80, tostring(count), "large")
  unidex.hints("", "add", "")
end

function onB()
  count = count + 1
  unidex.data.write("count", tostring(count))
  return "partial"
end
```

**What an app can do:** draw anything on the screen (text in five sizes, lines, boxes, images, QR codes, icons),
use both buttons the same way the built-in apps do, keep its own files on the SD card, read the time and the battery,
wake itself up for the next redraw, and, if it asks for the `wifi` permission, fetch things from the web over HTTPS.
Games, trackers, timers, quizzes, flash cards, a page of today's weather, a habit tracker: whatever you like.

**Plugins** add to the built-in apps instead. The first kind adds badges to Badge that are drawn by code each time
they're shown, like **Today**, which always shows the date. More built-in apps will take plugins.

**Games** are played in Games, next to the built-in four, with B as the one button. You write four functions (start
a round, step one frame, draw it, and the score), and the Games app runs the frames, the game-over screen and the
best score. [Making games](https://github.com/Forrest404/unidex-apps/blob/main/GAMES.md) explains how, with a
template to start from. Games need firmware newer than v1.9.

**Safe to try anything.** Each app runs in a sandbox: its own memory, its own folder, a time limit on every step,
and no way to reach your WiFi password, your keys, your notes or other apps' files. If an app breaks, the device
says "App error" with the line it broke on, and goes back to the list. It never takes the device down with it.

**Getting started:**

1. Read [Writing apps for unidex](docs/APPS.md): the whole API, the limits, games, plugins, and examples.
2. Copy the template, or any app, from the [store's repository](https://github.com/Forrest404/unidex-apps): every
   app lives there.
3. Try it: in the [browser emulator](https://tools.unidex.shop/try.html) without a device, or
   on your unidex with `python3 tools/appinstall.py install your-app`.
4. Share it: open a pull request to [unidex-apps](https://github.com/Forrest404/unidex-apps). Automatic checks try
   it in the emulator, then it's read by a person, and accepted apps appear on the website's
   [Apps](https://tools.unidex.shop/store.html) page.

## The website

Everything runs in Chrome or Edge on a computer and talks to the device over USB. Nothing is uploaded anywhere.

- **[Install](https://tools.unidex.shop/)** has **Install**, which puts unidex on a board (it erases the
  board first), and **Update**, which installs the newest version and keeps your settings, keys and files.
- **[Tools](https://tools.unidex.shop/tools.html)** has **Sync everything** (the newest firmware, the
  clock, your calendar file, and notes waiting to send, in one click, and optionally every time you plug in), the
  badge maker, the Pet designer, the clock and calendar, and the automatic Mac calendar sync.
- **[Setup](https://tools.unidex.shop/setup.html)** is where you set up WiFi, API keys and GitHub, run the
  tests, and download notes. On a phone, set up from the device instead: [Settings, hold B](#set-up-from-your-phone).
- **[Apps](https://tools.unidex.shop/store.html)** shows what's on your unidex and the store's apps and
  plugins: install, update, offload (the app goes, its data stays) or delete (it warns you how much data goes with
  it).

To update, use **Update** on the Install page, or **Sync everything** on Tools, which updates by itself when
there's a newer release.

## Battery

The percentage is an estimate from the battery's voltage. While charging, it climbs slowly to 100% over the last
stretch instead of jumping there, and 100% means it has been topping up long enough. The board can't tell a
finished plain wall charger from a battery, so it may go to sleep while on one. How long a charge lasts depends on
how much you use it: it sleeps between presses, and WiFi is only on while a note or reminder sends, during a time
sync, or when an app uses it.

When the battery gets low, the things that need a lot of power (WiFi, the Pet's Meet and recording)
say **"Battery too low"**. When it's flat, the screen shows **"Charge me"** and the device switches itself
off so the battery isn't drained further. Plug in USB to charge it, then press PWR.

## Troubleshooting

- **The website can't find the device.** It's asleep, so press a button and try again. If it's a brand-new board
  (or one with other firmware), unplug it, hold **BOOT** while you plug it back in, let go, then try again.
- **"No SD card".** Check that the card is FAT32 and pushed fully into the slot. Timetable, Reminders, Badge and
  Apps need it.
- **"App error".** The app hit a bug or one of its limits; the message says where. Tell its author (open an issue
  on [unidex-apps](https://github.com/Forrest404/unidex-apps)), or offload or delete it with **hold B** in Apps.
- **"Time not set".** Use **Settings > Sync** (over WiFi), set it by hand in Settings, or use Set the clock or
  Sync everything on the Tools page.
- **It froze.** It restarts by itself after 30 seconds, or you can hold **A + B** for a second to restart it now.
- **Still stuck, or found a bug?** [Open an issue](https://github.com/Forrest404/unidex/issues/new/choose) with the
  firmware version and what happened. For security problems, see [SECURITY.md](SECURITY.md).

## Privacy

unidex has no account, analytics or tracking. [PRIVACY.md](PRIVACY.md) covers what the device and website send
and where (Notes and Reminders, the Pet's Meet, the website).

## Licence

Copyright 2026 Forrest. From version 1.5, unidex is licensed under the **PolyForm Noncommercial License 1.0.0**
([LICENSE](LICENSE)). Here's the short version, though the licence itself is what counts:

- **You can** use it, study it, fork it, change it and share your changes, for non-commercial purposes: personal
  use, hobby projects, study and research, and use by schools, charities and other non-commercial organisations.
- **You can't** sell it or use it commercially without written permission from Forrest. That includes selling
  devices with unidex on them, selling the software, or building it into a paid product. To ask about a commercial
  licence, open an issue on GitHub.
- Anyone who gets a copy from you must also get the licence and its `Required Notice:` line.

Releases v1.0 to v1.4 were published under the GNU General Public License v3.0 or later, and those copies carry that
licence. Third-party libraries, fonts and data in the firmware, and their licences, are listed in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## For developers

Building from source, the project layout, adding an app, tests, the website, the USB protocol and the hardware
details are in [docs/DEVELOPING.md](docs/DEVELOPING.md). Contributions are welcome under the terms in
[CONTRIBUTING.md](CONTRIBUTING.md). The board doesn't check firmware signatures, so any build you make will run
(`pio run -t upload`).
