#!/bin/sh
# Syntax-checks the firmware sources the browser build uses, to see what is still missing.
cd "$(dirname "$0")/.."
. ${EMSDK_ENV:-/private/tmp/emsdk/emsdk_env.sh} >/dev/null 2>&1
LIB=.pio/libdeps/dev
GFX="$LIB/Adafruit GFX Library"
SRC=$(git ls-files 'src/*.cpp' | grep -vE '^src/core/(audio|battery|clock|link|net|power)\.cpp$|^src/apps/notes/(cloud|job|phone|usb)\.cpp$|^src/apps/scripts/appfetch\.cpp$')
for f in $SRC emu/hw/*.cpp; do
  out=$(em++ -std=gnu++17 -fsyntax-only -DUNIDEX_EMU=1 -DARDUINO=100 -DUNIDEX_DEV=1 -DDEBUG=0 -Iemu/shim -Isrc -Isrc/core \
     -I"$GFX" -I$LIB/ArduinoJson/src -I$LIB/QRCode/src "$f" 2>&1 | grep -E "error" | head -4)
  [ -n "$out" ] && printf '%s\n%s\n' "== $f" "$out"
done
