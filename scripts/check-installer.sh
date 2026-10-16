#!/usr/bin/env bash
#
# Check a Windows installer made from openastro-guide-insight.iss: it
# installs silently, puts the app and its files in place, and uninstalls
# cleanly. Runs in Git Bash on a Windows CI runner.
#
# usage: scripts/check-installer.sh SETUP.exe
#
set -euo pipefail

setup=$(realpath "${1:?usage: $0 SETUP.exe}")
dir=$(mktemp -d)/app
fail() { echo "check-installer: $*" >&2; exit 1; }

"$setup" //VERYSILENT //SUPPRESSMSGBOXES //NORESTART //CURRENTUSER "//DIR=$(cygpath -w "$dir")"
for f in OpenAstroGuideInsight.exe LICENSE.txt README.md unins000.exe; do
  [[ -f $dir/$f ]] || fail "$f was not installed"
done

"$dir/unins000.exe" //VERYSILENT //SUPPRESSMSGBOXES //NORESTART
# the uninstaller finishes in the background; give it a moment
for _ in $(seq 30); do
  [[ -e $dir/OpenAstroGuideInsight.exe ]] || break
  sleep 1
done
[[ ! -e $dir/OpenAstroGuideInsight.exe ]] || fail "the app was not uninstalled"

echo "check-installer: $(basename "$setup") is good"
