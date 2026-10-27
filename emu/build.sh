#!/bin/bash
# Builds the browser emulator: the firmware's own apps compiled to WebAssembly with Emscripten.
# Output: emu/out/unidex.js and unidex.wasm. Needs emcc (source emsdk_env.sh first, or set EMSDK_ENV).
set -e
cd "$(dirname "$0")/.."
# emcc must be on the PATH (CI installs it); locally, EMSDK_ENV points at emsdk_env.sh
[ -n "$EMSDK_ENV" ] && . "$EMSDK_ENV" >/dev/null 2>&1
command -v emcc >/dev/null || { echo "emcc not found: install Emscripten (emsdk) and source emsdk_env.sh"; exit 1; }
LIB=.pio/libdeps/dev
[ -d "$LIB" ] || LIB=.pio/libdeps/epaper154
[ -d "$LIB" ] || { echo "run 'pio run -e dev' once first (it fetches the libraries)"; exit 1; }
GFX="$LIB/Adafruit GFX Library"
mkdir -p emu/out
SRC=$(git ls-files 'src/*.cpp' | grep -vE '^src/core/(audio|battery|clock|link|net|power)\.cpp$|^src/apps/notes/(cloud|job|phone|usb)\.cpp$|^src/apps/scripts/appfetch\.cpp$')
emcc -O2 -c "$LIB/QRCode/src/qrcode.c" -o emu/out/qrcode.o
LUA=$(ls lib/lua/src/*.c 2>/dev/null || true)  # Lua (for apps) is in v1.7 and newer; emu/build_version.sh builds older ones
for f in $LUA; do emcc -O2 -DLUAI_MAXCCALLS=120 -c "$f" -o "emu/out/lua-$(basename "$f" .c).o"; done
em++ -std=gnu++17 -O2 -DUNIDEX_EMU=1 -DARDUINO=100 -DUNIDEX_DEV=1 -DDEBUG=0 \
  -Iemu/shim -Isrc -Isrc/core -Ilib/lua/src -I"$GFX" -I$LIB/ArduinoJson/src -I$LIB/QRCode/src \
  $SRC emu/hw/*.cpp "$GFX/Adafruit_GFX.cpp" emu/out/qrcode.o $(ls emu/out/lua-*.o 2>/dev/null) \
  -sASYNCIFY -sALLOW_MEMORY_GROWTH -sMODULARIZE -sEXPORT_NAME=createUnidex -sEXPORTED_RUNTIME_METHODS=ccall,cwrap,HEAPU8,UTF8ToString \
  -o emu/out/unidex.js

# The Try it page (site/try.html) loads it from site/emu/ (not committed: built from the source, as the firmware is).
mkdir -p site/emu
cp emu/out/unidex.js emu/out/unidex.wasm site/emu/
