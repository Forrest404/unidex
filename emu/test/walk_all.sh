#!/bin/sh
# Runs every walkthrough against the emulator and lists the problems (some steps need hardware and will complain).
cd "$(dirname "$0")/../.."
for w in tools/walkthroughs/*.txt; do
  n=$(basename $w .txt)
  [ "$n" = before ] && continue
  echo "== $n"
  perl -e "alarm 240; exec @ARGV" ~/.platformio/penv/bin/python -u tools/devshot.py run $w --emu --out /tmp/emu-walk/$n 2>&1 | grep -E "PROBLEM|Error|Traceback|shots in|expected" | head -8
done
