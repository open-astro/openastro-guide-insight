#!/usr/bin/env bash
#
# Check an AppImage starts: under a virtual display, it must print the
# version it was built as. That shows its bundled libraries load.
#
# usage: scripts/check-appimage.sh APP.AppImage VERSION
#
set -euo pipefail

app=$(realpath "${1:?usage: $0 APP.AppImage VERSION}")
version=${2:?usage: $0 APP.AppImage VERSION}

export APPIMAGE_EXTRACT_AND_RUN=1
got=$(xvfb-run -a "$app" --version)
want="OpenAstro Guide Insight $version"
if [[ $got != "$want" ]]; then
  echo "check-appimage: expected \"$want\", got \"$got\"" >&2
  exit 1
fi
echo "check-appimage: $(basename "$app") is good"
