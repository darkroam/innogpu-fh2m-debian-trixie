#!/bin/bash
# Verify hardware-GL persistence after a reboot. This is read-only and writes a
# timestamped report under baselines/.

set -euo pipefail

ROOT="${INNOGPU_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
REPORT="$ROOT/baselines/post-reboot-hwgl-$(date +%Y%m%d-%H%M%S).txt"

mkdir -p "$ROOT/baselines"

boot_time="$(uptime -s)"
boot_epoch="$(date -d "$boot_time" +%s)"
latest_desktop_result="$ROOT/baselines/latest-desktop-hwgl-result.txt"
latest_desktop_report="$(
    find "$ROOT/baselines" -maxdepth 1 -type f -name 'desktop-hwgl-*.txt' -printf '%T@ %p\n' 2>/dev/null |
        sort -nr |
        head -1 |
        cut -d' ' -f2-
)"

existing_post_boot_pass=0
if [[ -r "$latest_desktop_result" && -n "${latest_desktop_report:-}" && -r "$latest_desktop_report" ]]; then
    result_epoch="$(stat -c '%Y' "$latest_desktop_result" 2>/dev/null || echo 0)"
    report_epoch="$(stat -c '%Y' "$latest_desktop_report" 2>/dev/null || echo 0)"
    if [[ "$result_epoch" -ge "$boot_epoch" &&
          "$report_epoch" -ge "$boot_epoch" &&
          "$(cat "$latest_desktop_result")" == "PASS_DESKTOP_HWGL" ]] &&
       grep -q 'direct rendering: Yes' "$latest_desktop_report" &&
       grep -q 'Accelerated: yes' "$latest_desktop_report" &&
       grep -q 'OpenGL renderer string: Fantasy II-M' "$latest_desktop_report" &&
       grep -q 'DRI3' "$latest_desktop_report" &&
       grep -q 'GLX' "$latest_desktop_report" &&
       grep -q 'Present' "$latest_desktop_report"; then
        existing_post_boot_pass=1
    fi
fi

{
    echo "post_reboot_hwgl_check=START"
    echo "timestamp=$(date -Is)"
    echo "boot_time=$boot_time"
    echo
    "$ROOT/scripts/check-innogpu-progress.sh"
    echo
    echo "===== Existing Post-Reboot Desktop Evidence ====="
    echo "latest_desktop_result=${latest_desktop_result:-missing}"
    echo "latest_desktop_report=${latest_desktop_report:-missing}"
    echo "existing_post_boot_pass=$existing_post_boot_pass"
    if [[ -n "${latest_desktop_report:-}" && -r "$latest_desktop_report" ]]; then
        grep -E 'x_display_query=|desktop_xorg_active=|direct rendering|Accelerated|OpenGL vendor|OpenGL renderer|DRI3|GLX|Present|Provider 0|AIGLX|GLX: Initialized' "$latest_desktop_report" | tail -120 || true
    fi
    echo
    echo "===== Desktop Hardware GL Validation ====="
    if [[ "$existing_post_boot_pass" == "1" ]]; then
        echo "SKIPPED_LIVE_DESKTOP_CHECK_EXISTING_POST_BOOT_PASS"
    else
        "$ROOT/scripts/check-desktop-hwgl.sh" || true
    fi
} | tee "$REPORT"

echo
echo "post_reboot_report=$REPORT"

if [[ "$existing_post_boot_pass" == "1" ]] || grep -qx 'RESULT: PASS_DESKTOP_HWGL' "$REPORT"; then
    echo "RESULT: PASS_POST_REBOOT_HWGL"
    printf '%s\n' "PASS_POST_REBOOT_HWGL" > "$ROOT/baselines/latest-post-reboot-hwgl-result.txt"
    exit 0
fi

echo "RESULT: FAIL_POST_REBOOT_HWGL"
printf '%s\n' "FAIL_POST_REBOOT_HWGL" > "$ROOT/baselines/latest-post-reboot-hwgl-result.txt"
exit 1
