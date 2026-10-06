# Privacy

> **DRAFT, not reviewed.** This describes what the unidex firmware and website do with your data, as far as
> the code shows. It has not been checked by a lawyer and is not legal advice.

## In short

unidex has no account, no analytics and no tracking. Nothing is ever sent to whoever made or sold your device.
Only the Notes app and the clock's time sync send anything over the internet, and Notes only talks to services you
set up with your own keys.

## Notes

When you record a note and the device has WiFi:

| What | Sent to | When |
|---|---|---|
| The recording (audio) | OpenAI, to transcribe it (Whisper) | every note |
| The transcript (up to 6000 characters) and the note's date and time (to make sense of "tomorrow") | OpenAI or Anthropic (Claude), to tidy it up | unless tidy-up is set to off |
| The finished note (text), with its title as the file name | your GitHub repo | only if GitHub is turned on |

These requests use **your own API keys and accounts**, so each service's own terms and privacy policy apply,
including how long they keep the data:

- OpenAI: https://openai.com/policies/privacy-policy/ and, for the API, https://openai.com/enterprise-privacy/
- Anthropic: https://www.anthropic.com/legal/privacy and https://privacy.anthropic.com/
- GitHub: https://docs.github.com/en/site-policy/privacy-policies/github-general-privacy-statement

The **Test** buttons on the Notes page each make one request with your key (OpenAI and Anthropic: list the
available models; GitHub: read the repo's details). No note content is sent.

If you record other people, tell them first: their voice goes to these services too.

## On the device

- **SD card:** recordings (`.wav`) and notes (`.md`), kept until you delete the note. Also your timetable, badges,
  calendar events and the Dex list. The card is not encrypted: anyone who has it can read it.
- **The board's settings storage:** your WiFi name and password, API keys, GitHub token and settings. Keys are
  only sent to their own service. The Notes page never sees your passwords or whole keys: only whether each is set
  and the last 4 characters of each key. It does show plain settings such as the network name, eduroam username
  and repo.
- **WiFi:** the password goes to your network when the device logs in. On eduroam, your username and password go
  to your institution's login server. Without a CA certificate set on the Notes page, a fake network with the
  same name could capture them.
- Anyone who has the device and a USB cable can use your saved keys. If you lose it, revoke the keys at OpenAI,
  Anthropic and GitHub.

## Other features

- **Clock:** the time sync contacts `pool.ntp.org` or `time.google.com`, which see your IP address.
- **Dex** scans for nearby WiFi networks and keeps network names and salted, hashed router IDs on the SD card.
  Nothing is sent anywhere.
- **Calendar sync (Mac):** the Mac app reads your calendar on your Mac and sends events to the device over USB
  only.
- **Pet > Meet** talks straight to other unidex devices nearby over the radio (ESP-NOW: no internet, no router).
  Only while the Meet screen is open, it sends to any device in range, about 1-4 times a second: your Pet's look,
  its name (if you gave it one), a random Pet number made on your device (not linked to you or the device), and
  how strongly it hears the nearest other Pet. Each time Meet opens it uses a new random radio address, so the
  device can't be followed by its chip address. Once two devices are connected they also send which act to play.
  Nothing is sent when Meet is closed. If both of you choose to be friends, each device keeps the other Pet's random
  number, look and name, and how many times and when you met, on the device only (up to 16 friends; remove one
  from the friends list, or all with Settings > Reset > Everything).

## The website

- It is hosted on GitHub Pages; GitHub may log visits (see GitHub's privacy statement above).
- The Install and Tools pages load code from `unpkg.com` and `cdn.jsdelivr.net`, which see your IP address.
- The Notes page loads nothing from other sites. It talks only to your device, over USB.
- The website sets no cookies.

## Deleting your data

- **A note:** delete it on the device or on the Notes page (removes the recording and the text from the card).
  Notes already on GitHub stay there until you delete them in your repo.
- **Keys and WiFi:** **Clear all keys** on the Notes page. It clears unidex's own settings only: anything left by
  other firmware the board ran before stays until a full erase.
- **Everything on the device:** a full erase (the website's **Install** button), and format the SD card.
- **Data held by OpenAI, Anthropic or GitHub:** follow their own policies above.
