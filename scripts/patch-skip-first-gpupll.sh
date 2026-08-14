#!/bin/bash
# Apply the Fantasy II-M runtime workaround used on Debian Trixie 6.12.x,
# where innogpu can Oops during:
#   g0m_soc_hw_init -> g0m_soc_setpll -> set_pll_reg
#
# The proprietary driver ships most low-level G0M code in innogpu.o_shipped,
# so the practical fix is to patch the shipped object before DKMS builds, or
# patch the installed innogpu.ko after DKMS builds.
#
# Usage:
#   sudo innogpu-skip-first-gpupll [kernel-version]
#   sudo innogpu-skip-first-gpupll --source [/usr/src/innogpu-kernel-2.2]
#   sudo innogpu-skip-first-gpupll --file /path/to/innogpu.ko-or-innogpu.o_shipped

set -euo pipefail

ROOT="${INNOGPU_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"

patch_file() {
    local target="$1"
    local backup_suffix="${2:-pre-skip-first-gpupll}"

    if [[ ! -f "$target" ]]; then
        echo "ERROR: target not found: $target" >&2
        exit 1
    fi

    local backup="${target}.${backup_suffix}"
    if [[ ! -f "$backup" ]]; then
        cp -a "$target" "$backup"
        echo "Backup written: $backup"
    else
        echo "Backup already exists: $backup"
    fi

    python3 "$ROOT/tools/patch-gpupll-object.py" "$target"
}

normalize_dkms_module() {
    local kver="$1"
    local base_dir="/lib/modules/${kver}/kernel/drivers/gpu/drm/innogpu"
    local ko="${base_dir}/innogpu.ko"

    if [[ ! -f "$ko" ]]; then
        local dkms_xz="/lib/modules/${kver}/updates/dkms/innogpu.ko.xz"
        local dkms_zst="/lib/modules/${kver}/updates/dkms/innogpu.ko.zst"
        local dkms_ko="/lib/modules/${kver}/updates/dkms/innogpu.ko"
        mkdir -p "$base_dir"
        if [[ -f "$dkms_xz" ]]; then
            xz -dc "$dkms_xz" > "$ko"
            echo "Decompressed DKMS module: $dkms_xz -> $ko"
        elif [[ -f "$dkms_zst" ]]; then
            zstd -dc "$dkms_zst" > "$ko"
            echo "Decompressed DKMS module: $dkms_zst -> $ko"
        elif [[ -f "$dkms_ko" ]]; then
            cp -a "$dkms_ko" "$ko"
            echo "Copied DKMS module: $dkms_ko -> $ko"
        fi
    fi

    printf '%s\n' "$ko"
}

if [[ ${EUID:-$(id -u)} -ne 0 ]]; then
    echo "ERROR: run as root: sudo $0 [kernel-version|--source|--file]" >&2
    exit 1
fi

case "${1:-}" in
    --source)
        src="${2:-/usr/src/innogpu-kernel-2.2}"
        patch_file "${src}/innogpu/innogpu.o_shipped" "pre-skip-first-gpupll"
        echo "Source object patched. Rebuild DKMS after this step."
        ;;
    --file)
        [[ -n "${2:-}" ]] || { echo "ERROR: --file requires a path" >&2; exit 1; }
        patch_file "$2" "pre-skip-first-gpupll"
        ;;
    *)
        kver="${1:-$(uname -r)}"
        ko="$(normalize_dkms_module "$kver")"
        patch_file "$ko" "pre-skip-first-gpupll"
        depmod -a "$kver"
        echo "depmod complete for $kver"
        echo "Next: test manually before enabling boot autoload: sudo modprobe innogpu"
        ;;
esac
