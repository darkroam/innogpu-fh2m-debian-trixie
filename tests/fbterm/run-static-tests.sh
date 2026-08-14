#!/bin/bash

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_SCRIPT="$ROOT/scripts/build-patched-fbterm.sh"
PATCH_FILE="$ROOT/patches/fbterm/001-configurable-redraw-scrolling.patch"

bash -n "$BUILD_SCRIPT"
"$BUILD_SCRIPT" --help >/dev/null

grep -Fq 'getOption("scrolling"' "$PATCH_FILE"
grep -Fq 'mOffsetCur = 0' "$PATCH_FILE"
grep -Fq -- '--scrolling=MODE' "$PATCH_FILE"

echo "PASS_FBTERM_REDRAW_STATIC"
