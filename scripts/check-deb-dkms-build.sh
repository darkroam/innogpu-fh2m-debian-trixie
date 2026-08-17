#!/bin/bash
# Compile the DKMS source contained in a candidate deb without installing it.

set -euo pipefail

PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
export PATH

ROOT="${INNOGPU_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
DEB=${1:-}
KERNEL=${2:-$(uname -r)}

if [[ -z "$DEB" || $# -gt 2 ]]; then
    echo "Usage: scripts/check-deb-dkms-build.sh DEB [KERNEL]" >&2
    exit 2
fi
[[ -f "$DEB" ]] || { echo "ERROR: package not found: $DEB" >&2; exit 1; }
DEB=$(realpath "$DEB")
[[ -d "/lib/modules/$KERNEL/build" ]] || {
    echo "ERROR: kernel headers are missing: /lib/modules/$KERNEL/build" >&2
    exit 1
}
"$ROOT/scripts/check-release-package.sh" "$DEB" >/dev/null

runtime=$(mktemp -d "${TMPDIR:-/tmp}/innogpu-dkms-build.XXXXXX")
trap 'rm -rf "$runtime"' EXIT HUP INT TERM
dpkg-deb -x "$DEB" "$runtime/root"

source_dir="$runtime/root/usr/src/innogpu-kernel-2.2"
[[ -f "$source_dir/Makefile" ]] || {
    echo "ERROR: package does not contain the expected DKMS source" >&2
    exit 1
}

jobs=$(nproc)
(( jobs > 16 )) && jobs=16
if ! make -C "$source_dir" -j "$jobs" \
    KERNELDIR="/lib/modules/$KERNEL/build" >"$runtime/build.log" 2>&1; then
    cat "$runtime/build.log" >&2
    echo "ERROR: offline module build failed" >&2
    exit 1
fi

module="$source_dir/innogpu.ko"
[[ -s "$module" ]] || { echo "ERROR: build did not produce innogpu.ko" >&2; exit 1; }
module_name=$(modinfo -F name "$module")
vermagic=$(modinfo -F vermagic "$module")
[[ "$module_name" == "innogpu" ]] || {
    echo "ERROR: unexpected module name: $module_name" >&2
    exit 1
}
[[ "$vermagic" == "$KERNEL "* ]] || {
    echo "ERROR: module vermagic does not match $KERNEL: $vermagic" >&2
    exit 1
}

warnings=$(grep -c 'warning:' "$runtime/build.log" || true)
echo "RESULT: PASS_OFFLINE_DKMS_BUILD kernel=$KERNEL warnings=$warnings vermagic=$vermagic"
