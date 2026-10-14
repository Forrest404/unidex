# Writing apps for unidex

DRAFT: the device side (the Apps app, this API, the USB commands) and the website's Apps page are built; the review
of submitted apps is next. `tools/appinstall.py` installs an app folder over USB, for trying your own.

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
| `api` | The API level it was written for. The device refuses an app that needs a newer one. This guide is level 1. |
| `type` | `"app"`. (`"plugin"` is planned: see the end.) |
| `permissions` | What it asks for. The website shows these before you install. See below. |
| `icon` | Optional. 40 strings of 40 characters, `#` is ink, anything else is blank. |
| `license` | Optional on the device. The store may require one. |

## What your code defines

Everything is optional except `draw`. Define the ones you need as global functions.

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
screen at most once a second unless the app is in its animation mode (not in level 1).

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
| `unidex.text(x, y, str, font)` | Text with its baseline at `y`. `font`: `"tiny" "small" "medium" "large" "bold"` (default `"small"`). |
| `unidex.centered(y, str, font)` | The same, centred. |
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
| `unidex.api` | The API level of this firmware. |

### The network (permission `"wifi"`)

`unidex.http.get(url)` returns `status, body`. HTTPS only, up to 32 KB of reply, 15 seconds, and it blocks the
screen while it works. The device refuses it on a low battery. Apps never see the saved WiFi password or any
key, and can't ask for one.

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

## How an app gets into the store

1. Write it and try it on your unidex (copy the folder to `/apps/` on the card, or install it from the store page
   in developer mode).
2. Open a pull request to the store's registry repository with your folder.
3. Automatic checks run on it: the manifest, the limits, and a test that it starts and draws.
4. The store's owner reads the code and either accepts it (it is merged and appears in the store) or declines it
   with a reason, written on the pull request. You can fix it and open it again.

Only accepted apps are ever listed. Everything about the store's rules (what is accepted, licence and terms
for submissions) will be written here before submissions open.

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

## Plugins (planned)

A plugin is an add-on for one of the built-in apps (for example a Notes action or a Pet outfit). It uses the
same folder, with `"type": "plugin"` and `"extends": "notes"`, and defines hook functions the built-in app calls
at named moments. It gets only the data that hook passes in. The first hooks will be written down here before
they are built.
