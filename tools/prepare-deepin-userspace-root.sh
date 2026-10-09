#!/bin/bash
# Prepare the Deepin 202504 userspace root used for DDX/GL installation.

set -euo pipefail

ROOT="${INNOGPU_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
if [[ -z "${INNOGPU_DEEPIN_DEB:-}" ]]; then
    for candidate in \
        "$ROOT/debs/innogpu-fh2m_20250421190503-debug_amd64.deb" \
        "$ROOT/innogpu-fh2m_20250421190503-debug_amd64.deb"; do
        if [[ -f "$candidate" ]]; then
            INNOGPU_DEEPIN_DEB="$candidate"
            break
        fi
    done
fi
DEEPIN_DEB="${INNOGPU_DEEPIN_DEB:-}"
OUT_ROOT="${INNOGPU_DEEPIN_ROOT:-$ROOT/third_party/innogpu-fh2m-deepin-202504/root}"

if [[ -d "$OUT_ROOT/usr" ]]; then
    printf '%s\n' "$OUT_ROOT"
    exit 0
fi

if [[ ! -f "$DEEPIN_DEB" ]]; then
    echo "ERROR: missing Deepin userspace deb: $DEEPIN_DEB" >&2
    echo "Set INNOGPU_DEEPIN_DEB or put the release artifact in $ROOT/debs/." >&2
    exit 1
fi

command -v dpkg-deb >/dev/null 2>&1 || {
    echo "ERROR: missing dpkg-deb; install dpkg-dev/dpkg first" >&2
    exit 1
}

echo "Extracting Deepin userspace deb to $OUT_ROOT ..." >&2
rm -rf "$OUT_ROOT"
mkdir -p "$OUT_ROOT"
dpkg-deb -x "$DEEPIN_DEB" "$OUT_ROOT"

printf '%s\n' "$OUT_ROOT"
