#!/bin/bash
# Read-only check for the Hygon 1d94:148c reset-on-resume quirk.

set -euo pipefail

SYSFS_ROOT=/sys
KERNEL_LOG=

usage() {
    echo "Usage: $0 [--sysfs-root DIR] [--kernel-log FILE]" >&2
    exit 2
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --sysfs-root) [[ $# -ge 2 ]] || usage; SYSFS_ROOT=$2; shift 2 ;;
        --kernel-log) [[ $# -ge 2 ]] || usage; KERNEL_LOG=$2; shift 2 ;;
        -h|--help) usage ;;
        *) usage ;;
    esac
done

tmp=
if [[ -z "$KERNEL_LOG" ]]; then
    tmp=$(mktemp)
    trap 'rm -f "$tmp"' EXIT
    journalctl -k -b -o cat --no-pager >"$tmp" 2>/dev/null || dmesg >"$tmp" 2>/dev/null || true
    KERNEL_LOG=$tmp
fi

found=0
failed=0
unverified=0
for pci in "$SYSFS_ROOT"/bus/pci/devices/*; do
    [[ -d "$pci" ]] || continue
    [[ "$(cat "$pci/vendor" 2>/dev/null || true)" == "0x1d94" ]] || continue
    [[ "$(cat "$pci/device" 2>/dev/null || true)" == "0x148c" ]] || continue
    found=1
    bdf=${pci##*/}
    line=$(grep -E "xhci_hcd ${bdf//./\\.}: .* quirks 0x[[:xdigit:]]+" "$KERNEL_LOG" | tail -n 1 || true)
    if [[ -z "$line" ]]; then
        printf 'hygon_xhci_resume_fix=UNVERIFIED device=%s reason=boot_quirk_log_missing\n' "$bdf"
        unverified=1
        continue
    fi
    hex=$(sed -n 's/.* quirks 0x\([[:xdigit:]]\+\).*/\1/p' <<<"$line")
    if (( (16#$hex & 0x80) != 0 )); then
        printf 'hygon_xhci_resume_fix=PASS device=%s quirks=0x%s\n' "$bdf" "$hex"
    else
        printf 'hygon_xhci_resume_fix=FAIL device=%s quirks=0x%s reason=reset_on_resume_missing\n' "$bdf" "$hex"
        failed=1
    fi
done

if [[ "$found" == 0 ]]; then
    echo 'hygon_xhci_resume_fix=NOT_APPLICABLE reason=pci_1d94_148c_absent'
    exit 0
fi
(( failed == 0 )) || exit 1
(( unverified == 0 )) || exit 3
