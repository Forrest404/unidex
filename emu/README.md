# The browser emulator

The firmware's own apps (everything in `src/` except the hardware drivers) compiled to WebAssembly with Emscripten,
running behind a simulated screen and two buttons. Its page is `site/try.html`, published as
[Try unidex](https://forrest404.github.io/unidex/try.html).

## Versions

The page opens on the newest release, and its Version menu switches to any other release that has an emulator (v1.7
and newer: older releases draw straight to the panel, with no emulator hook) or to the next version, built from
main. The website workflow builds each of them with `emu/build_version.sh <tag> <folder>`: that release's `src/`,
`lib/` and libraries inside this checkout's `emu/`, so the page's scripts work with every version. It writes
`site/emu/<tag>/`, `site/emu/next/` and `site/emu/versions.json` (the list, with `latest`); a release's build is
cached and only made again when `emu/` changes. `?v=<tag>` in the address opens a version directly. Each build's
Settings shows its version, as `v1.7 (emulator)`.

## Build and run

You need Emscripten (emsdk 3.1.74), and the libraries PlatformIO fetches (`pio run -e dev` once).

```sh
EMSDK_ENV=~/emsdk/emsdk_env.sh bash emu/build.sh     # writes emu/out/ and site/emu/unidex.* (not committed)
python3 -m http.server 8000                        # from the repo folder; open http://localhost:8000/site/try.html
```

With no `site/emu/versions.json` the page runs that one build, `site/emu/unidex.js`, and hides the Version menu.

## How it works

- `emu/shim/` is a small stand-in for the Arduino core and the ESP-IDF headers the firmware includes: `String`,
  `Print`, `Serial`, files over the browser's memory file system (the "SD card" is `/sd`), NVS as a map, and empty
  radio, I2S and sleep headers.
- `emu/hw/` replaces the drivers that need real hardware: `power`, `battery`, `clock`, the network, the radio, the
  microphone and the Notes cloud and hotspot. They report "Not in the demo". `emu_main.cpp` is the runtime: time,
  the buttons, Serial, and the export the page calls.
- Everything else is compiled as it is, including `src/main.cpp`, the launcher, every app, `storage.cpp`,
  `input.cpp` (the debounce and long-press timing) and `usbsync.cpp`. `src/core/display.cpp` has one
  `#ifdef UNIDEX_EMU` where the SPI panel code would be: a refresh hands the buffer to the page.
- It is built with `UNIDEX_DEV`, so the test commands (`X BTN`, `X SHOT`, `X STATE`, `X DEMO`) work. `X DEMO 1`
  runs at start-up, so the page shows sample data and nothing from anyone's card.

## Trying an app

`site/try.html` has a "Try your app" box: choose an app's folder (docs/APPS.md) and it is installed on the
emulated device with the website's own install code (`site/store-device.js`); its `print()` output shows in the
box. `emu/test/app_smoke.mjs <folders>` does the same without a person and fails on "App error";
`emu/test/store_test.mjs` checks the store page's USB steps.

## Testing

`tools/devshot.py ... --emu` runs the walkthroughs in `tools/walkthroughs/` against the emulator (through
`emu/test/bridge.cjs`), and `sh emu/test/walk_all.sh` runs them all. Steps that need hardware (WiFi, the radio, the
microphone) report problems there; that is expected. `emu/check.sh` syntax-checks the sources against the shim.
