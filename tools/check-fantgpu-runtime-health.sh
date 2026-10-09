#!/usr/bin/env bash
# Check the F display prerequisite before any R item is executed.
# This is read-only. It does not load modules, touch DRM state, or run Xorg.

set -euo pipefail

usage() {
    echo "Usage: $0 --dmesg FILE --status-file FILE [--sysfs-root DIR] [--dev-root DIR]" >&2
    exit 2
}

DMESG=
STATUS=
SYSFS_ROOT=/sys
DEV_ROOT=/dev
while [[ $# -gt 0 ]]; do
    case "$1" in
        --dmesg) [[ $# -ge 2 ]] || usage; DMESG=$2; shift 2 ;;
        --status-file) [[ $# -ge 2 ]] || usage; STATUS=$2; shift 2 ;;
        --sysfs-root) [[ $# -ge 2 ]] || usage; SYSFS_ROOT=$2; shift 2 ;;
        --dev-root) [[ $# -ge 2 ]] || usage; DEV_ROOT=$2; shift 2 ;;
        *) usage ;;
    esac
done
[[ -n "$DMESG" && -n "$STATUS" ]] || usage

failures=()
if [[ ! -s "$DMESG" ]]; then
    echo "fantgpu_runtime_health=UNVERIFIED reason=dmesg_unavailable"
    exit 3
fi
if [[ ! -f "$STATUS" ]]; then
    failures+=("status_unavailable")
elif ! grep -Eq '^Driver Status:[[:space:]]+OK([[:space:]]|$)' "$STATUS" ||
     ! grep -Eq '^Firmware Status:[[:space:]]+OK([[:space:]]|$)' "$STATUS"; then
    failures+=("driver_or_firmware_status_not_ok")
fi

shopt -s nullglob
sysfs_cards=("$SYSFS_ROOT"/class/drm/card[0-9]*)
dev_cards=("$DEV_ROOT"/dri/card[0-9]*)
shopt -u nullglob
(( ${#sysfs_cards[@]} > 0 )) || failures+=("drm_sysfs_card_missing")
(( ${#dev_cards[@]} > 0 )) || failures+=("drm_dev_card_missing")

# A successful compute-side status is not sufficient: reject the exact boot
# failures that previously reached the R-item stage or crashed in component bind.
# hwinfo_g0m.bin is an optional board-info input handled by the F-only audio
# fallback. Other firmware failures remain a hard stop.
firmware_failures=$(grep -Ei 'firmware:[[:space:]]+(failed to load|direct-loading failed)|request_firmware[^[:cntrl:]]*failed' "$DMESG" || true)
if [[ -n "$firmware_failures" ]] &&
   grep -Eiv 'hwinfo_g0m\.bin' <<<"$firmware_failures" | grep -q .; then
    failures+=("firmware_request_failed")
fi
if grep -Eiq '(^|[^[:alnum:]_])(Oops|BUG:|NULL pointer dereference|general protection fault|kernel panic|Unable to handle kernel)([^[:alnum:]_]|$)' "$DMESG"; then
    failures+=("kernel_fault")
fi

if (( ${#failures[@]} > 0 )); then
    printf 'fantgpu_runtime_health=FAIL reason=%s\n' "$(IFS=,; echo "${failures[*]}")"
    exit 1
fi

if [[ -n "$firmware_failures" ]]; then
    echo "fantgpu_runtime_health=PASS drm_sysfs_card=yes drm_dev_card=yes firmware_requests=required_clean optional_hwinfo_g0m=missing_allowed kernel_faults=clean"
else
    echo "fantgpu_runtime_health=PASS drm_sysfs_card=yes drm_dev_card=yes firmware_requests=clean optional_hwinfo_g0m=not_reported kernel_faults=clean"
fi
