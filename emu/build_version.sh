#!/bin/bash
# Builds the browser emulator for one released version: that release's firmware code (src/, lib/, its libraries)
# inside this checkout's emulator (emu/shim, emu/hw, emu/build.sh), so the Try it page's scripts work with every
# version. Used by the website workflow for the version list on site/try.html.
#   bash emu/build_version.sh v1.7 out/dir             writes out/dir/unidex.js and unidex.wasm; fails if it can't build
#   bash emu/build_version.sh HEAD out/dir v1.7-12-gabc  any commit; the last argument is the version Settings shows
# Needs emcc (source emsdk_env.sh first, or set EMSDK_ENV) and PlatformIO (pio) for the release's libraries.
set -e
tag="$1"; out="$2"; name="${3:-$1}"
[ -n "$tag" ] && [ -n "$out" ] || { echo "usage: build_version.sh <tag> <output folder>"; exit 2; }
here="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$out"; out="$(cd "$out" && pwd)"
work="$(mktemp -d)"
trap 'git -C "$here" worktree remove --force "$work/src" >/dev/null 2>&1 || true; rm -rf "$work"' EXIT

git -C "$here" worktree add --detach "$work/src" "$tag" >/dev/null 2>&1
cd "$work/src"
rm -rf emu && cp -R "$here/emu" emu && rm -rf emu/out
printf '#pragma once\n#define UNIDEX_VERSION "%s (emulator)"\n' "$name" > emu/shim/unidex_version.h
command -v pio >/dev/null || PATH="$HOME/.platformio/penv/bin:$PATH"
pio pkg install -e epaper154 >/dev/null  # the release's own library versions
bash emu/build.sh >/dev/null
cp emu/out/unidex.js emu/out/unidex.wasm "$out/"
echo "built $name"
