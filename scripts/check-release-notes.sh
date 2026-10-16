#!/usr/bin/env bash
# Checks scripts/release-notes.sh against the real changelog.
set -euo pipefail
cd "$(dirname "$0")/.."
notes=$(scripts/release-notes.sh 0.6.4)
expected='- Fix problem with log section grid scrollbars sometimes missing
- Fix location of right-click popup menu'
[[ $notes == "$expected" ]] || { echo "unexpected notes for 0.6.4:"; echo "$notes"; exit 1; }
if scripts/release-notes.sh 9.9.9 >/dev/null 2>&1; then
  echo "a version not in the changelog should fail"; exit 1
fi
echo "release-notes: ok"
