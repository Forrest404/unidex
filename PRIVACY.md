# Privacy Policy

Last updated: 28 October 2026

This explains what personal data unidex handles, where it goes, and your rights. It covers the unidex firmware,
the website at [tools.unidex.shop](https://tools.unidex.shop) and the app store. "I", "me" and "my" mean Forrest,
who makes unidex and is responsible for it (the "controller" under UK data protection law, for the data I handle).

Orders from [unidex.shop](https://unidex.shop) are covered by that shop's own privacy policy, not this one.

How you may use unidex is in the [Terms of Use](TERMS.md).

## In short

unidex has no account, no analytics and no tracking. **Nothing is ever sent to me** from your device or from the
website. Only Notes, Reminders, Clock's voice commands, the agent and the clock's time sync send anything over the
internet (and apps from the store that you allow WiFi), and Notes, Reminders, Clock and the agent only talk to
services you set up with your own keys.

## What I receive

Almost nothing:

- **On GitHub:** what you choose to post there: issues, comments, suggested changes, apps sent to the store, and
  private security reports. Everything except a security report is public. GitHub handles it under
  [its privacy statement](https://docs.github.com/en/site-policy/privacy-policies/github-general-privacy-statement).
- **Nothing else.** The website has no server of mine behind it, and the device never contacts me.

## What your device sends, and to whom

### Notes

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

### Reminders

When you say a reminder, the recording goes to the same transcriber as your notes (OpenAI or ElevenLabs), only to
turn it into words. Working out the to-do and its time happens on the device. The recording is saved on the SD card
first (in `reminders/`, where it waits if there's no WiFi) and deleted once it's written down. The reminders
themselves are kept on the card in `reminders.csv`.

### Clock by voice

When you hold B in Clock and say a command, the recording goes to the same transcriber, only to get the words. It's
kept in the device's memory until then and never saved. Working out the timer or alarm happens on the device.

### The agent

When you hold B on the home screen and ask the agent something:

| What | Sent to | When |
|---|---|---|
| The recording (audio) | OpenAI (Whisper) or ElevenLabs (Scribe), the same transcriber as your notes, to get the words | each request |
| The words, with what's on the device so it can answer and act: the date and time, your name (if set), your classes and events for the next 7 days, your open reminders (up to 10), your alarms, timer and stopwatch, and your settings | Anthropic (Claude) if you've set an Anthropic key, otherwise OpenAI | each request |

The recording is kept in the device's memory until then and never saved. The model only answers; what it asks for
is checked and done on the device. The same keys, terms and links as Notes apply.

### The time

The clock's time sync contacts `pool.ntp.org` or `time.google.com`, which see your IP address.

## On the device

- **SD card:** recordings (`.wav`) and notes (`.md`), kept until you delete the note. Also your timetable, badges,
  calendar events and your apps' own files. The card is not encrypted: anyone who has it can read it.
- **The board's settings storage:** your WiFi name and password, API keys, GitHub token and settings, and the name
  you tell your Pet. Keys are only sent to their own service. The Setup page never sees your passwords or whole
  keys: only whether each is set and the last 4 characters of each key. It does show plain settings such as the
  network name, eduroam username and repo.
- **WiFi:** the password goes to your network when the device logs in. On eduroam, your username and password go
  to your institution's login server. Without a CA certificate set on the Setup page, a fake network with the
  same name could capture them.
- Anyone who has the device and a USB cable can use your saved keys. If you lose it, revoke the keys at OpenAI,
  ElevenLabs, Anthropic and GitHub.

## Other features

- **Open on phone** (hold B in an open note) and **phone setup** (hold B in Settings) turn on the device's own WiFi
  network (named `unidex-` and 4 characters, or the name you gave it, with a new random password each time, shown
  on the screen and in its QR code). A phone that joins can read, search, play, copy and delete your notes and
  recordings, change settings, and save WiFi networks and keys, straight from the device: nothing goes over the
  internet. Keys and passwords are never sent back to the page (a key shows as its last 4 characters). Anyone who
  can see the screen can join while it's on, so close it when you're done; it also switches off by itself after 5
  minutes unused. Before it starts from Settings, the device looks for nearby WiFi names to suggest them; it keeps
  nothing of that scan. If WiFi is saved, it then joins it for the Apps tab: it downloads the app store's list from
  tools.unidex.shop (saved on the card), and the apps you choose to install. Nothing about you or your device is
  sent with those downloads.
- **Apps** with the `wifi` permission can see the names of WiFi networks nearby (`unidex.wifi.scan`: name, signal
  and security, never a network's address). The Dex app from the store keeps the names it finds in its own folder
  on the SD card. Nothing is sent anywhere.
- **Calendar sync (Mac):** the Mac app reads your calendar on your Mac and sends events to the device over USB
  only.
- **Pet > Meet** talks straight to other unidex devices nearby over the radio (ESP-NOW: no internet, no router).
  Only while the Meet screen is open, it sends to any device in range, about 1-4 times a second: your Pet's look,
  its name (if you gave it one), a random Pet number made on your device (not linked to you or the device), and
  how strongly it hears the nearest other Pet. Your own name is never sent. Each time Meet opens it uses a new
  random radio address, so the device can't be followed by its chip address. Once two devices are connected they
  also send which act to play. Nothing is sent when Meet is closed. If both of you choose to be friends, each
  device keeps the other Pet's random number, look and name, and how many times and when you met (up to 16
  friends), on the device and in a copy on your SD card (`/pet/friends.bin`, so your friends survive a full
  reinstall). Remove one from the friends list, or all with Settings > Reset data > Pet friends (that clears the
  card's copy too).
- **Sending a badge** (to a friend, in Meet) sends that badge image to their device only. They see it first, and it
  is only saved on their SD card if they press keep; if they don't, it's gone from their device's memory. Badges can
  contain photos or names, so only send what you're happy for your friend to keep.

## The website

- It is hosted on GitHub Pages. GitHub may log visits, including your IP address (see GitHub's privacy statement
  above).
- The Install and Tools pages load code from `unpkg.com` and `cdn.jsdelivr.net`, which see your IP address.
- The Setup page loads nothing from other sites. It talks only to your device, over USB.
- The App store and the Try page load the store's apps from tools.unidex.shop itself.

### Cookies and browser storage

The website sets **no cookies**. A few pages keep things in your own browser, for you, and never send them
anywhere:

| Page | What it keeps | Why |
|---|---|---|
| App editor | the app you're writing | so it's still there when you come back |
| Tools | whether automatic sync is on, and which calendar file you picked | so it can sync the same file next time |
| Phone portal (on the device) | that the "slide to delete" hint was shown | so it only shows once per visit |

You can remove them by clearing this site's data in your browser's settings. As these are only for features you
asked for, no consent banner is needed for them.

## The legal side

### Why I may handle your data (lawful basis)

| What | Why | Basis under UK GDPR |
|---|---|---|
| What you post on GitHub (issues, changes, apps, security reports) | to answer you, fix problems and run the app store | legitimate interests: running and improving unidex |
| Visit logs kept by GitHub Pages and the code sites above | to deliver the website and keep it secure | legitimate interests: running the website safely |

What your device does with your notes, recordings and settings is your own use of your own device, and the
services you connect handle what they receive as your provider, under your account with them.

### How long

I don't keep personal data of my own. What you post on GitHub stays there until you or I delete it. You can edit
or delete your own issues and comments at any time.

### Outside the UK

GitHub, OpenAI, Anthropic and ElevenLabs are based in, or store data in, the United States and other countries
outside the UK. Data you send them through unidex goes there under each company's own safeguards for international
transfers, described in their privacy policies above.

### Children

unidex is for people aged 13 and over. Younger children need a parent or guardian to agree to the
[Terms of Use](TERMS.md) and to set it up with them. The services Notes uses have age rules of their own, so check
those before connecting them for a child.

### Your rights

Under UK and EU data protection law you can ask me to:

- tell you what personal data I hold about you, and give you a copy;
- correct it;
- delete it;
- limit how I use it, or object to my using it;
- give it to you in a form you can pass on.

Most of what I could hold is your own GitHub posts, which you can change or delete yourself. For anything else,
open an issue on [GitHub](https://github.com/Forrest404/unidex/issues) saying you have a privacy request, without
the details, and I'll give you a private way to send them. I'll answer within one month.

For data held by OpenAI, ElevenLabs, Anthropic or GitHub, ask them: their policies above say how.

### Complaints

If you're not happy with how I've handled your data, you can complain to the UK's Information Commissioner's Office
(ICO): [ico.org.uk](https://ico.org.uk), 0303 123 1113, or Wycliffe House, Water Lane, Wilmslow, Cheshire SK9 5AF.
If you live in the European Union, you can also complain to the data protection authority where you live.

## Deleting your data

- **A note:** delete it on the device, on the Setup page, or from the phone portal (removes the recording and the
  text from the card). Notes already on GitHub stay there until you delete them in your repo.
- **Keys and WiFi:** **Clear all keys** on the Setup page. It clears unidex's own settings only: anything left by
  other firmware the board ran before stays until a full erase.
- **Everything on the device:** Settings > Reset data > **Factory reset** (asks twice): your WiFi and keys, the
  Pet and its friends, every setting and the calendar. Notes, badges, apps and the timetable on the SD card stay:
  format the card on a computer too. A full erase (the website's **Install** button) also clears the board.
- **Data held by OpenAI, ElevenLabs, Anthropic or GitHub:** follow their own policies above.

## Changes to this policy

When unidex starts handling data in a new way, this policy changes first. The date at the top says when it last
changed, and the full history is on [GitHub](https://github.com/Forrest404/unidex/commits/main/PRIVACY.md).

## Contact

Open an issue on [GitHub](https://github.com/Forrest404/unidex/issues), or for anything private, see above.
