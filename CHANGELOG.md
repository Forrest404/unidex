# Changes

What changed in each release, in short. The full notes are on the
[releases page](https://github.com/Forrest404/unidex/releases). To update, use
https://tools.unidex.shop/ (Update keeps your settings and files).

## Next release

- **Reminders** (new): a to-do list you talk to. Hold B and say one ("call Sam tomorrow at 5pm", "take the cake out
  in 20 minutes", "dentist on the 3rd of November"). It's written down by the same service as your notes, with the
  key you already set up, and the device works out the to-do and its time by itself, with no AI. B ticks one off,
  and the last row clears the ticked ones. When one is due, the device wakes, chimes and shows it over whatever is
  on screen: B = done, A = again in 10 minutes. The home screen shows the next one ("5pm: Call Sam") or how many
  are late. Without WiFi the recording waits on the card and is added once WiFi is back (it tries every 10 minutes,
  or press B on its "waiting for WiFi" row), timed from when you said it. The recording is deleted once it's
  written down.
- **Notes:** listen back. In a note, B plays the recording on the speaker, even if it hasn't been sent yet, and any
  button stops it. Open on phone moves to hold B, and you delete notes from the list.
- **Notes:** a recording made without WiFi is kept ("Saved: sends on WiFi") and sends itself later. The device tries
  when you wake it and every 10 minutes while it's on, whatever app is open, and stays quiet if there's still no
  WiFi. B on the Notes screen still sends straight away.
- **Notes:** you can pick who transcribes on the Setup page: OpenAI Whisper, as before, or ElevenLabs Scribe, whose
  free plan covers about half an hour a month with no card needed. Each has its own key and Test button. ElevenLabs
  with the tidy-up off costs nothing: notes keep the plain transcript.
- **Recording screen** (Notes and Reminders): a waveform of your voice scrolls past instead of the level bar, and
  the time is in the header.
- **Settings > Volume:** the speaker's volume, from 0 (off) to 10. B is louder and hold B quieter, with a chime at
  each step so you can hear it.
- **Clock:** fixed the time not showing on boards where the clock chip's connection is powered from the audio
  supply. That supply was off except while recording, so the clock couldn't be read or set.
- **Website: Release or Beta.** Install and Update each have a Version menu: the newest release (the default) or
  Beta, the newest changes before they're released. Beta is rebuilt whenever it changes. Update back to Release
  the same way.

## v1.8 (15 October 2026)

- **New logo:** the start-up screen shows the new unidex logo, the robot above the name, instead of the four squares.
  The unidex badge in the starter files is the new logo too.
- **Chooser:** up to nine squares, in a 3×3 grid. The tally shows each square's number with its wins underneath,
  readable at every size.
- **Try unidex** (new): the emulator runs in your browser at
  [tools.unidex.shop/try.html](https://tools.unidex.shop/try.html), with your own app if you're
  writing one. Its Version menu opens the newest release by default, or an older one, or the next version in
  development.
- **Website:** a new design, with menus, a search (press /), a Help page, a Developers page, and a page for each app.
- **App store:** 15 new apps and plugins, including Dice, Weather, Focus, Flash Cards, Pet Rock and the Moon badge.
- **Writing apps:** step-by-step guides for apps and plugins, with templates, in
  [unidex-apps](https://github.com/Forrest404/unidex-apps).

## v1.7 (14 October 2026)

- **Apps** (new): small apps written in Lua, kept on the SD card and opened from Home > Apps. They run in
  a sandbox with their own memory and time limits, and an app that breaks shows "App error" instead of crashing
  the device. Hold B on one to offload it (its data stays) or delete it. Writing one: `docs/APPS.md`.
- **Website: Apps page.** See what is on your unidex, and install, update, offload or delete apps over USB. Delete
  says how much data will be lost first.
- **The app store:** anyone can send an app in as a pull request to
  [unidex-apps](https://github.com/Forrest404/unidex-apps); it's checked in the emulator and read by a person
  before it's listed.
- **Plugins** (first kind): a badge plugin draws a badge with code, such as Today, which always shows the date. Plugin
  badges are listed in Badge after your own. The Apps page lets go of the device after each action, so other pages
  and tools can use it.
- **Setup:** the website's Notes page is now called Setup (the old address still takes you there), and the device's
  hints say Setup too.
- **WiFi:** you can save up to five networks (home, eduroam or work, a phone hotspot). The device joins whichever is
  in range, trying the one that worked last first, then the others. The Setup page lists them with Edit and Remove.
  A second network needs this firmware; with older firmware the page keeps one.
- **Notes:** a recording where Whisper hears nothing is no longer deleted. It stays on the card as a note called
  "Heard nothing" (it isn't sent to GitHub or transcribed again), and you can delete it yourself.
- **Notes:** one note that can't be sent (a recording the service rejects, or a file GitHub refuses) no longer holds
  up the ones behind it. The rest sync, and the screen says how many are stuck. The check for an existing GitHub file
  now looks on the branch notes are pushed to.
- **Notes:** a second recording in the same wake no longer says "Not enough memory". The recording buffer is now
  kept instead of freed and allocated again each time.

## v1.6.1 (9 October 2026)

- **Games:** less ghosting, because each frame now drives the screen five times longer, using the panel maker's own
  balanced waveform. The games also run at their proper speed however long the screen takes, and the screen is
  crisp again after a game (it used to stay washed out until a restart).
- **Notes:** Open on phone has moved into the note. Open a note, then press B.

## v1.6 (9 October 2026)

- **Low battery:** WiFi, the Pet's radio, Dex scans and recording now wait until it's charged ("Battery too low").
  A flat battery shows "Charge me" and switches off instead of draining further.
- **Safer saving:** the calendar, badges, notes and the friends list are saved whole or not at all. A full or
  missing card no longer deletes the saved calendar, and a note that's already been transcribed isn't lost when the
  card is full (it still goes to GitHub, or is shown once).
- **Never stuck:** if the device ever freezes, it restarts by itself after 30 seconds.
- **Factory reset** in Settings > Reset data, for passing a device on. It wipes the WiFi, keys, the Pet, friends and
  every setting, asks twice, and leaves the files on the SD card alone.
- **Welcome:** a new device shows the buttons and a QR code for the website, once.
- Pet friends are also kept on the SD card, so they survive Reset > Everything and a reinstall.
- Timetable offers to set the clock even before there are events. Settings > Battery & info shows the device's
  name (the same as its Open on phone WiFi).
- Damaged files on the card can't run the device out of memory. The Dex says when it's full (2000 finds), and when
  new finds couldn't be saved, instead of counting them anyway.
- Fixes: confirm boxes (such as Reset everything) and a few screens no longer cut their text short.

## v1.5 (7 October 2026)

- **Pet**, a new app: a creature you dress up and name. **Meet** joins two devices held together into one room
  where the Pets greet, visit, swap screens and take trips. Friends get their own greeting and a list, and you can
  send a badge to a friend over the radio (they choose whether to keep it).
- unidex now has its own e-paper driver (slightly faster refreshes) and a new font, Liberation Sans.
- Licence: from v1.5 it's the PolyForm Noncommercial License 1.0.0 (v1.0 to v1.4 stay under the GPL).
- Website: a Pet designer on the Tools page.

## v1.4 (6 October 2026)

- One rule for the buttons everywhere (A next, B select, hold A back), with hints on every screen. Settings became
  an app, each app has a live line on the home screen, and anything that can't be undone asks first.
- Notes: you're back on the screen a second after recording (sending carries on in the background), silent
  recordings are dropped, and **Open on phone** lets you read your notes on your phone.
- **Games:** Flappy, Dino, Stack and Jetpack, with best scores.
- Timetable has a live countdown and a Today view. The Dex, Badge and Chooser screens are clearer, and the battery
  shows a real % while charging.
- Website: **Sync everything** (firmware, clock, calendar and notes in one click), and an eduroam certificate check.

## v1.3 (1 October 2026)

- **Notes**, a new app: hold B and talk, and the note is transcribed, tidied up, kept on the SD card and optionally
  sent to GitHub for Obsidian. You set it up on the website's Notes page.
- Timetable: a next-up card and event details. Home became a carousel that shows one app at a time.
- Files moved to a micro SD card. Update keeps settings and keys; Install erases them.

## v1.2 (1 October 2026)

- Send badges straight to the device from the badge maker, and pick them from a thumbnail picker.
- Hold A + B to restart. The device stays awake on USB, and the clock now keeps time while asleep (a clock chip fix).

## v1.1 (1 October 2026)

- Settings: date and time, sleep time, invert, battery and info, reset data.
- Time and battery on the home screen, with a calibrated battery reading.

## v1.0 (1 October 2026)

- The first release: Timetable, Name Badge, Dex and Chooser, with deep sleep between presses.
