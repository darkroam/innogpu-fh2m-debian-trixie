#!/bin/bash
# Confirm 001-005 bytes match components/linux, and whether the kernel tree already contains them.
set -euo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
SRC=$ROOT/tools/gentoo/overlay/sys-kernel/patches
LINUX=${KERNEL_TREE:-/usr/src/linux}
files_only=0
[[ ${1:-} == --files-only ]] && files_only=1
names=(
    001-hygon-148c-xhci-reset-on-resume.patch
    002-kaitian-x7h-battery-notification-delay.patch
    003-pinctrl-amd-hygon-gpio.patch
    004-snd-hda-hygon-14c9.patch
    005-drm-display-hdmi-helper-prompt.patch
)
declare -A expect=(
    [001-hygon-148c-xhci-reset-on-resume.patch]=299eb3f3d688a48518278c1ac5c6e97be89c8cc8f2c50bdcbfc4881bd47a5614
    [002-kaitian-x7h-battery-notification-delay.patch]=d7d6740eaab078eeaa40d87306c4d77fc61436f50ef31061416f0d1689899e1a
    [003-pinctrl-amd-hygon-gpio.patch]=2f6676f38175a709bb71488de5caa7e8490bf75e17d3a4e21c2b0f4b286ced0e
    [004-snd-hda-hygon-14c9.patch]=08dc3079d5b6d8d698afe321b957ae17ec410f269ab379ab1673c67e1891a229
    [005-drm-display-hdmi-helper-prompt.patch]=f2a8ad5f767177d6ffae02ba048dd8b9f9df1424f29ea8ddab27c91f65a581af
)
sha256() { sha256sum "$1" | awk '{print $1}'; }
fail=0
for name in "${names[@]}"; do
    got=$(sha256 "$ROOT/components/linux/$name")
    [[ $got == "${expect[$name]}" ]] || { echo "kernel_patch_sha=FAIL components $name"; fail=1; }
    got=$(sha256 "$SRC/$name")
    [[ $got == "${expect[$name]}" ]] || { echo "kernel_patch_sha=FAIL overlay $name"; fail=1; }
done
(( fail == 0 )) && echo kernel_patch_sha=PASS
BASE=${PORTAGE_PATCH_BASE:-/etc/portage/patches/sys-kernel}
for pkg in gentoo-sources gentoo-kernel; do
    dir=$BASE/$pkg
    if [[ ! -d $dir ]]; then
        echo "portage_patches=ABSENT pkg=$pkg"
        fail=1
        continue
    fi
    for name in "${names[@]}"; do
        if [[ ! -f $dir/$name ]] || [[ $(sha256 "$dir/$name") != $(sha256 "$SRC/$name") ]]; then
            echo "portage_patches=FAIL pkg=$pkg file=$name"
            fail=1
        fi
    done
done
(( fail == 0 )) && echo portage_patches=PASS
if (( files_only == 0 )); then
    if [[ -f $LINUX/drivers/usb/host/xhci-pci.c ]] \
        && rg -q "PCI_VENDOR_ID_HYGON" "$LINUX/drivers/usb/host/xhci-pci.c" \
        && rg -q "KaiTian" "$LINUX/drivers/acpi/battery.c" \
        && rg -q "HYGO0030" "$LINUX/drivers/pinctrl/pinctrl-amd.c" \
        && rg -q "PCI_VDEVICE\(HYGON, 0x14c9\)" "$LINUX/sound/pci/hda/hda_intel.c" \
        && rg -q 'bool "DRM HDMI display helpers"' "$LINUX/drivers/gpu/drm/display/Kconfig"; then
        echo "kernel_tree=APPLIED tree=$LINUX"
    else
        echo "kernel_tree=NOT_APPLIED tree=$LINUX"
        fail=1
    fi
fi
exit "$fail"
