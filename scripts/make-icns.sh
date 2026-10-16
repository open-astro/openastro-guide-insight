#!/usr/bin/env bash
#
# Make a macOS .icns icon from one square PNG, with every size the iconset
# needs. Sizes larger than the source are scaled up; give a 1024x1024 PNG
# for the sharpest result.
#
# usage: scripts/make-icns.sh SOURCE.png OUTPUT.icns
#
set -euo pipefail

src=${1:?usage: $0 SOURCE.png OUTPUT.icns}
out=${2:?usage: $0 SOURCE.png OUTPUT.icns}

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
iconset="$work/icon.iconset"
mkdir "$iconset"

for size in 16 32 128 256 512; do
  sips -z $size $size "$src" --out "$iconset/icon_${size}x${size}.png" >/dev/null
  sips -z $((size * 2)) $((size * 2)) "$src" --out "$iconset/icon_${size}x${size}@2x.png" >/dev/null
done

iconutil -c icns "$iconset" -o "$out"
