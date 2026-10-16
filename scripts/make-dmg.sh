#!/usr/bin/env bash
#
# Make the macOS disk image: the app, ad-hoc signed so it launches on Apple
# Silicon, beside a link to /Applications to drag it onto.
#
# usage: scripts/make-dmg.sh "OpenAstro Guide Insight.app" OUTPUT.dmg
#
set -euo pipefail

app=${1:?usage: $0 APP OUTPUT.dmg}
out=${2:?usage: $0 APP OUTPUT.dmg}
name=$(basename "$app" .app)

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

cp -R "$app" "$work/"
# Developer ID signing and notarization come later (#14); "-" signs ad hoc.
codesign --force --deep --sign - "$work/$(basename "$app")"
ln -s /Applications "$work/Applications"

rm -f "$out"
hdiutil create -quiet -volname "$name" -srcfolder "$work" -fs HFS+ -format UDZO "$out"
