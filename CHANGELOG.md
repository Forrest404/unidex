# Changes

What changed in each release, in short. The full notes are on the
[releases page](https://github.com/Forrest404/unidex/releases). Update from
https://forrest404.github.io/unidex/ (Update keeps your settings and files).

## v1.6.1 (9 October 2026)

- **Games:** less ghosting (each frame drives the screen five times longer, with the panel maker's own
  balanced waveform), they run at their proper speed whatever the screen takes, and the screen is crisp again
  after a game (it used to stay washed out until a restart).
- **Notes:** Open on phone moves into the note: open a note, then B.

## v1.6 (9 October 2026)

- **Low battery:** WiFi, the Pet's radio, Dex scans and recording wait until it's charged ("Battery too low"),
  and a flat battery shows "Charge me" and switches off instead of draining further.
- **Safer saving:** the calendar, badges, notes and the friends list are saved whole or not at all. A full or
  missing card no longer deletes the saved calendar, and a note that's already transcribed isn't lost when the card
  is full (it still goes to GitHub, or is shown once).
- **Never stuck:** if the device ever freezes it restarts by itself after 30 seconds.
- **Factory reset** in Settings > Reset data, for passing a device on: WiFi, keys, the Pet, friends and every
  setting (asks twice). Files on the SD card stay.
- **Welcome:** a new device shows the buttons and a QR code for the website once.
- Pet friends are also kept on the SD card, so they survive Reset > Everything and a reinstall.
- Timetable offers to set the clock even before there are events. Settings > Battery & info shows the device's
  name (the same as its Open on phone WiFi).
- Damaged files on the card can't run the device out of memory. The Dex says when it's full (2000 finds), and
  when new finds couldn't be saved (instead of counting them anyway).
- Fixes: confirm boxes (such as Reset everything) and a few screens no longer cut their text short.

## v1.5 (7 October 2026)

- **Pet**, a new app: a creature you dress up and name. **Meet** joins two devices held together into one room
  where the Pets greet, visit, swap screens and take trips; **friends** get their own greeting and a list; send a
  badge to a friend over the radio (they choose whether to keep it).
- unidex's own e-paper driver (slightly faster refreshes) and a new font, Liberation Sans.
- Licence: from v1.5, the PolyForm Noncommercial License 1.0.0 (v1.0 to v1.4 stay under the GPL).
- Website: a Pet designer on the Tools page.

## v1.4 (6 October 2026)

- One rule for the buttons everywhere (A next, B select, hold A back), hints on every screen, Settings as an app,
  a live line under each app on the home screen, a confirm before anything that can't be undone.
- Notes: back to the screen a second after recording (sending carries on in the background); silent recordings are
  dropped; **Open on phone** to read your notes on your phone.
- **Games:** Flappy, Dino, Stack and Jetpack, with best scores.
- Timetable live countdown and Today view; clearer Dex, Badge and Chooser screens; a real % while charging.
- Website: **Sync everything** (firmware, clock, calendar and notes in one click); eduroam certificate check.

## v1.3 (1 October 2026)

- **Notes**, a new app: hold B and talk; transcribed, tidied up, kept on the SD card and optionally sent to GitHub
  for Obsidian. Set up on the website's Notes page.
- Timetable: a next-up card and event details. Home becomes a one-app-at-a-time carousel.
- Files move to a micro SD card. Update keeps settings and keys; Install erases them.

## v1.2 (1 October 2026)

- Send badges straight to the device from the badge maker; a thumbnail picker.
- Restart with A + B held; stays awake on USB; the clock keeps time while asleep (clock chip fix).

## v1.1 (1 October 2026)

- Settings: date and time, sleep time, invert, battery and info, reset data.
- Time and battery on the home screen; a calibrated battery reading.

## v1.0 (1 October 2026)

- First release: Timetable, Name Badge, Dex and Chooser, with deep sleep between presses.
