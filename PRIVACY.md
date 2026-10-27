# Privacy

> **DRAFT, not reviewed.** This describes what the unidex firmware and website do with your data, as far as
> the code shows. It has not been checked by a lawyer and is not legal advice.

## In short

unidex has no account, no analytics and no tracking. Nothing is ever sent to whoever made or sold your device.
Only Notes, Reminders and the clock's time sync send anything over the internet (and apps from the store that you
allow WiFi), and Notes and Reminders only talk to services you set up with your own keys.

## Notes

When you record a note and the device has WiFi:

| What | Sent to | When |
|---|---|---|
| The recording (audio) | OpenAI (Whisper) or ElevenLabs (Scribe), whichever you chose, to transcribe it | every note |
| The transcript (up to 6000 characters) and the note's date and time (to make sense of "tomorrow") | OpenAI or Anthropic (Claude), to tidy it up | unless tidy-up is set to off |
| The finished note (text), with its title as the file name | your GitHub repo | only if GitHub is turned on |

These requests use **your own API keys and accounts**, so each service's own terms and privacy policy apply,
including how long they keep the data:

- OpenAI: https://openai.com/policies/privacy-policy/ and, for the API, https://openai.com/enterprise-privacy/
- ElevenLabs: https://elevenlabs.io/privacy-policy
- Anthropic: https://www.anthropic.com/legal/privacy and https://privacy.anthropic.com/
- GitHub: https://docs.github.com/en/site-policy/privacy-policies/github-general-privacy-statement

The **Test** buttons on the Setup page each make one request with your key (OpenAI and Anthropic: list the
available models; ElevenLabs: read your plan's details; GitHub: read the repo's details). No note content is sent.

If you record other people, tell them first: their voice goes to these services too.

## Reminders

When you say a reminder, the recording goes to the same transcriber as your notes (OpenAI or ElevenLabs), only to
turn it into words. Working out the to-do and its time happens on the device. The recording is saved on the SD card
first (in `reminders/`, where it waits if there's no WiFi) and deleted once it's written down. The reminders
themselves are kept on the card in `reminders.csv`.

## On the device

- **SD card:** recordings (`.wav`) and notes (`.md`), kept until you delete the note. Also your timetable, badges,
  calendar events and your apps' own files. The card is not encrypted: anyone who has it can read it.
- **The board's settings storage:** your WiFi name and password, API keys, GitHub token and settings. Keys are
  only sent to their own service. The Setup page never sees your passwords or whole keys: only whether each is set
  and the last 4 characters of each key. It does show plain settings such as the network name, eduroam username
  and repo.
- **WiFi:** the password goes to your network when the device logs in. On eduroam, your username and password go
  to your institution's login server. Without a CA certificate set on the Setup page, a fake network with the
  same name could capture them.
- Anyone who has the device and a USB cable can use your saved keys. If you lose it, revoke the keys at OpenAI,
  ElevenLabs, Anthropic and GitHub.

## Other features

- **Open on phone** (hold B in an open note) and **phone setup** (hold B in Settings) turn on the device's own WiFi
  network (named `unidex-` and 4 characters, with a new random password each time, shown on the screen and in its
  QR code). A phone that joins can read, search, play and copy your notes and recordings, change settings, and save
  WiFi networks and keys, straight from the device: nothing goes over the internet. Keys and passwords are never
  sent back to the page (a key shows as its last 4 characters). Anyone who can see the screen can join while it's
  on, so close it when you're done; it also switches off by itself after 5 minutes unused. Before it starts from
  Settings, the device looks for nearby WiFi names to suggest them; it keeps nothing of that scan. If WiFi is saved,
  it then joins it for the Apps tab: it downloads the app store's list from tools.unidex.shop (saved on the card),
  and the apps you choose to install. Nothing about you or your device is sent with those downloads.

- **Clock:** the time sync contacts `pool.ntp.org` or `time.google.com`, which see your IP address.
- **Apps** with the `wifi` permission can see the names of WiFi networks nearby (`unidex.wifi.scan`: name, signal
  and security, never a network's address). The Dex app from the store keeps the names it finds in its own folder
  on the SD card. Nothing is sent anywhere.
- **Calendar sync (Mac):** the Mac app reads your calendar on your Mac and sends events to the device over USB
  only.
- **Pet > Meet** talks straight to other unidex devices nearby over the radio (ESP-NOW: no internet, no router).
  Only while the Meet screen is open, it sends to any device in range, about 1-4 times a second: your Pet's look,
  its name (if you gave it one), a random Pet number made on your device (not linked to you or the device), and
  how strongly it hears the nearest other Pet. Each time Meet opens it uses a new random radio address, so the
  device can't be followed by its chip address. Once two devices are connected they also send which act to play.
  Nothing is sent when Meet is closed. If both of you choose to be friends, each device keeps the other Pet's random
  number, look and name, and how many times and when you met (up to 16 friends), on the device and in a copy on
  your SD card (`/pet/friends.bin`, so your friends survive a full reinstall). Remove one from the friends list, or
  all with Settings > Reset data > Pet friends (that clears the card's copy too).
- **Sending a badge** (to a friend, in Meet) sends that badge image to their device only. They see it first, and it
  is only saved on their SD card if they press keep; if they don't, it's gone from their device's memory. Badges can
  contain photos or names, so only send what you're happy for your friend to keep.

## The website

- It is hosted on GitHub Pages; GitHub may log visits (see GitHub's privacy statement above).
- The Install and Tools pages load code from `unpkg.com` and `cdn.jsdelivr.net`, which see your IP address.
- The Setup page loads nothing from other sites. It talks only to your device, over USB.
- The website sets no cookies.

## Deleting your data

- **A note:** delete it on the device or on the Setup page (removes the recording and the text from the card).
  Notes already on GitHub stay there until you delete them in your repo.
- **Keys and WiFi:** **Clear all keys** on the Setup page. It clears unidex's own settings only: anything left by
  other firmware the board ran before stays until a full erase.
- **Everything on the device:** Settings > Reset data > **Factory reset** (asks twice): your WiFi and keys, the
  Pet and its friends, every setting and the calendar. Notes, badges, apps and the timetable on the SD card stay:
  format the card on a computer too. A full erase (the website's **Install** button) also clears the board.
- **Data held by OpenAI, ElevenLabs, Anthropic or GitHub:** follow their own policies above.
