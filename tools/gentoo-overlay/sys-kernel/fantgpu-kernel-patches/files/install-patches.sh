#!/bin/bash
# Copy 001-004 into portage user-patches. Same SHA skips. Different SHA does not overwrite.
set -euo pipefail
names=(
    001-hygon-148c-xhci-reset-on-resume.patch
    002-kaitian-x7h-battery-notification-delay.patch
    003-pinctrl-amd-hygon-gpio.patch
    004-snd-hda-hygon-14c9.patch
)
sha256() { sha256sum "$1" | awk '{print $1}'; }
src=${PATCH_SRC:-}
if [[ -z $src ]]; then
    [[ -n ${ROOT:-} ]] || { echo "gentoo_kernel_patches=FAIL no ROOT" >&2; exit 1; }
    src=$ROOT/tools/gentoo-overlay/sys-kernel/patches
    for name in "${names[@]}"; do
        [[ -f $src/$name ]] || { echo "gentoo_kernel_patches=FAIL missing $name" >&2; exit 1; }
        a=$(sha256 "$src/$name")
        b=$(sha256 "$ROOT/components/linux/$name")
        [[ $a == "$b" ]] || { echo "gentoo_kernel_patches=FAIL sha mismatch $name" >&2; exit 1; }
    done
fi
base=${DESTDIR:-}/etc/portage/patches/sys-kernel
for pkg in gentoo-sources gentoo-kernel; do
    for name in "${names[@]}"; do
        [[ -f $src/$name ]] || { echo "gentoo_kernel_patches=FAIL missing $name" >&2; exit 1; }
        dest=$base/$pkg/$name
        if [[ -f $dest ]] && [[ $(sha256 "$dest") != $(sha256 "$src/$name") ]]; then
            echo "gentoo_kernel_patches=FAIL sha differs $pkg/$name" >&2
            echo "not overwriting a local change; confirm before replacing" >&2
            exit 1
        fi
    done
done
installed=0
skipped=0
for pkg in gentoo-sources gentoo-kernel; do
    mkdir -p "$base/$pkg"
    for name in "${names[@]}"; do
        dest=$base/$pkg/$name
        if [[ -f $dest ]]; then
            skipped=$((skipped + 1))
            continue
        fi
        install -m 0644 "$src/$name" "$dest"
        installed=$((installed + 1))
    done
done
echo "gentoo_kernel_patches=INSTALLED installed=$installed skipped=$skipped dest=$base"
