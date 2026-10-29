# Changes

What changed in each release, in short. The full notes are on the
[releases page](https://github.com/Forrest404/unidex/releases). To update, use
https://tools.unidex.shop/ (Update keeps your settings and files).

## Next release

- **Website: Legacy.** Install and Update's Version menu has a third choice, Legacy, for going back to one of the
  15 releases before the newest (v1.6 to v1.12 now: the ones before have no files to install), if something broke. Update keeps your settings and files, and
  Update with Release brings you back.

- **Pet > Meet: catch and tug of war** (new, hold B in the room). Catch: press B as the ball reaches your Pet to
  throw it back, faster each time; three misses in a row end it, and your best rally with each friend shows in the
  friends list. Tug of war: press B as fast as you can to pull the knot your way, first over or ahead after 15
  seconds wins. Hold A stops either game. Both devices need this version.

## v1.13 (28 October 2026)

- **Clock** (new, built in, after Reminders): a timer (presets from 1 minute to 2 hours), up to three alarms (once,
  every day, weekdays or weekends, with snooze), a stopwatch with laps, and the time in up to four of 24 cities.
  Alarms and timers wake the device and ring over whatever's on screen, even with Alert sound off.
- **Clock by voice** (new): hold B on Clock's list and say it: "set a timer for 8 minutes", "set an alarm for
  5pm", "wake me up at 7 every weekday", "stop the timer", "turn off my alarms", "start the stopwatch". Needs WiFi
  and a transcription key, and English.
- **Ringtones** (new): Settings > Ringtone picks the sound of every chime (alarms, the timer, reminders and apps):
  Chime (as before), Bell, Rise, Beeps or Ripple. B plays each as you go.
- **The Pet knows you** (new): give it your name (Dress up > Your name, or the Pet card on your phone) and it greets
  you by it. It talks about your day in a speech bubble, from your timetable, calendar and reminders: the next
  class within the hour ("Maths in 15 min, B12", excited when it's close), the one on now ("In Maths till 8:35"),
  a reminder that's late or due within 2 hours, good morning, and when you're free ("Free till tomorrow 9:00").
  It sleeps from 22:00 to 07:00. The first four also show on its home screen line.
- **The Pet** stays full size while it talks, with the bubble beside it. Press A again soon after it says hi and it
  giggles, then gets dizzy. It dresses for the season (a scarf in winter, sunglasses in summer, a witch's hat at
  Halloween, a Santa hat at Christmas), and reacts to your day: a backpack before class, a wave during one,
  confetti when the day's done. It notices what you did in other apps ("Nice one!" for a ticked-off reminder, a new
  best in Games, "Got it!" for a saved note), and now and then says it misses a friend you haven't met in a week.
- **Pet > Meet:** friends stay friends on both devices, and "Be friends?" shows on both. Friends who meet again
  wave, hug from the 5th meeting and do a secret handshake from the 10th; at night (22:00-07:00) they have a
  sleepover. Hold B and choose High five (or the room starts one now and then): press B together to slap hands. The
  new acts need both devices on v1.13.
- **Apps:** `unidex.climate()` (API 6) reads the board's temperature and humidity sensor. Thanks to @candour.
- **Timetable:** once every event its home line knew about has ended, it reads the card again (at most every 15
  minutes), so the line no longer stays on "Nothing coming up" until you open it.
- **Website:** a [Terms of Use](TERMS.md), and the [Privacy Policy](PRIVACY.md) rewritten, linked from every page.
  The Pet designer on the Tools page can send your name too.

## v1.12 (27 October 2026)

- **Setup page:** a Language choice on the Transcription card, as in the device's Settings and the phone portal.
  The device is the same as v1.11.

## v1.11 (27 October 2026)

- **Notes in other languages:** notes are transcribed and tidied up in the language you speak; the title, summary
  and note are never translated into English (a French note used to get an English summary). Auto (the default)
  works the language out; set yours (16 languages) in Settings > Note language or the Setup tab on your phone when it
  guesses wrong. On the screen, accented Latin letters (Polish, Czech, Turkish...) lose their marks instead of
  showing "?", and a note in a script the screen can't show (Arabic, Chinese, Greek...) says to read it on your
  phone, with the time it was recorded as its title.
- **Apps: `unidex.chime()`** (API level 5) plays the device's chime, so an app such as a timer can make a sound. It
  follows Settings > Alert sound, and chimes at most once a second.
- **Phone portal from Settings:** with none of your WiFi networks in range, the QR code shows straight away instead
  of after up to 45 seconds of trying to join.

## v1.10 (27 October 2026)

- **Games from the store** (new): games anyone can make now play in Games, after the four built in, with their
  best scores, the game-over screen and the home line, like the built-in ones. A game is four Lua functions
  (start, step, draw, score; API level 4): see GAMES.md in the app store's repo, the "A game" starter in the App
  editor, and Hop, the first one in the store. Store apps in the games category (Dex, Dice, Coin & RPS) are listed
  in Games too, not Apps. Hold B on one from the store there to offload or delete it.
- **Chooser moves to the app store**, like Dex: install it from the store (Apps). Your tally is kept: the update
  moves it into the app's folder, so Apps lists an offloaded "chooser" until you install it. It spins the same; it
  lives in Apps now (no home screen line), and Settings > Reset data no longer has a tally row (the app clears its
  own).
- **Apps: `unidex.frame(ms)`** (API level 3) shows the screen straight away, mid-function, so apps can animate (the
  Chooser spin uses it). The store says which API level each app needs.
- **Dex moves to the app store.** The WiFi network collecting game no longer comes built in: install it from the
  store on the website (Apps). Your finds are kept: the update moves them into the app's folder, so Apps lists an
  offloaded "dex" until you install it, and it carries on with them. It plays the same, but it lives in Apps now (no
  home screen line) and is cleared from inside it or with Delete in Apps; Settings > Reset data no longer has a Dex
  row. It keeps network names only, no longer a hashed address per find.
- **Apps: `unidex.wifi.scan()`** (API level 2, with the `wifi` permission) lists the WiFi networks nearby: name,
  signal and security, never their addresses. Apps that use it need this update.
- **Badge:** delete a badge. In the picker, hold B on it and press B to confirm, or tap Delete under it in the phone
  portal's Tools tab (it asks first there too). A plugin's badge goes with the plugin, in Apps.
- **Phone portal: delete notes** (new): Delete at the bottom of an open note (tap twice), or slide a note left in
  the list. A note that's sending can't be deleted until it's done.
- **Name your unidex** (new): in the portal's Settings. Its WiFi takes the name from the next start (up to 20
  characters); without one it's `unidex-` and 4 characters, as before.
- **Phone portal: Apps** (new): a fifth tab with the app store. Tap Install and the device downloads the app
  from the store itself, over your saved WiFi, checking each file's size and SHA-256 against the store's list; your
  phone stays on the device's own WiFi meanwhile (the device joins your WiFi first and puts its own on the same
  channel). It also lists the apps on the device (folded away until you tap), with Update and Delete, and the store has a
  search box. With no WiFi, it shows the store's list
  from last time.
- **Phone portal: Tools** (new): a fourth tab with the badge maker and the Pet designer. Choose a photo (or take
  one), pick Fit or Fill, Photo or Line art, the lightness and invert, and send it: it's saved on the card as the next
  badge and the Badge app opens on it. The Pet designer loads your Pet as it is, lets you change each part and its
  name (or Surprise me), and sends it.

- **Notes:** without the clock set, a new note could overwrite an older one after a reset (the numbering started
  again at 1 while the notes stayed on the card). Taken numbers are now skipped.
- **Notes:** with more than 255 notes, the later ones couldn't be reached in the list (it went back to the top).
- **Saving:** a power cut can't lose a file when something is later added to it (the backup copy is put back first,
  whatever the file is opened for), and the one-time copy from internal memory to the card (boards updated from
  before the SD card) is checked before it counts, and tried again if it didn't work.
- **Notes:** a recording made just as the previous one finished sending could wait until the next sync (up to 10
  minutes) before it was sent. It now goes straight away.
- **Timetable:** a long timetable.csv no longer pushes this week's calendar events out (each file has its own
  room: 128 classes, 64 events); a class ending at midnight (00:00, 24:00, or after it) shows, cut at midnight; a
  name or room in quotes can hold a comma (`"Maths, advanced"`); and a time that isn't one (`ab:cd`) is skipped
  instead of read as 00:00.
- **Clock:** a time sent over USB that isn't a number, or is outside 2024 to 2099, is refused (it used to set the
  clock to 1970 and a nonsense year on the clock chip).
- **Calendar:** a garbled calendar transfer over USB (no numbers, a negative count) is refused instead of being taken
  as an empty week, which replaced the saved calendar with nothing.
- **Phone portal:** the website's Sync, Test and microphone check leave the phone hotspot alone while it's on
  (they used to switch the radio over and end it), and say so.
## v1.9 (27 October 2026)

- **Set up from your phone** (new): in Settings, hold B. The device shows a QR code; your phone joins its WiFi and
  a page opens with Notes, Settings (sleep, volume, invert, the time) and Setup (WiFi networks, with the ones nearby
  to pick from, keys, the tidy-up, GitHub, the calendar link). No computer or cable needed, and nothing goes over
  the internet. Press Done when you're finished: if you changed WiFi or keys, the device checks each one and lists
  how it went. Open on phone (hold B in a note) opens the same page on its Notes tab. A new device's first screens
  offer it too: Set it up shows the QR code for your phone, with the website's address underneath for a computer.
- **Settings > Sync** (new): sets the clock from the internet and fetches this week's events from your calendar's
  private link over WiFi, with no computer. Save the link on the Setup page's new Calendar card (it says where
  Google, Outlook and iCloud keep it); its Test button says how many events it found. Repeating, moved and
  cancelled events and other time zones come out as they do on the Tools page. The clock also sets on networks that
  block the usual time servers.
- **Settings > Alert sound:** turn it off and a reminder falling due still pops up, without the chime.
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
- **Notes:** fixed a note being sent to GitHub twice (or more) when the card filled up or went missing between the
  recording and its text. The device now remembers it's on GitHub, writes the text to the card once there's room,
  and doesn't transcribe it again.
- **Notes:** Open on phone says "Battery too low" on a low battery, like everything else that uses WiFi.
- **Notes:** a long line on the Notes screen ("No SD card: GitHub only", "1 waiting: sends on WiFi", an error) goes
  on two lines instead of being cut off.
- **Timetable:** a long class's length is shortened ("2h 45m") when it wouldn't fit next to the time. A place written
  on two lines in your calendar ("Charrington Street" / "London") keeps a space between them, from the Mac app and
  the Tools page (reinstall the Mac app: `tools/calsync/install.sh`).
- **Notes:** playing a note starts about half a second sooner, and the time and bar move on as each second is heard
  instead of falling behind.
- **Reminders:** setting the clock back (by hand, or putting a fast clock right) no longer silences the reminders due
  before the old time: they come up when their time comes.
- **Apps:** an app stopped for running too long names the line that was looping, not the line that called it. The
  home screen's count of apps is right straight after one is added or removed from the website.
- **Battery:** the % while charging carries on through a restart instead of being worked out again from the
  voltage, which made it jump.
- **Notes:** an API key saved in the tidy-up's Model box no longer breaks the tidy-up ("OpenAI: error 404" after
  every note, with no summary). The device ignores it and uses the default model, and the Setup page won't save
  one. A model name the service doesn't know now says "unknown model".
- **Website: Release or Beta.** Install and Update each have a Version menu: the newest release (the default) or
  Beta, the newest changes before they're released. Beta is rebuilt whenever it changes. Update back to Release
  the same way.
- **Website: simpler to get around.** The top bar is six plain links (Install, Tools, Setup, Apps, Learn, Help), one
  click each, with Learn to code and GitHub always in reach. On a phone the menu lists every page with its
  sections. Each place goes by one name everywhere, and outside links open in a new tab.
- **Website: Apps page.** Apps are shown as boxes in a grid (three across on a computer, one per row on a phone),
  each with its icon, what it does, its tags, its size and Install, instead of one long list.
- **Website: smaller fixes.** Code to paste has a Copy button, and the Learn page's examples say which file they are.
  In the App editor on a phone, the unidex sits between the code and the output, so Run shows it straight away. The
  menu button no longer falls off the smallest phones. Also: Install and Update line up, two-line choices fill
  their button, the sliders and phone filters are easier to tap, Clear all keys is red, the Run key hint is
  readable, and the footer links sit in even columns on a phone.

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
