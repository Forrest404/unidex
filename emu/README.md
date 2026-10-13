# The browser emulator

The firmware's own apps (everything in `src/` except the hardware drivers) compiled to WebAssembly with Emscripten,
running behind a simulated screen and two buttons. The page is `site/try.html`.

## Build and run

You need Emscripten (emsdk 3.1.74), and the libraries PlatformIO fetches (`pio run -e dev` once).

```sh
EMSDK_ENV=~/emsdk/emsdk_env.sh bash emu/build.sh     # writes emu/out/ and site/emu/ (neither is committed)
cd site && python3 -m http.server 8000             # then open http://localhost:8000/try.html
```

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

## Testing

`tools/devshot.py ... --emu` runs the walkthroughs in `tools/walkthroughs/` against the emulator (through
`emu/test/bridge.cjs`), and `sh emu/test/walk_all.sh` runs them all. Steps that need hardware (WiFi, the radio, the
microphone) report problems there; that is expected. `emu/check.sh` syntax-checks the sources against the shim.
