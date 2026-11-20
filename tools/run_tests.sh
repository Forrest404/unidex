#!/bin/sh
# Runs every computer test: tools/*test/*_test.cpp, each built with the command in its first lines, and
# tools/*test/*_test.mjs with Node (they check the website's own code).
# Usage: tools/run_tests.sh   (from the repo root; CI runs it on every push)
set -u
out="${TMPDIR:-/tmp}/unidex-tests"
mkdir -p "$out"
failed=0
for f in tools/*test/*_test.cpp; do
  cmd=$(grep -m1 '^//   c++ ' "$f" | sed 's|^//   ||; s|/tmp/|'"$out"'/|g')
  name=$(basename "$f" .cpp)
  case "$cmd" in *"&& $out/$name"*) ;; *) cmd="$cmd && $out/$name" ;; esac
  if sh -c "$cmd" > "$out/$name.log" 2>&1; then
    echo "ok    $f"
  else
    echo "FAIL  $f"
    tail -20 "$out/$name.log"
    failed=1
  fi
done
for f in tools/*test/*_test.mjs; do
  name=$(basename "$f" .mjs)
  if node "$f" > "$out/$name.log" 2>&1; then
    echo "ok    $f"
  else
    echo "FAIL  $f"
    grep -v '^ok' "$out/$name.log" | tail -20
    failed=1
  fi
done
exit $failed
