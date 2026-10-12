#!/bin/bash
# Copy the overlay kernel patches into portage user-patches. Bytes stay identical to components/linux.
set -euo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
SRC=$ROOT/tools/gentoo-overlay/sys-kernel/patches
BASE=${DESTDIR:-}/etc/portage/patches/sys-kernel
names=(
    001-hygon-148c-xhci-reset-on-resume.patch
    002-kaitian-x7h-battery-notification-delay.patch
    003-pinctrl-amd-hygon-gpio.patch
    004-snd-hda-hygon-14c9.patch
)
sha256() { sha256sum "$1" | awk '{print $1}'; }
for name in "${names[@]}"; do
    [[ -f $SRC/$name ]] || { echo "gentoo_kernel_patches=FAIL missing $name" >&2; exit 1; }
    a=$(sha256 "$SRC/$name")
    b=$(sha256 "$ROOT/components/linux/$name")
    [[ $a == "$b" ]] || { echo "gentoo_kernel_patches=FAIL sha mismatch $name" >&2; exit 1; }
done
for pkg in gentoo-sources gentoo-kernel; do
    mkdir -p "$BASE/$pkg"
    for name in "${names[@]}"; do
        install -m 0644 "$SRC/$name" "$BASE/$pkg/$name"
    done
done
echo "gentoo_kernel_patches=INSTALLED dest=$BASE"
