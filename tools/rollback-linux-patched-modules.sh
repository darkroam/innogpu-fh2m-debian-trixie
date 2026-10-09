#!/usr/bin/env bash
# Remove r51 module overrides and restore the saved initramfs.
set -euo pipefail
PATH=/usr/sbin:/usr/bin:/sbin:/bin:$PATH

EVIDENCE_ROOT=""
while (($#)); do
    case $1 in
        --evidence) EVIDENCE_ROOT=${2:?missing value}; shift 2 ;;
        -h|--help) printf '%s\n' 'Usage: tools/rollback-linux-patched-modules.sh --evidence DIR'; exit 0 ;;
        *) printf 'FAIL: unknown option: %s\n' "$1" >&2; exit 2 ;;
    esac
done
[[ -n $EVIDENCE_ROOT && -r "$EVIDENCE_ROOT/result.txt" ]] || exit 2
EVIDENCE_ROOT=$(realpath "$EVIDENCE_ROOT")
KERNEL_RELEASE=$(awk -F= '$1 == "kernelrelease" {print $2}' "$EVIDENCE_ROOT/result.txt")
[[ -n $KERNEL_RELEASE ]] || exit 1
if [[ $(id -u) -ne 0 ]]; then
    exec sudo -- "$(realpath "$0")" --evidence "$(realpath "$EVIDENCE_ROOT")"
fi

MODULE_ROOT=/lib/modules/$KERNEL_RELEASE
BACKUP="$EVIDENCE_ROOT/install-backup"
[[ -d $BACKUP ]] || { printf 'FAIL: install backup missing\n' >&2; exit 1; }
exec > >(tee "$EVIDENCE_ROOT/rollback.log") 2>&1
rm -f "$MODULE_ROOT/updates/r51/battery.ko" "$MODULE_ROOT/updates/r51/xhci-pci.ko"
rmdir "$MODULE_ROOT/updates/r51" 2>/dev/null || true
cp -a "$BACKUP/initrd.img-$KERNEL_RELEASE.before" "/boot/initrd.img-$KERNEL_RELEASE"
depmod -a "$KERNEL_RELEASE"
printf 'ROLLBACK_PASS kernel=%s no_reboot=1 evidence=%s\n' "$KERNEL_RELEASE" "$EVIDENCE_ROOT"
