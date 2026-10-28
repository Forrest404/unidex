# Terms of Use

Last updated: 28 October 2026

These terms cover the unidex website at [tools.unidex.shop](https://tools.unidex.shop) (the installer, Tools,
Setup, Apps, the App editor, the Try page and the rest of it), the unidex firmware, and the app store. In these
terms, "unidex" means all of those, and "I", "me" and "my" mean Forrest, who makes it.

Buying a unidex at [unidex.shop](https://unidex.shop) is covered by that shop's own terms, not these.

By using unidex you agree to these terms. If you don't agree, please don't use it.

## 1. Who can use it

You need to be at least 13. If you're younger, a parent or guardian has to agree to these terms for you and help
you set it up, especially the parts that send your recordings to other services (see [section 5](#5-other-peoples-services)).

## 2. The software and its licence

The unidex code is published under the [PolyForm Noncommercial License 1.0.0](LICENSE). That licence, not these
terms, says what you may do with the code itself: in short, use it, change it and share it, but not commercially.
Parts of unidex come from other people under their own licences, listed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

The website is free to use for your own, non-commercial purposes. Please don't:

- try to break into it, overload it, or get round its security;
- copy it to pass it off as your own, or sell access to it;
- use it to spread malware, including through the app store.

The name unidex and its logo are mine. You can use them to talk about unidex, but not in a way that suggests I made
or support something I didn't.

## 3. Installing firmware

The Install and Update buttons write firmware to your board over USB. **Install** erases the board first, and
**Update** keeps your settings and files. Keep the cable in until it says it's done. A flash that's cut off
partway can leave the board needing to be put into its download mode and installed again (see
[Help](https://tools.unidex.shop/help.html)).

Beta firmware is newer and less tested than a release. Use it knowing it may have bugs.

## 4. Your device and your data

What you record, write and save on a unidex stays on it (on the SD card and in the board's settings) unless you set
up a service that it sends things to. Keep your own copies of anything that matters: an SD card can fail, and a
factory reset or a full Install erases things on purpose.

**Don't rely on unidex for anything where a missed alarm, timer or reminder could cause harm**, such as taking
medicine or getting somewhere safe. A flat battery, a crash or a wrong setting can stop it ringing.

## 5. Other people's services

Voice notes, reminders, Clock's voice commands and the GitHub sync use services you choose and set up with your own
accounts and API keys: OpenAI, ElevenLabs, Anthropic and GitHub. A calendar link is fetched from wherever you got it.

- Your use of those services is between you and them, under their terms. They may charge you for what you use.
- You're responsible for your keys. If you lose your device, revoke them at each service.
- unidex works with these services as they are now. If one changes or stops, that part of unidex may stop working.

The [Privacy Policy](PRIVACY.md) explains exactly what goes to each one.

## 6. Recording people

Only record people who know you're recording them and are happy for you to. Recording someone without their
knowledge can be against the law, and their voice goes to the transcription service you chose, too. You're
responsible for what you record.

## 7. The app store and apps

Apps in the store are written by other people. Each one is checked by automatic tests and read by a person before
it's listed, but I can't promise that any app works, is safe or is suitable for you. Apps run in a sandbox on the
device, away from your keys, passwords and notes (see [the app guide](docs/APPS.md)).

- Each app has its own licence, shown in the store.
- I can remove an app from the store at any time, for example if it breaks, is harmful, or breaks these terms.
- If you send an app to the store, you confirm it's yours to share under the licence you give it, and that it
  doesn't break anyone's rights or the law. The rules for submitting are in the store's repository,
  [unidex-apps](https://github.com/Forrest404/unidex-apps).

## 8. Pet, Meet and sending badges

Meet talks to other unidex devices nearby over radio. Be kind: don't use a Pet's name or a badge to bully,
harass or impersonate anyone, and only send a badge you're happy for the other person to keep.

## 9. Ideas and contributions

If you open an issue, suggest something or send a change on GitHub, I can use your idea in unidex without paying
you or naming you. Code you contribute is covered by [CONTRIBUTING.md](CONTRIBUTING.md) and the project's licence.

## 10. Changes, and no promises of support

I may change, pause or stop any part of unidex at any time, including the website, the store and older firmware
versions on the Try page. I'll try to keep things working and to answer issues, but I don't promise support.

## 11. Warranties and liability

unidex's software and website are free and made by one person, and are provided "as is". I don't promise they'll
always be available, free of bugs, or right for what you need.

As far as the law allows, I'm not responsible for:

- lost or damaged data, notes, recordings or settings;
- missed alarms, timers or reminders;
- charges from the services you connect (see [section 5](#5-other-peoples-services));
- anything an app from the store does;
- losses I couldn't reasonably have foreseen, or business losses (unidex is for personal use).

Nothing in these terms limits liability that the law doesn't allow to be limited, such as for death or personal
injury caused by negligence, or for fraud, and nothing affects your legal rights as a consumer. Your rights as a
buyer of a device are covered by the shop's terms and by consumer law, not by this section.

## 12. If you break these terms

If you break these terms, for example by attacking the website or putting harmful apps in the store, I may stop you
using the website or the store. You can stop using unidex at any time. Sections 2, 6, 7, 9 and 11 still apply
afterwards.

## 13. The law

These terms are governed by the law of England and Wales, and disputes go to the courts of England and Wales. If
you live in Scotland or Northern Ireland, you can also bring a claim in your own courts. If you live in the
European Union, you keep the protections of the law where you live.

## 14. Changes to these terms

I may update these terms. The date at the top says when they last changed, and the full history is on
[GitHub](https://github.com/Forrest404/unidex/commits/main/TERMS.md). If you keep using unidex after a change, the
new terms apply. If a change matters a lot, I'll say so in the [changelog](CHANGELOG.md) and on the website.

## 15. Contact

Questions about these terms: open an issue on [GitHub](https://github.com/Forrest404/unidex/issues). For anything
private, such as a security problem, use [private reporting](SECURITY.md) instead of a public issue.
