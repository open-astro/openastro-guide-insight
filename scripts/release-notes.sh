#!/usr/bin/env bash
#
# Print a version's notes from CHANGELOG.md: the lines under its "## VERSION"
# heading, without the heading or surrounding blank lines. Fails if the
# changelog has no such version.
#
# usage: scripts/release-notes.sh VERSION
#
set -euo pipefail

version=${1:?usage: $0 VERSION}
changelog="$(cd "$(dirname "$0")/.." && pwd)/CHANGELOG.md"

notes=$(awk -v v="$version" '
  /^## / { if (found) exit; if ($2 == v) { found = 1; next } }
  found { print }
' "$changelog" | sed -e '/./,$!d' | sed -e ':a' -e '/^\n*$/{$d;N;ba' -e '}')

if [[ -z $notes ]]; then
  echo "release-notes: no notes for $version in CHANGELOG.md" >&2
  exit 1
fi
printf '%s\n' "$notes"
