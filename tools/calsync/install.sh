#!/bin/zsh
# Builds UnidexSync.app and runs it as a LaunchAgent. `./install.sh uninstall` removes it.
set -e
cd "$(dirname "$0")"
DIR="$HOME/Library/Application Support/UnidexSync"
APP="$DIR/UnidexSync.app"
LABEL=com.forrest.unidex-sync
PLIST="$HOME/Library/LaunchAgents/$LABEL.plist"

launchctl bootout "gui/$(id -u)/$LABEL" 2>/dev/null || true
if [[ "$1" == uninstall ]]; then
  rm -rf "$DIR" "$PLIST"
  echo "removed"
  exit 0
fi

mkdir -p "$APP/Contents/MacOS"
swiftc -O calsync.swift -o "$APP/Contents/MacOS/UnidexSync"
cp Info.plist "$APP/Contents/Info.plist"
codesign --force --sign - "$APP"   # ad-hoc: gives macOS a stable identity to grant Calendar access to

# Ask for Calendar access once, as an app, so macOS can show the prompt.
open -W "$APP" --args --print

cat > "$PLIST" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>Label</key><string>$LABEL</string>
  <key>ProgramArguments</key><array><string>$APP/Contents/MacOS/UnidexSync</string></array>
  <key>RunAtLoad</key><true/>
  <key>KeepAlive</key><true/>
  <key>StandardOutPath</key><string>$DIR/sync.log</string>
  <key>StandardErrorPath</key><string>$DIR/sync.log</string>
</dict>
</plist>
PLIST
launchctl bootstrap "gui/$(id -u)" "$PLIST"
echo "installed; log: $DIR/sync.log"
