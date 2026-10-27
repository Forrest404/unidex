# Writing apps for unidex

This needs unidex v1.7 or newer (Settings > Battery & info shows the version; the website's Update installs the
newest).

An unidex app is a small folder of Lua files on the SD card. You write it in a text editor, the website copies it
to your device over USB, and it shows up in the Apps list. No toolchain, no reflashing.

## What an app is

```
/apps/hello/
  app.json     who it is and what it needs
  main.lua     the code (more .lua files and small assets are fine)
  data/        its private files, made on the device (this is what a delete loses)
```

### app.json

```json
{
  "id": "hello",
  "name": "Hello",
  "version": "1.0.0",
  "author": "your name",
  "description": "Counts button presses.",
  "api": 1,
  "type": "app",
  "permissions": [],
  "license": "MIT"
}
```

| Field | Rule |
|---|---|
| `id` | Letters a-z, digits and `-`, up to 24 characters. Unique in the store. |
| `name` | Up to 20 characters, shown in the list. |
| `version` | `major.minor.patch`. The website offers an update when the store has a higher one. |
| `author`, `description` | Up to 40 and 140 characters. |
| `api` | The API level it was written for. The device refuses an app that needs a newer one. Level 2 adds `unidex.wifi.scan`, 3 `unidex.frame`, 4 games (v1.10), and 5 `unidex.chime` (the firmware after v1.10). |
| `type` | `"app"`, `"game"` (see Games), or `"plugin"` with `extends` (see Plugins). Both are at the end. |
| `frame_ms` | Games only, optional: how often `step()` runs, 48 to 1000 milliseconds (100 by default). |
| `permissions` | What it asks for. The website shows these before you install. See below. |
| `icon` | Optional. 40 strings of 40 characters, `#` is ink, anything else is blank. |
| `category` | Optional, for the store's filters: `games`, `tools`, `time`, `badges`, `learning`, `fun` or `other` (the default). An app in `games` is listed in Games on the device, not in Apps. |
| `license` | Optional on the device. The store may require one. |

## What your code defines

Everything is optional except `draw`. Define the ones you need as global functions. (A game defines four functions
of its own: see Games, at the end.)

| Function | When it runs | What to return |
|---|---|---|
| `onEnter()` | The app opens. | nothing |
| `draw()` | Whenever the screen needs painting. Paint the whole screen. | nothing |
| `onA()` | A pressed (the "next" button). | a redraw (below) |
| `onB()` | B pressed (the "select" button). | a redraw |
| `onBHold()` | B held. | a redraw |
| `onBack()` | A held. Without it, A held leaves the app. | `"exit"`, or a redraw to stay |
| `tick()` | About once a second while the app is open. | a redraw, or nothing |
| `status()` | The list wants a one-line summary. | a short string |
| `onExit()` | The app is closing. | nothing |

A **redraw** is `"partial"` (a quick refresh, the normal choice), `"full"` (flashes the whole screen, use it
sparingly) or `nil` (nothing changed). e-paper ghosts if you refresh too often, so the device refreshes the
screen at most once a second, except for animation frames (`unidex.frame`, level 3) and games (level 4), which use
the panel's fast mode.

The device sleeps a few seconds after the last press (Settings > Sleep after), and a sleep forgets everything in
memory. When it wakes in your app, the app starts again from the top and `onEnter()`: keep anything that matters in
your data files (the example below does). `unidex.wake_in(ms)` asks to be woken for a redraw while it naps.

The device keeps the usual rule everywhere: hold A goes back. An app can't take that away. Hold both buttons
for a second still restarts the device.

## What you can call

Everything lives in one table, `unidex`. There is no `io`, `os`, `require` of other files outside the app, or
`debug`: the app can only do what is listed here.

### Drawing

The screen is 200 x 200 pixels, black on white, origin top left. Keep content between y = 24 and y = 163: the
header sits above and the hints below.

| Call | What it does |
|---|---|
| `unidex.header(title, right)` | The title bar. `right` is optional (a time, a count). |
| `unidex.hints(a, b, bhold)` | The bottom line: what A, B and hold-B do. Every screen should have one. |
| `unidex.text(x, y, str, font, colour)` | Text with its baseline at `y`. `font`: `"tiny" "small" "medium" "large" "bold"` (default `"small"`); `colour`: `"black"` (default) or `"white"`, for text on a filled box. |
| `unidex.centered(y, str, font, colour)` | The same, centred. |
| `unidex.wrap(x, y, w, str, maxlines, font)` | Wrapped text; returns the lines used. |
| `unidex.width(str, font)` | Width in pixels. |
| `unidex.rect(x, y, w, h)` and `unidex.fill(x, y, w, h)` | An outline, a solid box. Add `"white"` as a last argument to erase. |
| `unidex.line(x1, y1, x2, y2)` | A line. |
| `unidex.qr(text, x, y, size)` | A QR code (size in pixels). |
| `unidex.icon(rows, x, y, scale)` | Draw a 40 x 40 icon in the `icon` format. |
| `unidex.bitmap(file, x, y)` | A 1-bit BMP from the app folder (up to 200 x 200). |

### Your private files

Only the app's own `data/` folder. Names use a-z, 0-9, `.`, `_` and `-`.

| Call | What it does |
|---|---|
| `unidex.data.read(name)` | The file's text, or `nil`. |
| `unidex.data.write(name, text)` | Saves it whole or not at all. Returns `true` or `false`. |
| `unidex.data.append(name, text)` | Adds to the end. |
| `unidex.data.list()` | A table of names. |
| `unidex.data.remove(name)` | Deletes a file. |

### The device

| Call | What it does |
|---|---|
| `unidex.time()` | A table: `year month day hour min sec wday`, and `valid` (false until the clock is set). |
| `unidex.battery()` | Percent, and whether it is charging. |
| `unidex.toast(text)` | A short message over the screen. |
| `unidex.wake_in(ms)` | Ask to be redrawn after this long (the device may be asleep until then). |
| `unidex.random(n)` | A number from 1 to `n`. |
| `unidex.frame(ms)` | API 3: shows `draw()` now, mid-function, as an animation frame, then waits `ms` (up to 1000). At most 60 a call; the screen and the wait don't count against the app's time. |
| `unidex.chime()` | API 5: plays the device's chime (about half a second) and returns true, or false if it was skipped: Settings > Alert sound is off, it already chimed less than a second ago, or the speaker is busy (a recording). At Volume 0 it plays silently. The chime doesn't count against the app's time. |
| `unidex.api` | The API level of this firmware: 5 after v1.10 (4 before `unidex.chime`, 3 before games, 2 before `unidex.frame`, 1 before `unidex.wifi`). |

### The network (permission `"wifi"`)

`unidex.http.get(url)` returns `status, body`. HTTPS only, up to 32 KB of reply, 15 seconds, and it blocks the
screen while it works. The device refuses it on a low battery. Apps never see the saved WiFi password or any
key, and can't ask for one.

`unidex.wifi.scan()` (API 2) lists the WiFi networks nearby, up to 64: a table of `{ssid=, rssi=, security=}`
(security: `open`, `WEP`, `WPA`, `WPA2`, `WPA2-E` or `WPA3`), hidden networks left out, one entry per access point
(a name can show more than once). Never a network's address. It takes about 2 seconds, which don't count against
the app's time, and returns `nil` and the reason on a low battery or while the radio is busy. Set `"api": 2` in
`app.json` to use it: older firmware then refuses the install with "needs newer firmware".

Other permission names (`"mic"`, `"meet"`) are reserved and do nothing yet.

## Limits

These stop one app from hanging or draining the device. They are enforced by the firmware.

| Limit | Value |
|---|---|
| A callback may run | about 2 seconds, then the app is stopped |
| Memory per app | about 512 KB |
| One file | 64 KB |
| App code and assets | 512 KB in all |
| Its `data/` folder | 512 KB in all |
| Apps on one device | 24 |

If your app breaks a limit or hits an error, the device shows "App error" with the line and a short message, and
returns to the app list. It never crashes the device.

## Install, offload and delete

You install from the store page on the website: plug the unidex in, open the page, choose an app. The site
copies the files over USB, and the app is only listed once everything has arrived, so a cable pulled halfway
leaves nothing broken behind.

Or from a phone, with no cable: in Settings, hold B, scan the code, and open the Apps tab. The device downloads the
store's files itself over its WiFi and checks each one's size and SHA-256 against the store's `index.json` before
installing it the same way.

- **Offload** removes the app but keeps its `data/`, so installing it again brings your files back.
- **Delete** removes the app and its data. The site and the device both warn you first, and say how much data
  would be lost.
- **Update** replaces the code and keeps `data/`.

## An example

`main.lua`:

```lua
local count = tonumber(unidex.data.read("count") or "0")

function draw()
  unidex.header("Hello")
  unidex.centered(80, tostring(count), "large")
  unidex.hints("", "add", "reset")
end

function onB()
  count = count + 1
  unidex.data.write("count", tostring(count))
  return "partial"
end

function onBHold()
  count = 0
  unidex.data.write("count", "0")
  return "partial"
end
```

## Trying it

- **Without a device:** open the emulator on the website, [Try unidex](https://tools.unidex.shop/try.html) (or build it yourself, see
  `emu/README.md`). Under "Try your app", choose your app's folder: it is
  installed the way the store installs it, you open it from Apps on the simulated screen, and what your code
  `print()`s shows beside it. Choose the folder again after a change.
- **On your unidex:** `python3 tools/appinstall.py install path/to/your-app` (plug it in and press a button first).
  With the test firmware (`pio run -e dev`), `print()` output comes out on USB as `LUA <id>: ...` lines.
- **Automatically:** `node emu/test/app_smoke.mjs path/to/your-app` opens it in the emulator, presses every button
  and says if it ever shows "App error". The store runs the same check on every submission.

## How an app gets into the store

1. Write it and try it (above).
2. Open a pull request to the store's registry,
   [Forrest404/unidex-apps](https://github.com/Forrest404/unidex-apps), adding your folder as `apps/<id>/`.
3. Automatic checks run on it: the manifest and the limits, the Lua syntax, and a run in the emulator that opens
   the app and presses every button (it fails on "App error"; the screenshots are attached to the run).
4. The store's owner reads the code and either accepts it (it is merged and appears in the store within about 6
   hours) or declines it with a reason, written on the pull request. You can fix it and open a new one.

Only accepted apps are ever listed. What the review looks for is written in the store repository's
[README](https://github.com/Forrest404/unidex-apps#what-gets-checked-by-hand).

## For the firmware, the USB commands (not for app authors)

| Command | Reply |
|---|---|
| `A LIST` | `A <id> <version or -> <code bytes> <data bytes> <ok or hex problem>` per app (`-`: offloaded), then `OK A end` |
| `A SPACE` | `OK A <apps> <free bytes>` |
| `A BEGIN <id>` | `OK A BEGIN`: starts an install in a staging folder (any unfinished one is cleared) |
| `A PUT <id> <path> <bytes> <crc32>`, then `D <hex>` lines | `OK A PUT`, a `K` per line, then `OK A FILE`, or `ERR` (a bad CRC or size drops the file) |
| `A DONE <id>` | `OK A DONE <id>` once the staged app passes its checks and is moved into place (`app.json` last, which makes it visible), or `ERR <hex reason>`; the old version's `data/` stays |
| `A DATA <id>` | `OK A DATA <bytes>`: the size the delete warning shows |
| `A RM <id> [keep]` | `OK A RM <id>`: `keep` is an offload; `ERR busy` while it's open on the device |

Paths may not contain `..` or start with `/`, and only `.lua .json .bmp .txt` files are accepted. The data
transfer is the same hex lines and `K` acknowledgements as badges.

## Plugins

A plugin adds to one of the built-in apps instead of being an app of its own. It is the same kind of folder, with
`"type": "plugin"` and `"extends"` naming the app, and it defines the function that app calls. It can use the same
`unidex.*` calls (and its own `data/` folder) as an app; it is started for each call and closed straight after, so
keep anything it needs to remember in its data files.

| `extends` | Your code defines | What happens |
|---|---|---|
| `"badge"` | `badge()`: draw the whole badge (200 x 200, no header or hints needed) | It is listed in Badge after the image badges, with a thumbnail. Each time it is shown, `badge()` runs, so it can show today's date, a countdown, a QR code... If it fails, Badge says so in its place. |

More built-in apps will take plugins later; each is added to this table first. `today-badge` in the
[store's repository](https://github.com/Forrest404/unidex-apps/tree/main/apps/today-badge) is an example.

```json
{ "id": "today-badge", "name": "Today", "version": "1.0.0", "api": 1, "type": "plugin", "extends": "badge" }
```

## Games

A game is played in Games, after Flappy, Dino, Stack and Jetpack, with B as its one button. It is the same kind of
folder as an app, with `"type": "game"`, `"category": "games"` and `"api": 4` (firmware after v1.9; older firmware
refuses it with "needs newer firmware"). It can't have `extends`. `frame_ms` sets how often `step()` runs.

| Function | When it runs | What to return |
|---|---|---|
| `start(seed)` | A new round. Call `math.randomseed(seed)` and use `math.random`, so the same seed replays the same round (the tests and the test tools rely on it). | nothing |
| `step(tapped, held)` | Every frame. `tapped`: B was pressed since the last frame (a quick tap between frames counts). `held`: B is down now. | `"over"` (the round ends), `"moved"` (redraw) or `"still"` (no redraw). Nothing counts as `"moved"`. |
| `draw()` | The play screen needs painting. Paint all of it; no header or hints. | nothing |
| `score()` | The score so far. | a whole number |

The rest of the API works as in an app (`unidex.data`, the drawing calls, `unidex.random`, `unidex.time`), and so
do the limits: a step should take milliseconds, not the 2 seconds a call may run.

The Games app does the rest. It lists the game with "best N", runs the frames on the panel's fast mode (about 7
a second, so keep drawing simple), ends a round when A is held (no score) or after 30 seconds without B (so the
device can sleep), shows the game-over screen (the score, "New best!" or the best, "B again"), keeps the best score
in the game's data file `best`, and shows "Best: <name> N" on the home screen. Hold B on anything from the store in
the list offers Offload and Delete, as in Apps. An error ends the round on "This game failed" with the reason.

Apps in the `games` category that aren't games (Dice, Coin & RPS, Dex) are listed in Games too, and open as normal
apps from there.

The guide, with a template (Hop, a block-hopping game), is
[Making games](https://github.com/Forrest404/unidex-apps/blob/main/GAMES.md) in the store's repository.
