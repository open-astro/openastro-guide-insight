#!/usr/bin/env bash
#
# Check a macOS DMG made by make-dmg.sh: it mounts, holds the app and an
# Applications link, and the app is arm64-only with a valid signature.
#
# usage: scripts/check-dmg.sh APP.dmg
#
set -euo pipefail

dmg=${1:?usage: $0 APP.dmg}
mnt=$(mktemp -d)
hdiutil attach -nobrowse -readonly -mountpoint "$mnt" "$dmg" >/dev/null
trap 'hdiutil detach -quiet "$mnt" || true; rmdir "$mnt" 2>/dev/null || true' EXIT

fail() { echo "check-dmg: $*" >&2; exit 1; }

app=$(find "$mnt" -maxdepth 1 -name '*.app' | head -1)
[[ -n $app ]] || fail "no app in the DMG"
[[ -L $mnt/Applications && $(readlink "$mnt/Applications") == /Applications ]] || fail "no Applications link"

exe="$app/Contents/MacOS/$(plutil -extract CFBundleExecutable raw "$app/Contents/Info.plist")"
[[ $(lipo -archs "$exe") == arm64 ]] || fail "not arm64-only: $(lipo -archs "$exe")"
codesign --verify --deep --strict "$app" || fail "the signature does not verify"

echo "check-dmg: $(basename "$dmg") is good"
